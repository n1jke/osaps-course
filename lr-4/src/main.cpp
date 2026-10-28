#include "cli.h"
#include "shared.h"
#include "sync.h"
#include "table.h"

#include <windows.h>

#include <array>
#include <cstdint>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

namespace app {
constexpr DWORD wait_limit_ms = shared::wait_ms;


void report_error(const char* operation, DWORD error) {
    char* message = nullptr;
    FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                       FORMAT_MESSAGE_IGNORE_INSERTS,
                   nullptr, error, 0, reinterpret_cast<char*>(&message), 0, nullptr);
    std::cerr << operation << " failed (" << error << "): "
              << (message ? message : "unknown error") << '\n';
    if (message) {
        LocalFree(message);
    }
}

int worker(const char* name, const char* role, const char* number) {
    HANDLE mapping = OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, name);
    if (!mapping) {
        report_error("OpenFileMapping", GetLastError());
        return 1;
    }
    auto* data = static_cast<shared::Data*>(
        MapViewOfFile(mapping, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(shared::Data)));
    if (!data) {
        const DWORD error = GetLastError();
        CloseHandle(mapping);
        report_error("MapViewOfFile", error);
        return 1;
    }

    sync::Objects objects;
    if (!sync::open(objects, name)) {
        sync::close(objects);
        UnmapViewOfFile(data);
        CloseHandle(mapping);
        return 1;
    }

    const bool reader = std::string(role) == "reader";
    const int worker_index = (reader ? 0 : 2) + (std::string(number) == "1" ? 0 : 1);
    auto& stats = data->stats[worker_index];
    std::mt19937 random(data->seed + static_cast<std::uint32_t>(worker_index));
    std::uniform_int_distribution<std::size_t> records(0, shared::record_count - 1);
    bool ok = true;
    for (std::uint32_t i = 0; i < data->counts[worker_index] && ok; ++i) {
        if (!data->quiet) std::cout << '[' << GetTickCount64() - data->start_ms << " ms] "
                  << role << ' ' << number << " PID=" << GetCurrentProcessId()
                  << " STATE=WAITING" << std::endl;
        LARGE_INTEGER start{}, end{};
        if (!QueryPerformanceCounter(&start)) {
            report_error("QueryPerformanceCounter", GetLastError());
            ok = false;
            break;
        }
        if (!(reader ? sync::enter_read(objects, *data) : sync::enter_write(objects, *data))) {
            ok = false;
            break;
        }
        if (!QueryPerformanceCounter(&end)) {
            report_error("QueryPerformanceCounter", GetLastError());
            if (reader) sync::leave_read(objects, *data);
            else sync::leave_write(objects, *data);
            ok = false;
            break;
        }
        const LONGLONG elapsed = end.QuadPart - start.QuadPart;
        stats.wait_ticks += elapsed;
        if (elapsed > stats.max_wait_ticks) stats.max_wait_ticks = elapsed;
        if (!data->quiet) std::cout << '[' << GetTickCount64() - data->start_ms << " ms] "
                  << role << ' ' << number << " PID=" << GetCurrentProcessId()
                  << " STATE=" << (reader ? "READING" : "WRITING") << std::endl;
        if (reader) {
            table::read(*data, data->read_ms);
            ok = sync::leave_read(objects, *data);
        } else {
            table::write(*data, records(random));
            ok = sync::leave_write(objects, *data);
        }
        if (ok) ++stats.operations;
    }
    if (!data->quiet) std::cout << '[' << GetTickCount64() - data->start_ms << " ms] "
              << role << ' ' << number << " PID=" << GetCurrentProcessId()
              << " STATE=" << (ok ? "FINISHED" : "FAILED") << std::endl;
    sync::close(objects);
    const BOOL unmapped = UnmapViewOfFile(data);
    const DWORD unmap_error = unmapped ? ERROR_SUCCESS : GetLastError();
    CloseHandle(mapping);
    if (!unmapped) {
        report_error("UnmapViewOfFile", unmap_error);
        return 1;
    }
    return ok ? 0 : 1;
}

