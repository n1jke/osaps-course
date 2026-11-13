#include "http.h"
#include "results.h"
#include "socket_io.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <fstream>
#include <iostream>
#include <locale>
#include <stdexcept>

namespace {

constexpr unsigned student_id = 112;

struct Config {
    http::Method method = http::Method::get;
    std::string path;
    std::string host = "127.0.0.1";
    unsigned port = 20000 + student_id;
    unsigned timeout_ms = 3000;
    unsigned count = 1;
    std::string csv;
};

unsigned number(std::string_view value, std::string_view flag, unsigned min, unsigned max) {
    unsigned result = 0;
    const auto end = value.data() + value.size();
    const auto parsed = std::from_chars(value.data(), end, result);
    if (parsed.ec != std::errc{} || parsed.ptr != end || result < min || result > max) {
        throw std::invalid_argument(std::string(flag) + " requires an integer in "
            + std::to_string(min) + ".." + std::to_string(max));
    }
    return result;
}

Config parse(int argc, char** argv) {
    if (argc < 3) {
        throw std::invalid_argument("method and path are required");
    }
    Config config;
    const std::string_view method(argv[1]);
    if (method == "HEAD") {
        config.method = http::Method::head;
    } else if (method != "GET") {
        throw std::invalid_argument("method must be GET or HEAD");
    }
    config.path = argv[2];
    if (config.path.empty() || config.path.front() != '/' || config.path.size() > 2048
        || !std::all_of(config.path.begin(), config.path.end(), [](unsigned char c) {
            return c >= '!' && c <= '~' && c != '#';
        })) {
        throw std::invalid_argument("path must start with / and use at most 2048 printable ASCII bytes without spaces or #");
    }

    constexpr std::array<std::string_view, 5> options{{"--host", "--port", "--timeout-ms", "--count", "--csv"}};
    std::array<bool, options.size()> seen{};
    for (int i = 3; i < argc; ++i) {
        const std::string_view flag(argv[i]);
        const auto option = std::find(options.begin(), options.end(), flag);
        if (option == options.end()) {
            throw std::invalid_argument("unknown option: " + std::string(flag));
        }
        const auto index = static_cast<std::size_t>(option - options.begin());
        if (seen[index]) {
            throw std::invalid_argument("repeated option: " + std::string(flag));
        }
        seen[index] = true;
        if (++i == argc || argv[i][0] == '\0'
            || std::string_view(argv[i]).substr(0, 2) == "--") {
            throw std::invalid_argument("missing value for " + std::string(flag));
        }
        const std::string_view value(argv[i]);
        switch (index) {
        case 0: config.host = value; break;
        case 1: config.port = number(value, flag, 20000, 29999); break;
        case 2: config.timeout_ms = number(value, flag, 1, 60000); break;
        case 3: config.count = number(value, flag, 1, 10000); break;
        case 4: config.csv = value; break;
        }
    }
    return config;
}

results::Attempt exchange(const Config& config, const sockaddr_in& remote, std::string_view request,
    unsigned number, std::int64_t frequency, http::Response& response) {
    results::Attempt attempt;
    attempt.number = number;
    std::optional<socket_io::Socket> socket;
    std::array<char, 8192> buffer{};
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(config.timeout_ms);
    const auto start = results::ticks();
    std::optional<std::int64_t> connect_start;
    auto end = start;
    try {
        socket.emplace();
        attempt.stage = "connect";
        connect_start = results::ticks();
        socket->connect(remote, deadline);
        attempt.connect_ms = results::milliseconds(*connect_start, results::ticks(), frequency);
        attempt.stage = "send";
        socket->send_all(request, deadline, attempt.sent);
        while (!response.complete()) {
            attempt.stage = "receive";
            const int count = socket->receive(buffer.data(), static_cast<int>(buffer.size()), deadline);
            attempt.received += static_cast<std::size_t>(count);
            attempt.stage = "http";
            if (count == 0) {
                response.finish();
            } else {
                response.consume({buffer.data(), static_cast<std::size_t>(count)});
            }
        }
        end = results::ticks();
        attempt.outcome = results::Outcome::ok;
        attempt.stage = {};
    } catch (const socket_io::Timeout& error) {
        end = results::ticks();
        attempt.outcome = results::Outcome::timeout;
        attempt.error = error.what();
    } catch (const std::runtime_error& error) {
        end = results::ticks();
        attempt.error = error.what();
    }

    if (connect_start && !attempt.connect_ms) {
        attempt.connect_ms = results::milliseconds(*connect_start, end, frequency);
    }
    attempt.attempt_ms = results::milliseconds(start, end, frequency);
    attempt.status = response.status();
    attempt.body = response.body_size();
    attempt.request_complete = attempt.sent == request.size();
    attempt.response_complete = response.complete();
    return attempt;
}

int run(const Config& config) {
    const socket_io::Winsock winsock;
    const auto remote = socket_io::address(config.host.c_str(), config.port);
    const auto request = http::request(config.method, config.path, config.host, config.port);
    const auto frequency = results::frequency();
    const auto run_id = results::make_run_id();
    const auto peer = config.host + ":" + std::to_string(config.port);

    std::ofstream csv;
    if (!config.csv.empty()) {
        csv.exceptions(std::ios::failbit | std::ios::badbit);
        csv.imbue(std::locale::classic());
        csv.open(config.csv);
        results::csv_header(csv);
    }

    const bool is_get = config.method == http::Method::get;
    results::Totals totals;
    for (unsigned number = 1; number <= config.count; ++number) {
        http::Response response(config.method, is_get && config.count == 1);
        results::log(run_id, peer, number, std::string(http::name(config.method)) + " " + config.path);
        const auto attempt = exchange(config, remote, request, number, frequency, response);
        totals.add(attempt);
        if (attempt.outcome == results::Outcome::ok) {
            results::log(run_id, peer, number, "response complete status=" + std::to_string(*attempt.status));
            auto headers = response.headers();
            headers.erase(std::remove(headers.begin(), headers.end(), '\r'), headers.end());
            std::cout << headers << "body_bytes=" << attempt.body << '\n';
            if (!response.preview().empty()) {
                std::cout << "body_preview=" << http::escape(response.preview()) << '\n';
            }
        } else {
            const auto prefix = attempt.outcome == results::Outcome::timeout ? "timeout " : "error ";
            results::log(run_id, peer, number, prefix + std::string(attempt.stage) + ": " + attempt.error);
        }
        if (csv.is_open()) {
            results::csv_row(csv, run_id, config.host, config.port, http::name(config.method), config.path, attempt);
        }
    }
    if (csv.is_open()) {
        csv.close();
    }
    results::summary(std::cout, totals, config.count, is_get);
    return totals.successful == config.count ? 0 : 1;
}

}

int main(int argc, char** argv) {
    try {
        return run(parse(argc, argv));
    } catch (const std::invalid_argument& error) {
        std::cerr << "argument error: " << error.what() << '\n'
                  << "usage: http_client GET|HEAD <path> [--host <ipv4>] [--port <port>]"
                  << " [--timeout-ms <ms>] [--count <n>] [--csv <path>]\n";
        return 2;
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << '\n';
        return 1;
    }
}
