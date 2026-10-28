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

struct WorkerStats {
    std::uint32_t operations;
    LONGLONG wait_ticks;
    LONGLONG max_wait_ticks;
};

struct Data {
    Record records[record_count];
    ULONGLONG start_ms;
    std::uint32_t counts[4];
    std::uint32_t read_ms;
    std::uint32_t seed;
    bool quiet;
    WorkerStats stats[4];
    LONG readers;
    LONG writers;
    volatile LONG active_readers;
    volatile LONG active_writers;
    volatile LONG violations;
    volatile LONG timeouts;
};

constexpr std::uint32_t check_value(std::uint32_t value) {
    return ~value;
}

} // namespace shared
