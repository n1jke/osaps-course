#pragma once

#include <windows.h>

#include <cstddef>
#include <cstdint>

namespace shared {

constexpr std::size_t record_count = 100;
constexpr DWORD wait_ms = 30000;

struct Record {
    std::uint32_t value;
    std::uint32_t check;
};

struct Data {
    Record records[record_count];
    ULONGLONG start_ms;
    LONG readers;
    LONG writers;
    volatile LONG active_readers;
    volatile LONG active_writers;
    volatile LONG violations;
};

constexpr std::uint32_t check_value(std::uint32_t value) {
    return ~value;
}

} // namespace shared
