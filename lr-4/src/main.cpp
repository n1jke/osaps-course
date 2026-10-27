#include <windows.h>

#include <array>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

namespace {
constexpr std::size_t record_count = 100;
constexpr DWORD wait_limit_ms = 30000;

struct SharedData {
    std::uint32_t records[record_count];
};

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
    HANDLE mapping = OpenFileMappingA(FILE_MAP_READ, FALSE, name);
    if (!mapping) {
        report_error("OpenFileMapping", GetLastError());
        return 1;
    }
    const auto* data = static_cast<const SharedData*>(
        MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, sizeof(SharedData)));
    if (!data) {
        const DWORD error = GetLastError();
        CloseHandle(mapping);
        report_error("MapViewOfFile", error);
        return 1;
    }

    // This stage only reads initialized data; concurrent writes arrive with synchronization.
    std::cout << role << ' ' << number << " PID=" << GetCurrentProcessId()
              << " STATE=READY RECORDS=" << record_count
              << " FIRST=" << data->records[0] << std::endl;
    const BOOL unmapped = UnmapViewOfFile(data);
    const DWORD unmap_error = unmapped ? ERROR_SUCCESS : GetLastError();
    CloseHandle(mapping);
    if (!unmapped) {
        report_error("UnmapViewOfFile", unmap_error);
        return 1;
    }
    return 0;
}

int coordinator() {
    const std::string name = "Local\\lab4_mapping_" + std::to_string(GetCurrentProcessId());
    HANDLE mapping = CreateFileMappingA(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE,
                                        0, sizeof(SharedData), name.c_str());
    if (!mapping) {
        report_error("CreateFileMapping", GetLastError());
        return 1;
    }
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        std::cerr << "mapping name already exists\n";
        CloseHandle(mapping);
        return 1;
    }
    auto* data = static_cast<SharedData*>(
        MapViewOfFile(mapping, FILE_MAP_WRITE, 0, 0, sizeof(SharedData)));
    if (!data) {
        const DWORD error = GetLastError();
        CloseHandle(mapping);
        report_error("MapViewOfFile", error);
        return 1;
    }
    *data = {};

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
    if (!UnmapViewOfFile(data)) {
        report_error("UnmapViewOfFile", GetLastError());
        ok = false;
    }
    CloseHandle(mapping);
    return ok ? 0 : 1;
}
} // namespace

int main(int argc, char** argv) {
    if (argc == 1) {
        return coordinator();
    }

    if (argc == 5 && std::string(argv[1]) == "--worker") {
        return worker(argv[2], argv[3], argv[4]);
    }
    std::cerr << "usage: lab4 (base stage; experiment parameters not implemented yet)\n";
    return 2;
}
