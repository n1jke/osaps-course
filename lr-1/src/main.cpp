#include <windows.h>

#include <iostream>
#include <string>
#include <vector>

namespace {

long long const kMaxN = 3000000;
LARGE_INTEGER g_frequency = {0};

struct ThreadWork {
    long long start;
    long long end;
    long long result;
    int index;
};

struct ProcessWork {
    HANDLE handle;
    DWORD pid;
    int index;
    std::string fileName;
    long long result;
};

void printError(const char* operation) {
    DWORD error = GetLastError();
    char text[512] = {0};
    FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                   NULL, error, 0, text, sizeof(text), NULL);
    std::cout << "[ERROR] " << operation << ": " << error << " : " << text << '\n';
}

bool parseNumber(const char* text, long long& value) {
    try {
        std::size_t pos = 0;
        value = std::stoll(text, &pos);
        return pos == std::string(text).size();
    } catch (...) {
        return false;
    }
}

double now() {
    LARGE_INTEGER counter = {0};
    QueryPerformanceCounter(&counter);
    return static_cast<double>(counter.QuadPart) / g_frequency.QuadPart;
}

long long valueAt(long long i) {
    return i * i + 3 * i + 7;
}

long long calculate(long long start, long long end) {
    long long sum = 0;
    for (long long i = start; i <= end; ++i) {
        sum += valueAt(i);
    }
    return sum;
}

DWORD WINAPI threadWorker(LPVOID parameter) {
    ThreadWork* work = static_cast<ThreadWork*>(parameter);
    work->result = calculate(work->start, work->end);

    std::cout << "[thread] TID=" << GetCurrentThreadId()
              << ", #" << work->index
              << ", range=[" << work->start << ", " << work->end << "]"
              << ", partial=" << work->result << '\n';
    return 0;
}

bool runThreads(long long n, int k, long long& total, double& creationTime) {
    long long chunk = n / k;
    std::vector<ThreadWork> works(k);
    std::vector<HANDLE> handles(k, NULL);

    for (int i = 0; i < k; ++i) {
        works[i] = {1 + static_cast<long long>(i) * chunk,
                    i == k - 1 ? n : static_cast<long long>(i + 1) * chunk,
                    0, i + 1};
    }

    double start = now();
    for (int i = 0; i < k; ++i) {
        handles[i] = CreateThread(NULL, 0, threadWorker, &works[i], 0, NULL);
        if (handles[i] == NULL) {
            printError("CreateThread");
            for (int j = 0; j < i; ++j) {
                CloseHandle(handles[j]);
            }
            return false;
        }
    }
    creationTime = now() - start;

    if (WaitForMultipleObjects(k, handles.data(), TRUE, INFINITE) != WAIT_OBJECT_0) {
        printError("WaitForMultipleObjects(thread)");
        for (int i = 0; i < k; ++i) {
            CloseHandle(handles[i]);
        }
        return false;
    }

    total = 0;
    for (int i = 0; i < k; ++i) {
        DWORD code = 0;
        if (!GetExitCodeThread(handles[i], &code)) {
            printError("GetExitCodeThread");
            CloseHandle(handles[i]);
            return false;
        }

        total += works[i].result;
        std::cout << "[main] thread #" << i + 1
                  << " completed, exit code=" << code << '\n';
        CloseHandle(handles[i]);
    }
    return true;
}

bool readResult(const std::string& fileName, long long& result) {
    HANDLE file = CreateFileA(fileName.c_str(), GENERIC_READ, 0, NULL,
                              OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        printError("CreateFileA(read result)");
        return false;
    }

    char text[64] = {0};
    DWORD read = 0;
    bool ok = ReadFile(file, text, sizeof(text) - 1, &read, NULL);
    if (!ok) {
        printError("ReadFile");
    }
    CloseHandle(file);

    return ok && parseNumber(text, result);
}

void waitAndClose(std::vector<ProcessWork>& works) {
    for (int i = 0; i < static_cast<int>(works.size()); ++i) {
        WaitForSingleObject(works[i].handle, INFINITE);
        CloseHandle(works[i].handle);
    }
}

