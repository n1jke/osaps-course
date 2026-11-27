#include "output.h"
#include "monitor.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <locale>
#include <sstream>

namespace output {

std::wstring describe(const Error& error)
{
    wchar_t message[512]{};
    FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                   nullptr, error.code, 0, message, 512, nullptr);
    return std::wstring(error.operation) + L" (" + std::to_wstring(error.code)
           + L") " + (message[0] ? message : L"Описание недоступно");
}

void log(const wchar_t* operation, const std::wstring& object,
         const wchar_t* status, DWORD code, const SYSTEMTIME* timestamp)
{
    SYSTEMTIME time{};
    if (timestamp) {
        time = *timestamp;
    } else {
        GetLocalTime(&time);
    }
    std::wostringstream line;
    line << std::setfill(L'0') << time.wYear << L'-' << std::setw(2) << time.wMonth
         << L'-' << std::setw(2) << time.wDay << L' ' << std::setw(2) << time.wHour
         << L':' << std::setw(2) << time.wMinute << L':' << std::setw(2) << time.wSecond
         << L'.' << std::setw(3) << time.wMilliseconds << L' ' << operation
         << L" object=" << object << L" status=" << status;
    if (code) {
        line << L" code=" << code;
    }
    line << L'\n';
    const auto wide = line.str();
    const int length = static_cast<int>(wide.size());
    const int bytes = WideCharToMultiByte(CP_UTF8, 0, wide.data(), length,
                                          nullptr, 0, nullptr, nullptr);
    if (!bytes) {
        throw Error{L"WideCharToMultiByte", GetLastError()};
    }
    std::string utf8(bytes, '\0');
    if (!WideCharToMultiByte(CP_UTF8, 0, wide.data(), length,
                             utf8.data(), bytes, nullptr, nullptr)) {
        throw Error{L"WideCharToMultiByte", GetLastError()};
    }
    std::ofstream file("lr6.log", std::ios::app);
    file.write(utf8.data(), bytes);
    file.close();
    if (!file) {
        throw Error{L"Запись lr6.log", ERROR_WRITE_FAULT};
    }
}

static void write_csv(const std::filesystem::path& path, const monitor::Run& run)
{
    std::ofstream file{path};
    if (!file) {
        throw Error{L"Открытие CSV", ERROR_OPEN_FAILED};
    }
    file.imbue(std::locale::classic());
    file << std::fixed << std::setprecision(6)
         << "run_id,target_hz,period_ms,requested_ms,observed_ms,status,"
            "interval_start_ms,interval_end_ms,message_id,count,study_timer_count\n";
    const auto elapsed = run.end - run.start;
    const auto ms_per_tick = 1000.0 / static_cast<double>(run.clock_hz);
    for (unsigned bucket = 0; run.clock_hz * bucket < elapsed; ++bucket) {
        const auto end = std::min(elapsed, run.clock_hz * (bucket + 1));
        const auto prefix = [&] {
            file << run.id << ',' << run.hz << ',' << 1000 / run.hz << ','
                 << run.seconds * 1000 << ',' << elapsed * ms_per_tick << ','
                 << (run.completed ? "completed" : "stopped") << ','
                 << bucket * 1000 << ',' << end * ms_per_tick << ',';
        };
        bool empty = true;
        for (unsigned type = 0; type < monitor::message_types; ++type) {
            const auto count = run.counts[static_cast<std::size_t>(bucket) *
                                          monitor::message_types + type];
            if (count) {
                prefix();
                file << type << ',' << count << ','
                     << (type == WM_TIMER ? run.timer_counts[bucket] : 0) << '\n';
                empty = false;
            }
        }
        if (empty) {
            prefix();
            file << ",0,0\n";
        }
    }
    file.close();
    if (!file) {
        throw Error{L"Запись CSV", ERROR_WRITE_FAULT};
    }
}

void save_csv(const std::wstring& path, const monitor::Run& run)
{
    const std::filesystem::path destination(path);
    auto directory = destination.parent_path();
    if (directory.empty()) {
        directory = L".";
    }
    wchar_t temporary[MAX_PATH]{};
    if (!GetTempFileNameW(directory.c_str(), L"lr6", 0, temporary)) {
        throw Error{L"GetTempFileName", GetLastError()};
    }
    try {
        write_csv(temporary, run);
        if (!MoveFileExW(temporary, path.c_str(), 0)) {
            throw Error{L"MoveFileEx", GetLastError()};
        }
    } catch (...) {
        DeleteFileW(temporary);
        throw;
    }
}

}
