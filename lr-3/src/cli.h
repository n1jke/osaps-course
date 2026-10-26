#pragma once
#include <cstdint>
#include <string>

namespace cli {
struct Options {
    bool server = false;
    uint32_t run = 0;
    uint32_t client = 0;
    uint32_t size = 64;
    uint32_t timeout = 30000;
    uint64_t start_at = 0;
    std::string log;
    std::string fault;
};
Options parse(int argc, char** argv);
void help();
}