int coordinator(const cli::Options& options) {
    const std::string name = "Local\\lab4_mapping_" + std::to_string(GetCurrentProcessId());
    HANDLE mapping = CreateFileMappingA(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE,
                                        0, sizeof(shared::Data), name.c_str());
    if (!mapping) {
        report_error("CreateFileMapping", GetLastError());
        return 1;
    }
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        std::cerr << "mapping name already exists\n";
        CloseHandle(mapping);
        return 1;
    }
    auto* data = static_cast<shared::Data*>(
        MapViewOfFile(mapping, FILE_MAP_WRITE, 0, 0, sizeof(shared::Data)));
    if (!data) {
        const DWORD error = GetLastError();
        CloseHandle(mapping);
        report_error("MapViewOfFile", error);
        return 1;
    }
    *data = {};
    data->start_ms = GetTickCount64();
    const auto reads = static_cast<std::uint32_t>(
        static_cast<std::uint64_t>(options.operations) * options.read_percent / 100);
    const auto writes = options.operations - reads;
    data->counts[0] = (reads + 1) / 2;
    data->counts[1] = reads / 2;
    data->counts[2] = (writes + 1) / 2;
    data->counts[3] = writes / 2;
    data->read_ms = options.read_ms;
    data->seed = options.seed;
    data->quiet = options.quiet;
    for (auto& record : data->records) {
        record.check = shared::check_value(record.value);
    }
    sync::Objects objects;
    if (!sync::create(objects, name)) {
        sync::close(objects);
        UnmapViewOfFile(data);
        CloseHandle(mapping);
        return 1;
    }

    std::array<char, 32768> executable{};
    DWORD length = GetModuleFileNameA(nullptr, executable.data(), static_cast<DWORD>(executable.size()));
    bool ok = length != 0 && length < executable.size();

    if (!ok) {
        if (length == 0) {
            report_error("GetModuleFileName", GetLastError());
        } else {
            std::cerr << "executable path is too long\n";
        }
    }

    LARGE_INTEGER frequency{}, experiment_start{}, experiment_end{};
    if (!QueryPerformanceFrequency(&frequency) || !QueryPerformanceCounter(&experiment_start)) {
        report_error("performance counter", GetLastError());
        sync::close(objects);
        UnmapViewOfFile(data);
        CloseHandle(mapping);
        return 1;
    }
    std::vector<HANDLE> processes;
    const char* roles[] = {"reader", "reader", "writer", "writer"};
    for (int i = 0; ok && i < 4; ++i) {
        std::string command = "\"" + std::string(executable.data(), length) + "\" --worker " +
                              name + " " + roles[i] + " " + std::to_string(i % 2 + 1);
        std::vector<char> mutable_command(command.begin(), command.end());
        mutable_command.push_back('\0');
        STARTUPINFOA startup{};
        startup.cb = sizeof(startup);
        PROCESS_INFORMATION process{};
        if (!CreateProcessA(executable.data(), mutable_command.data(), nullptr, nullptr, FALSE,
                            0, nullptr, nullptr, &startup, &process)) {
            report_error("CreateProcess", GetLastError());
            ok = false;
            break;
        }
        CloseHandle(process.hThread);
        processes.push_back(process.hProcess);
    }
    if (ok) {
        const DWORD result = WaitForMultipleObjects(static_cast<DWORD>(processes.size()),
                                                   processes.data(), TRUE, wait_limit_ms);

        if (result == WAIT_TIMEOUT) {
            InterlockedIncrement(&data->timeouts);
            std::cerr << "experiment timed out\n";
            ok = false;
        } else if (result == WAIT_FAILED) {
            report_error("WaitForMultipleObjects", GetLastError());
            ok = false;
        }
    }

    if (!ok) {
        // This preliminary stage has no cooperative cancellation protocol.
        for (HANDLE process : processes) {
            if (WaitForSingleObject(process, 0) == WAIT_TIMEOUT &&
                !TerminateProcess(process, 1)) {
                report_error("TerminateProcess", GetLastError());
            }
        }
    }

    for (HANDLE process : processes) {
        const DWORD result = WaitForSingleObject(process, ok ? 0 : 1000);

        if (result == WAIT_OBJECT_0) {
            DWORD code = 0;

            if (!GetExitCodeProcess(process, &code)) {
                report_error("GetExitCodeProcess", GetLastError());
                ok = false;
            } else if (code != 0) {
                std::cerr << "worker exited with code " << code << '\n';
                ok = false;
            }
        } else if (result == WAIT_FAILED) {
            report_error("WaitForSingleObject", GetLastError());
            ok = false;
        } else {
            std::cerr << "worker did not exit after cancellation\n";
            ok = false;
        }

        CloseHandle(process);
    }
    if (!QueryPerformanceCounter(&experiment_end)) {
        report_error("QueryPerformanceCounter", GetLastError());
        ok = false;
        experiment_end = experiment_start;
    }
    const LONG violations = InterlockedCompareExchange(&data->violations, 0, 0);
    const LONG max_readers = InterlockedCompareExchange(&data->max_active_readers, 0, 0);
    std::cout << "invariant_violations=" << violations << std::endl;
    std::cout << "max_active_readers=" << max_readers << std::endl;
    if (violations != 0) {
        ok = false;
    }
    const double to_ms = 1000.0 / static_cast<double>(frequency.QuadPart);
    const auto r1 = data->stats[0];
    const auto r2 = data->stats[1];
    const auto w1 = data->stats[2];
    const auto w2 = data->stats[3];
    const LONG timeouts = InterlockedCompareExchange(&data->timeouts, 0, 0);
    sync::close(objects);
    if (!UnmapViewOfFile(data)) {
        report_error("UnmapViewOfFile", GetLastError());
        ok = false;
    }
    if (!CloseHandle(mapping)) {
        report_error("CloseHandle(mapping)", GetLastError());
        ok = false;
    }
    const auto reader_count = r1.operations + r2.operations;
    const auto writer_count = w1.operations + w2.operations;
    const std::string csv_header = "experiment,operations,read_percent,read_ms,seed,total_ms,reader_done,writer_done,reader_wait_sum_ms,reader_wait_avg_ms,reader_wait_max_ms,writer_wait_sum_ms,writer_wait_avg_ms,writer_wait_max_ms,max_active_readers,violations,timeouts,status";
    std::ifstream existing(options.results);
    std::string previous_header;
    const bool header = !std::getline(existing, previous_header);
    existing.close();
    if (!previous_header.empty() && previous_header.back() == '\r') {
        previous_header.pop_back();
    }
    if (!header && previous_header != csv_header) {
        std::cerr << "results file has an incompatible CSV header\n";
        ok = false;
    }
    std::ofstream csv;
    if (header || previous_header == csv_header) {
        csv.open(options.results, std::ios::app);
    }
    if (!csv) {
        std::cerr << "cannot open results file: " << options.results << std::endl;
        ok = false;
    } else {
        if (header) csv << csv_header << '\n';
        csv << options.experiment << ',' << options.operations << ',' << options.read_percent << ','
            << options.read_ms << ',' << options.seed << ',' << std::fixed << std::setprecision(6)
            << (experiment_end.QuadPart - experiment_start.QuadPart) * to_ms << ','
            << reader_count << ',' << writer_count << ',';
        csv << (r1.wait_ticks + r2.wait_ticks) * to_ms << ',';
        if (reader_count) csv << (r1.wait_ticks + r2.wait_ticks) * to_ms / reader_count;
        csv << ',' << (r1.max_wait_ticks > r2.max_wait_ticks ? r1.max_wait_ticks : r2.max_wait_ticks) * to_ms
            << ',' << (w1.wait_ticks + w2.wait_ticks) * to_ms << ',';
        if (writer_count) csv << (w1.wait_ticks + w2.wait_ticks) * to_ms / writer_count;
        csv << ',' << (w1.max_wait_ticks > w2.max_wait_ticks ? w1.max_wait_ticks : w2.max_wait_ticks) * to_ms
            << ',' << max_readers << ',' << violations << ',' << timeouts
            << ',' << (ok ? "ok" : "error") << '\n';
        csv.close();
        if (!csv) {
            std::cerr << "cannot write results file" << std::endl;
            ok = false;
        }
    }
    return ok ? 0 : 1;
}
} // namespace app

int main(int argc, char** argv) {
    if (argc == 5 && std::string(argv[1]) == "--worker") {
        return app::worker(argv[2], argv[3], argv[4]);
    }
    try {
        return app::coordinator(cli::parse(argc, argv));
    } catch (const std::invalid_argument& error) {
        std::cerr << "[ARGUMENT ERROR] " << error.what() << '\n';
        return 2;
    }
}
