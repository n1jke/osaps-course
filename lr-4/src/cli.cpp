#include "cli.h"

#include <limits>
#include <stdexcept>
#include <string>

namespace cli {
namespace {

std::uint32_t number(const std::string& text) {
    if (text.empty()) throw std::invalid_argument("expected a nonnegative integer");
    std::uint32_t value = 0;
    for (char c : text) {
        if (c < '0' || c > '9') throw std::invalid_argument("expected decimal digits");
        const auto digit = static_cast<std::uint32_t>(c - '0');
        if (value > (std::numeric_limits<std::uint32_t>::max() - digit) / 10) {
            throw std::invalid_argument("integer overflow");
        }
        value = value * 10 + digit;
    }
    return value;
}

} // namespace

Options parse(int argc, char** argv) {
    Options options;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--quiet") {
            options.quiet = true;
            continue;
        }
        if (i + 1 >= argc) throw std::invalid_argument("missing value for " + arg);
        const std::string value = argv[++i];
        if (arg == "--operations") options.operations = number(value);
        else if (arg == "--read-percent") options.read_percent = number(value);
        else if (arg == "--read-ms") options.read_ms = number(value);
        else if (arg == "--seed") options.seed = number(value);
        else if (arg == "--experiment") options.experiment = value;
        else if (arg == "--results") options.results = value;
        else throw std::invalid_argument("unknown argument: " + arg);
    }
    if (options.operations == 0 || options.operations > 1000000 ||
        options.read_percent > 100 || options.read_ms > 10000) {
        throw std::invalid_argument("operations must be 1..1000000, read-percent 0..100, read-ms 0..10000");
    }
    if (options.experiment.empty() || options.results.empty() ||
        options.experiment.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-") != std::string::npos) {
        throw std::invalid_argument("provide --experiment (letters, digits, _ or -) and --results");
    }
    return options;
}

} // namespace cli
