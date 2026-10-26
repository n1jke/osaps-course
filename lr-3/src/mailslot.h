#pragma once
#include <windows.h>
#include <memory>
#include <string>
#include <vector>
#include <cstdint>

namespace mailslot {
using Handle = std::unique_ptr<void, decltype(&CloseHandle)>;
Handle checked(HANDLE handle, const char* operation);
[[noreturn]] void fail(const char* operation, DWORD error);
std::string name(uint32_t run);
Handle server(uint32_t run);
Handle client(uint32_t run);
Handle journal(const std::string& path);
void write(HANDLE handle, const std::vector<uint8_t>& bytes);
void write_line(HANDLE handle, const std::string& line);
bool read(HANDLE handle, HANDLE event, DWORD timeout, std::vector<uint8_t>& bytes);
void close(Handle& handle);
}
