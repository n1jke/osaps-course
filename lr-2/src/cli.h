#pragma once

#include "codec.h"

#include <cstdint>
#include <string>

namespace cli {

enum class Command {
    Help,
    Create,
    Add,
    List,
    Find,
    Generate
};

struct Arguments {
    Command command = Command::Help;
    std::string path;
    std::string text;
    std::string id;
    std::uint64_t count = 0;
    std::uint32_t max_length = 0;
};

Arguments parse(int argc, char* argv[]);

std::string help();

} // namespace cli
