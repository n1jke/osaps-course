#pragma once

#include <windows.h>

#include <cstdint>
#include <vector>

namespace monitor {

constexpr UINT test_message = WM_APP + 1;
constexpr unsigned message_types = 65536;
constexpr unsigned max_seconds = 300;

struct Run {
    std::uint64_t id = 0;
    unsigned hz = 0;
    unsigned seconds = 0;
    LONGLONG clock_hz = 0;
    LONGLONG start = 0;
    LONGLONG end = 0;
    std::uint64_t total = 0;
    std::uint64_t timer_total = 0;
    std::uint64_t posted = 0;
    bool completed = false;
    bool valid = false;
    std::vector<std::uint32_t> counts;
    std::vector<std::uint32_t> timer_counts;
};

const Run& result();
bool active();
bool can_start();
bool deadline_reached();
void start(HWND window, unsigned hz, unsigned seconds);
void stop();
void post_test();

}