bool runProcesses(const std::string& workerPath, long long n, int k,
                  long long& total, double& creationTime) {
    long long chunk = n / k;
    std::vector<ProcessWork> works;
    works.reserve(k);

    double start = now();
    for (int i = 0; i < k; ++i) {
        long long rangeStart = 1 + static_cast<long long>(i) * chunk;
        long long rangeEnd = i == k - 1 ? n : static_cast<long long>(i + 1) * chunk;
        std::string fileName = "result_" + std::to_string(i) + ".txt";
        std::string commandLine = "\"" + workerPath + "\" " +
                                  std::to_string(rangeStart) + " " +
                                  std::to_string(rangeEnd) + " \"" + fileName + "\"";
        std::vector<char> command(commandLine.begin(), commandLine.end());
        command.push_back('\0');

        STARTUPINFOA startupInfo = {0};
        startupInfo.cb = sizeof(startupInfo);
        PROCESS_INFORMATION processInfo = {0};
        if (!CreateProcessA(workerPath.c_str(), command.data(), NULL, NULL, FALSE,
                            0, NULL, NULL, &startupInfo, &processInfo)) {
            printError("CreateProcessA");
            waitAndClose(works);
            return false;
        }

        CloseHandle(processInfo.hThread);
        works.push_back({processInfo.hProcess, processInfo.dwProcessId, i + 1, fileName, 0});
    }
    creationTime = now() - start;

    for (int i = 0; i < k; ++i) {
        std::cout << "[main] process #" << works[i].index
                  << " started, PID=" << works[i].pid << '\n';
    }

    std::vector<HANDLE> handles;
    for (int i = 0; i < k; ++i) {
        handles.push_back(works[i].handle);
    }
    if (WaitForMultipleObjects(k, handles.data(), TRUE, INFINITE) != WAIT_OBJECT_0) {
        printError("WaitForMultipleObjects(process)");
        waitAndClose(works);
        return false;
    }

    total = 0;
    bool ok = true;
    for (int i = 0; i < k; ++i) {
        DWORD code = 0;
        if (!GetExitCodeProcess(works[i].handle, &code)) {
            printError("GetExitCodeProcess");
            ok = false;
        } else if (code != 0 || !readResult(works[i].fileName, works[i].result)) {
            ok = false;
        } else {
            total += works[i].result;
        }

        std::cout << "[main] process #" << works[i].index
                  << ", PID=" << works[i].pid
                  << ", exit code=" << code << '\n';
        CloseHandle(works[i].handle);
        DeleteFileA(works[i].fileName.c_str());
    }
    return ok;
}

bool getWorkerPath(std::string& workerPath) {
    char path[MAX_PATH] = {0};
    if (GetModuleFileNameA(NULL, path, sizeof(path)) == 0) {
        printError("GetModuleFileNameA");
        return false;
    }

    std::string self = path;
    std::size_t slash = self.find_last_of("\\/");
    workerPath = slash == std::string::npos
        ? "worker.exe"
        : self.substr(0, slash + 1) + "worker.exe";
    return true;
}

void printUsage() {
    std::cout << "Usage: main.exe <N> <K> [seq]\n"
              << "N: divisible by 8, from 8 to " << kMaxN << "\n"
              << "K: 1, 2, 4 or 8\n";
}

} // namespace

int main(int argc, char* argv[]) {
    if (argc < 3 || argc > 4) {
        printUsage();
        return 1;
    }

    long long n = 0;
    long long kValue = 0;
    if (!parseNumber(argv[1], n) || !parseNumber(argv[2], kValue) ||
        n < 8 || n > kMaxN || n % 8 != 0 ||
        (kValue != 1 && kValue != 2 && kValue != 4 && kValue != 8)) {
        printUsage();
        return 1;
    }
    if (argc == 4 && std::string(argv[3]) != "seq") {
        printUsage();
        return 1;
    }
    if (!QueryPerformanceFrequency(&g_frequency)) {
        printError("QueryPerformanceFrequency");
        return 1;
    }

    std::string workerPath;
    if (!getWorkerPath(workerPath)) {
        return 1;
    }

    int k = static_cast<int>(kValue);
    std::cout << "=== Process vs Thread: N=" << n << ", K=" << k << " ===\n";

    double start = now();
    long long expected = calculate(1, n);
    std::cout << "Sequential: result=" << expected << ", time=" << now() - start << " s\n";
    if (argc == 4) {
        return 0;
    }

    long long threadResult = 0;
    double threadCreation = 0;
    start = now();
    bool threadsOk = runThreads(n, k, threadResult, threadCreation);
    std::cout << "Threads: result=" << threadResult
              << ", matches sequential=" << (threadsOk && threadResult == expected ? "yes" : "no")
              << ", creation=" << threadCreation
              << " s, total=" << now() - start << " s\n";

    long long processResult = 0;
    double processCreation = 0;
    start = now();
    bool processesOk = runProcesses(workerPath, n, k, processResult, processCreation);
    std::cout << "Processes: result=" << processResult
              << ", matches sequential=" << (processesOk && processResult == expected ? "yes" : "no")
              << ", creation=" << processCreation
              << " s, total=" << now() - start << " s\n";

    return threadsOk && processesOk &&
           threadResult == expected && processResult == expected ? 0 : 1;
}
