#include "cli.h"
#include "protocol.h"
#include <charconv>
#include <iostream>
#include <set>
#include <stdexcept>

namespace cli {
uint64_t number64(const std::string& text) {
    uint64_t value = 0;
    const char* begin = text.data();
    const char* end = begin + text.size();
    const auto result = std::from_chars(begin, end, value);
    // Require a valid number, the entire argument consumed, and a nonzero value.
    if (result.ec != std::errc{} || result.ptr != end || value == 0) {
        throw std::invalid_argument("Expected a positive integer: " + text);
    }
    return value;
}
uint32_t number(const std::string& text) {
    const uint64_t value = number64(text);
    if (value > UINT32_MAX) throw std::invalid_argument("Number exceeds uint32: " + text);
    return static_cast<uint32_t>(value);
}
void help() {
    std::cout << "lr3 server --run-id N --log PATH [--timeout-ms T]\n"
                 "lr3 client --run-id N --client-id 1..3 [--size 64|4096]\n"
                 "           [--start-at-ms UNIX_MS] (up to 60 seconds ahead)\n"
                 "           [--fault type|size] (manual error checks only)\n";
}
Options parse(int argc, char** argv) {
    if (argc < 2 || (std::string(argv[1]) != "server" && std::string(argv[1]) != "client")) {
        throw std::invalid_argument("First argument must be server or client");
    }
    Options o;
    o.server = std::string(argv[1]) == "server";
    std::set<std::string> seen;
    for (int i = 2; i < argc; i += 2) {
        const std::string key = argv[i];
        if (i + 1 >= argc || !seen.insert(key).second) {
            throw std::invalid_argument("Missing value or duplicate option: " + key);
        }
        const std::string value = argv[i + 1];
        if (key == "--run-id") o.run = number(value);
        else if (key == "--log" && o.server && !value.empty()) o.log = value;
        else if (key == "--timeout-ms" && o.server) o.timeout = number(value);
        else if (key == "--client-id" && !o.server) o.client = number(value);
        else if (key == "--size" && !o.server) o.size = number(value);
        else if (key == "--start-at-ms" && !o.server) o.start_at = number64(value);
        else if (key == "--fault" && !o.server) o.fault = value;
        else throw std::invalid_argument("Unknown option for this role: " + key);
    }
    if (!o.run || (o.server && o.log.empty()) || (!o.server && (o.client < 1 || o.client > 3))) {
        throw std::invalid_argument("Required arguments missing or client-id outside 1..3");
    }
    if (o.size <= protocol::HEADER_SIZE || o.size > protocol::MAX_SIZE || o.timeout == UINT32_MAX) {
        throw std::invalid_argument("Size must be 37..4096; timeout must be 1..4294967294 ms");
    }
    if (!o.fault.empty() && o.fault != "type" && o.fault != "size") {
        throw std::invalid_argument("Fault must be type or size");
    }
    return o;
}
}
