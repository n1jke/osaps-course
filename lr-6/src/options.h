#pragma once

#include <string>
#include <string_view>
#include <vector>

struct Options {
    unsigned hz = 10;
    unsigned seconds = 30;
    std::wstring csv;
};

inline constexpr wchar_t usage[] =
    L"lr6.exe [--hz 10|50|100] [--seconds 1..300] [--csv PATH]";

bool parse_options(const std::vector<std::wstring_view>& arguments, Options& result);
