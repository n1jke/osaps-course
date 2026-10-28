#pragma once

#include <cstdint>
#include <string>

namespace cli {

struct Options {
    std::uint32_t operations = 100;
    std::uint32_t read_percent = 50;
    std::uint32_t read_ms = 0;
    std::uint32_t seed = 11864;
    std::string experiment;
    std::string results;
    bool quiet = false;
};

Options parse(int argc, char** argv);

} // namespace cli
