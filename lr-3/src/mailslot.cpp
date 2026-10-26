#include "mailslot.h"
#include "protocol.h"
#include <stdexcept>

namespace mailslot {
[[noreturn]] void fail(const char* operation, DWORD error) {
    char* description = nullptr;
    const DWORD length = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, error, 0,
        reinterpret_cast<char*>(&description), 0, nullptr);
    const std::string detail = length && description ? std::string(description, length) : "No error description";
    if (description) LocalFree(description);
    throw std::runtime_error(std::string(operation) + " GetLastError=" + std::to_string(error) + " " + detail);
}
Handle checked(HANDLE handle, const char* operation) {
    if (handle == INVALID_HANDLE_VALUE || handle == nullptr) fail(operation, GetLastError());
    return Handle(handle, &CloseHandle);
}
std::string name(uint32_t run) {
    return "\\\\.\\mailslot\\lr3_" + std::to_string(run);
}
Handle server(uint32_t run) {
    return checked(CreateMailslotA(name(run).c_str(), static_cast<DWORD>(protocol::MAX_SIZE), 30000, nullptr), "CreateMailslotA");
}
Handle client(uint32_t run) {
    return checked(CreateFileA(name(run).c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr), "CreateFileA(mailslot)");
}
Handle journal(const std::string& path) {
    return checked(CreateFileA(path.c_str(), GENERIC_WRITE, 0, nullptr,
        CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr), "CreateFileA(journal)");
}
void write(HANDLE handle, const std::vector<uint8_t>& bytes) {
    DWORD written = 0;
    if (!WriteFile(handle, bytes.data(), static_cast<DWORD>(bytes.size()), &written, nullptr)) {
        fail("WriteFile(mailslot)", GetLastError());
    }
    // One write is one message: retrying a remainder would create another message.
    if (written != bytes.size()) throw std::runtime_error("Short mailslot write");
}
void write_line(HANDLE handle, const std::string& line) {
    size_t offset = 0;
    while (offset < line.size()) {
        DWORD written = 0;
        if (!WriteFile(handle, line.data() + offset, static_cast<DWORD>(line.size() - offset), &written, nullptr)) {
            fail("WriteFile(journal)", GetLastError());
        }
        if (!written) throw std::runtime_error("Journal write made no progress");
        offset += written;
    }
}
bool read(HANDLE handle, HANDLE event, DWORD timeout, std::vector<uint8_t>& bytes) {
    if (!SetMailslotInfo(handle, timeout)) fail("SetMailslotInfo", GetLastError());
    DWORD next = 0;
    if (!GetMailslotInfo(handle, nullptr, &next, nullptr, nullptr)) fail("GetMailslotInfo", GetLastError());
    if (next != MAILSLOT_NO_MESSAGE && next > protocol::MAX_SIZE) {
        throw std::runtime_error("Incoming message exceeds receive limit");
    }
    bytes.resize(protocol::MAX_SIZE);
    DWORD received = 0;
    if (!ResetEvent(event)) fail("ResetEvent", GetLastError());
    // CreateMailslot returns an overlapped handle. Keep the buffer and operation
    // alive until completion; SetMailslotInfo bounds the pending read itself.
    OVERLAPPED operation{};
    operation.hEvent = event;
    if (!ReadFile(handle, bytes.data(), static_cast<DWORD>(bytes.size()), &received, &operation)) {
        const DWORD error = GetLastError();
        if (error == ERROR_SEM_TIMEOUT) return false;
        if (error != ERROR_IO_PENDING) fail("ReadFile(mailslot)", error);
    }
    if (!GetOverlappedResult(handle, &operation, &received, TRUE)) {
        const DWORD error = GetLastError();
        if (error == ERROR_SEM_TIMEOUT) return false;
        fail("GetOverlappedResult(mailslot)", error);
    }
    bytes.resize(received);
    return true;
}
void close(Handle& handle) {
    HANDLE raw = handle.release();
    if (raw && !CloseHandle(raw)) fail("CloseHandle", GetLastError());
}
}
