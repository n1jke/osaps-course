#pragma once

#include <windows.h>

#include <string>

namespace monitor { struct Run; }

namespace output {

struct Error {
    const wchar_t* operation;
    DWORD code;
};

std::wstring describe(const Error& error);
void log(const wchar_t* operation, const std::wstring& object,
         const wchar_t* status, DWORD code = 0, const SYSTEMTIME* timestamp = nullptr);
void save_csv(const std::wstring& path, const monitor::Run& run);

}
