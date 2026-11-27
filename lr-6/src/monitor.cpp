#include "monitor.h"
#include "output.h"

#include <algorithm>
#include <limits>
#include <utility>

namespace monitor {
namespace {

Run run;
HWND window = nullptr;
HHOOK hook = nullptr;
UINT_PTR study_timer = 0;
UINT_PTR end_timer = 0;
UINT_PTR next_timer = 1;
std::uint64_t next_run = 0;
bool measuring = false;
bool cleanup_failed = false;
const wchar_t* counter_error = nullptr;
DWORD counter_error_code = 0;

LONGLONG now()
{
    LARGE_INTEGER time{};
    if (!QueryPerformanceCounter(&time)) {
        throw output::Error{L"QueryPerformanceCounter", GetLastError()};
    }
    return time.QuadPart;
}

LONGLONG deadline()
{
    return run.start + run.clock_hz * run.seconds;
}

LRESULT CALLBACK observe(int code, WPARAM action, LPARAM data)
{
    if (code == HC_ACTION && action == PM_REMOVE && measuring && !counter_error) {
        const auto& msg = *reinterpret_cast<const MSG*>(data);
        const bool own_timer = msg.message == WM_TIMER && msg.hwnd == window;
        if ((!own_timer || msg.wParam == study_timer) && msg.hwnd &&
            (msg.hwnd == window || IsChild(window, msg.hwnd))) {
            LARGE_INTEGER time{};
            if (!QueryPerformanceCounter(&time)) {
                counter_error_code = GetLastError();
                counter_error = L"QueryPerformanceCounter";
            } else if (msg.message >= message_types) {
                counter_error_code = ERROR_INVALID_DATA;
                counter_error = L"Код оконного сообщения";
            } else if (time.QuadPart >= run.start && time.QuadPart < deadline()) {
                const auto bucket = static_cast<std::size_t>(
                    (time.QuadPart - run.start) / run.clock_hz);
                auto& count = run.counts[bucket * message_types + msg.message];
                if (count == std::numeric_limits<std::uint32_t>::max()) {
                    counter_error_code = ERROR_ARITHMETIC_OVERFLOW;
                    counter_error = L"Счётчик сообщений";
                } else {
                    ++count;
                    ++run.total;
                    if (own_timer) {
                        ++run.timer_counts[bucket];
                        ++run.timer_total;
                    }
                }
            }
        }
    }
    return CallNextHookEx(nullptr, code, action, data);
}

void release()
{
    output::Error error{nullptr, 0};
    if (hook) {
        if (!UnhookWindowsHookEx(hook)) {
            error = {L"UnhookWindowsHookEx", GetLastError()};
        } else {
            hook = nullptr;
        }
    }
    for (auto* timer : {&study_timer, &end_timer}) {
        if (*timer) {
            if (!KillTimer(window, *timer)) {
                const DWORD code = GetLastError();
                if (!error.operation) {
                    error = {L"KillTimer", code};
                }
            } else {
                *timer = 0;
            }
        }
    }
    if (error.operation) {
        cleanup_failed = true;
        throw error;
    }
}

UINT_PTR make_timer(unsigned ms)
{
    const UINT_PTR id = next_timer++;
    if (!SetTimer(window, id, ms, nullptr)) {
        throw output::Error{L"SetTimer", GetLastError()};
    }
    return id;
}

}

const Run& result() { return run; }
bool active() { return measuring; }
bool can_start() { return !measuring && !cleanup_failed; }

bool deadline_reached()
{
    return measuring && (counter_error || now() >= deadline());
}

void start(HWND owner, unsigned hz, unsigned seconds)
{
    Run prepared;
    LARGE_INTEGER frequency{};
    if (!QueryPerformanceFrequency(&frequency)) {
        throw output::Error{L"QueryPerformanceFrequency", GetLastError()};
    }
    prepared.clock_hz = frequency.QuadPart;
    prepared.hz = hz;
    prepared.seconds = seconds;
    prepared.counts.resize(static_cast<std::size_t>(seconds) * message_types);
    prepared.timer_counts.resize(seconds);
    FILETIME timestamp{};
    GetSystemTimeAsFileTime(&timestamp);
    const auto stamp = (static_cast<std::uint64_t>(timestamp.dwHighDateTime) << 32)
                       | timestamp.dwLowDateTime;
    next_run = std::max(next_run + 1, stamp);
    prepared.id = next_run;

    window = owner;
    counter_error = nullptr;
    try {
        hook = SetWindowsHookEx(WH_GETMESSAGE, observe, nullptr, GetCurrentThreadId());
        if (!hook) {
            throw output::Error{L"SetWindowsHookEx", GetLastError()};
        }
        study_timer = make_timer(1000 / hz);
        prepared.start = now();
        end_timer = make_timer(seconds * 1000);
        run = std::move(prepared);
        measuring = true;
    } catch (...) {
        release();
        throw;
    }
}

void stop()
{
    const bool was_active = measuring;
    measuring = false;
    LONGLONG time = 0;
    try {
        if (was_active && !counter_error) {
            time = now();
        }
    } catch (...) {
        release();
        throw;
    }
    release();
    if (!was_active) {
        return;
    }
    if (counter_error) {
        throw output::Error{counter_error, counter_error_code};
    }
    run.end = std::min(time, deadline());
    run.completed = time >= deadline();
    run.valid = run.end > run.start;
}

void post_test()
{
    for (int i = 0; i < 10; ++i) {
        if (!PostMessage(window, test_message, 0, 0)) {
            throw output::Error{L"PostMessage", GetLastError()};
        }
        ++run.posted;
    }
}

}
