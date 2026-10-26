#include "cli.h"
#include "config.h"
#include "mailslot.h"
#include "protocol.h"
#include <array>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
#include <stdexcept>

namespace app {
using Clock = std::chrono::steady_clock;

void identify(const cli::Options& o) {
    std::cout << (o.server ? "SERVER" : "CLIENT") << " #" << o.client
              << " | PID=" << GetCurrentProcessId() << " | VARIANT=" << config::VARIANT_ID
              << " | STUDENT_ID=" << config::STUDENT_ID << " | RUN_ID=" << o.run << '\n';
}
void event(const char* action, const protocol::Message& m, size_t size) {
    std::cout << '[' << protocol::now_ms() << "] PID=" << GetCurrentProcessId()
              << " RUN_ID=" << m.run << ' ' << action << " requestId=" << m.request
              << " type=" << (m.type == protocol::LOG ? "LOG" : "DONE")
              << " size=" << size << " client=" << m.client << " sourcePid=" << m.pid << '\n';
}
std::string record(const protocol::Message& m) {
    std::ostringstream line;
    line << "received_at=" << protocol::now_ms() << "|RUN_ID=" << m.run
         << "|sourcePid=" << m.pid << "|clientId=" << m.client
         << "|requestId=" << m.request << "|send_at=" << m.sent_at
         << "|payloadSize=" << m.payload.size() << "|text=" << protocol::escape(m.payload) << "\r\n";
    return line.str();
}
int client(const cli::Options& o) {
    auto slot = mailslot::client(o.run);
    if (o.start_at) {
        uint64_t now = protocol::now_ms();
        if (o.start_at <= now || o.start_at - now > 60000) {
            throw std::runtime_error("Scheduled start must be in the future, within 60 seconds");
        }
        std::cout << "RUN_ID=" << o.run << " READY start_at_ms=" << o.start_at << std::endl;
        while (now < o.start_at) {
            const uint64_t remaining = o.start_at - now;
            Sleep(static_cast<DWORD>(remaining > 100 ? 100 : remaining));
            now = protocol::now_ms();
        }
    }
    protocol::Message m;
    m.pid = GetCurrentProcessId();
    m.run = o.run;
    m.client = o.client;
    m.payload.resize(o.size - protocol::HEADER_SIZE);
    std::mt19937 generator(config::STUDENT_ID + o.client);
    if (!o.fault.empty()) {
        m.sent_at = protocol::now_ms();
        auto bytes = protocol::encode(m);
        if (o.fault == "type") bytes[4] = 99;
        else bytes[16] ^= 1;
        mailslot::write(slot.get(), bytes);
        std::cout << "RUN_ID=" << o.run << " FAULT_SENT=" << o.fault << '\n';
    }
    for (uint32_t i = 1; i <= config::MESSAGE_COUNT; ++i) {
        for (auto& c : m.payload) c = static_cast<uint8_t>('a' + generator() % 26);
        m.request = i;
        m.sent_at = protocol::now_ms();
        mailslot::write(slot.get(), protocol::encode(m));
        event("SEND", m, o.size);
    }
    m.type = protocol::DONE;
    m.request = 0;
    m.sent_at = protocol::now_ms();
    m.payload.clear();
    protocol::append_u32(m.payload, config::MESSAGE_COUNT);
    const auto bytes = protocol::encode(m);
    mailslot::write(slot.get(), bytes);
    event("SEND", m, bytes.size());
    mailslot::close(slot);
    std::cout << "RUN_ID=" << o.run << " sent=" << config::MESSAGE_COUNT
              << " delivery_acknowledged=no\n";
    return 0;
}
struct ClientState {
    uint32_t pid = 0;
    uint32_t count = 0;
    uint32_t last = 0;
    uint32_t duplicates = 0;
    bool done = false;
    std::array<bool, config::MESSAGE_COUNT + 1> seen{};
};
void validate(const protocol::Message& m, const cli::Options& o, const ClientState& state) {
    if (m.run != o.run || m.client < 1 || m.client > 3 || !m.pid) {
        throw std::runtime_error("Wrong RUN_ID, clientId or PID");
    }
    if (state.pid && state.pid != m.pid) throw std::runtime_error("Client changed PID");
    if (state.done) throw std::runtime_error("Message after DONE or duplicate DONE");
    if (m.type == protocol::LOG) {
        if (m.request < 1 || m.request > config::MESSAGE_COUNT || m.payload.empty()) {
            throw std::runtime_error("Invalid LOG sequence number or empty text");
        }
        for (uint8_t c : m.payload) {
            if (c > 127) throw std::runtime_error("LOG text must be ASCII");
        }
    } else if (m.request != 0 || m.payload.size() != 4 ||
               protocol::read_u32(m.payload.data()) != config::MESSAGE_COUNT) {
        throw std::runtime_error("Invalid DONE count or layout");
    }
}
int server(const cli::Options& o) {
    auto slot = mailslot::server(o.run);
    auto journal = mailslot::journal(o.log);
    auto read_event = mailslot::checked(CreateEventA(nullptr, TRUE, FALSE, nullptr), "CreateEventA");
    std::array<ClientState, 3> clients{};
    std::vector<uint8_t> bytes;
    uint32_t completed = 0;
    uint32_t received = 0;
    uint32_t rejected = 0;
    bool errors = false;
    bool started = false;
    Clock::time_point begin{}, end{};
    auto progress = Clock::now();
    std::cout << "RUN_ID=" << o.run << " READY slot=" << mailslot::name(o.run)
              << " expected=" << 3 * config::MESSAGE_COUNT << " timeout_ms=" << o.timeout << std::endl;
    while (completed < 3) {
        const auto idle = std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - progress).count();
        if (idle >= o.timeout || !mailslot::read(slot.get(), read_event.get(), o.timeout - static_cast<DWORD>(idle), bytes)) {
            std::cerr << "RUN_ID=" << o.run << " TIMEOUT: no valid progress\n";
            errors = true;
            break;
        }
        // The first blocking wait is excluded; timing starts before decoding the first LOG.
        const auto read_at = Clock::now();
        protocol::Message m;
        try {
            m = protocol::decode(bytes);
            if (m.client < 1 || m.client > 3) throw std::runtime_error("clientId outside 1..3");
            validate(m, o, clients[m.client - 1]);
        } catch (const std::runtime_error& e) {
            ++rejected;
            errors = true;
            std::cerr << "RUN_ID=" << o.run << " REJECT size=" << bytes.size() << " " << e.what() << '\n';
            continue;
        }
        auto& state = clients[m.client - 1];
        if (m.type == protocol::LOG && state.seen[m.request]) {
            ++state.duplicates;
            ++rejected;
            errors = true;
            std::cerr << "RUN_ID=" << o.run << " DUPLICATE client=" << m.client
                      << " requestId=" << m.request << '\n';
            continue;
        }
        state.pid = m.pid;
        if (m.type == protocol::LOG) {
            if (!started) {
                begin = read_at;
                started = true;
            }
            if (m.request != state.last + 1) {
                errors = true;
                std::cerr << "RUN_ID=" << o.run << " SEQUENCE client=" << m.client
                          << " expected=" << state.last + 1 << " actual=" << m.request << '\n';
            }
            mailslot::write_line(journal.get(), record(m));
            state.seen[m.request] = true;
            state.last = m.request;
            ++state.count;
            ++received;
            event("RECEIVE", m, bytes.size());
            end = Clock::now();
        } else {
            state.done = true;
            ++completed;
            event("RECEIVE", m, bytes.size());
            if (state.count != config::MESSAGE_COUNT) errors = true;
        }
        progress = Clock::now();
    }
    for (size_t i = 0; i < clients.size(); ++i) {
        const auto& state = clients[i];
        std::cout << "RUN_ID=" << o.run << " client=" << i + 1 << " received=" << state.count
                  << " missing=" << config::MESSAGE_COUNT - state.count
                  << " duplicates=" << state.duplicates << " done=" << (state.done ? "yes" : "no") << '\n';
    }
    mailslot::close(journal);
    mailslot::close(read_event);
    mailslot::close(slot);
    const bool success = !errors && completed == 3 && received == 3 * config::MESSAGE_COUNT;
    std::cout << "RUN_ID=" << o.run << " status=" << (success ? "OK" : "INCOMPLETE_OR_INVALID")
              << " received=" << received << " rejected=" << rejected << '\n';
    if (success && started) {
        const double seconds = std::chrono::duration<double>(end - begin).count();
        std::cout << std::fixed << std::setprecision(6) << "RUN_ID=" << o.run
                  << " elapsed_s=" << seconds << " messages_per_s=" << (seconds > 0 ? received / seconds : 0)
                  << " scope=receive_validate_console_journal\n";
    } else {
        std::cout << "RUN_ID=" << o.run << " benchmark=invalid\n";
    }
    return success ? 0 : 1;
}
}
int main(int argc, char** argv) {
    if (argc == 2 && std::string(argv[1]) == "--help") {
        cli::help();
        return 0;
    }
    cli::Options options;
    try {
        options = cli::parse(argc, argv);
    } catch (const std::invalid_argument& e) {
        std::cerr << "Arguments: " << e.what() << '\n';
        cli::help();
        return 2;
    }
    try {
        app::identify(options);
        return options.server ? app::server(options) : app::client(options);
    } catch (const std::exception& e) {
        std::cerr << "RUN_ID=" << options.run << " ERROR: " << e.what() << '\n';
        return 1;
    }
}
