#include <windows.h>

#include <cstdio>
#include <iostream>
#include <string>

namespace {

long long const kMaxN = 3000000;

long long valueAt(long long i) {
    return i * i + 3 * i + 7;
}

void printWinError(const char* operation, DWORD error = GetLastError()) {
    char message[512] = {0};
    FormatMessageA(
        FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL, error, 0, message, static_cast<DWORD>(sizeof(message)), NULL);

    std::cout << "[ERROR] " << operation << ": " << error << " : " << message << '\n';
}

bool parseLongLong(const std::string& text, long long& value) {
    try {
        std::size_t pos = 0;
        value = std::stoll(text, &pos);
        return pos == text.size();
    } catch (...) {
        return false;
    }
}

bool closeHandle(HANDLE handle, const char* operation) {
    if (CloseHandle(handle)) {
        return true;
    }

    printWinError(operation);
    return false;
}

} // namespace

int main(int argc, char* argv[]) {
    if (argc != 4) {
        std::cout << "Usage: worker.exe <start> <end> <output-file>\n";
        return 1;
    }

    long long start = 0;
    long long end = 0;
    if (!parseLongLong(argv[1], start) || !parseLongLong(argv[2], end) ||
        start < 1 || end < start || end > kMaxN) {
        std::cout << "[ERROR] Invalid range [1, 3000000].\n";
        return 1;
    }

    long long sum = 0;
    for (long long i = start; i <= end; ++i) {
        sum += valueAt(i);
    }

    HANDLE file = CreateFileA(argv[3], GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        printWinError("CreateFileA");
        return 1;
    }

    char output[64] = {0};
    int length = std::snprintf(output, sizeof(output), "%lld", sum);
    if (length < 0 || static_cast<std::size_t>(length) >= sizeof(output)) {
        std::cout << "[ERROR] Could not format partial result.\n";
        closeHandle(file, "CloseHandle(output file)");
        return 1;
    }

    DWORD written = 0;
    bool writeOk = WriteFile(file, output, static_cast<DWORD>(length), &written, NULL);
    DWORD writeError = writeOk ? ERROR_SUCCESS : GetLastError();
    bool closeOk = closeHandle(file, "CloseHandle(result file)");
    if (!writeOk) {
        printWinError("WriteFile", writeError);
        return 1;
    }
    if (written != static_cast<DWORD>(length)) {
        std::cout << "[ERROR] WriteFile wrote only " << written << " of " << length << " bytes.\n";
        return 1;
    }
    if (!closeOk) {
        return 1;
    }

    std::cout << "WORKER PID=" << GetCurrentProcessId() << ", range=[" << start << ", " << end << "]" << ", partial=" << sum << '\n';
    return 0;
}
