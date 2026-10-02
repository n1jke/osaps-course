#include "file_io.h"

#include <windows.h>
#include <rpc.h>

#include <algorithm>
#include <iostream>
#include <limits>
#include <memory>
#include <system_error>

namespace file_io {

[[noreturn]] void fail(const char* operation) {
    const DWORD error = GetLastError();

    throw std::system_error(static_cast<int>(error), std::system_category(), operation);
}

void close_handle(HANDLE handle) noexcept {
    if (handle != INVALID_HANDLE_VALUE && !CloseHandle(handle)) {
        const DWORD error = GetLastError();
        printWinError("CloseHandle(cleanup)", error);
    }
}

// Standard ownership wrapper, without a custom File class.
using File = std::unique_ptr<void, decltype(&close_handle)>;

void close(File& file) {
    const HANDLE handle = file.release();

    if (handle != nullptr && !CloseHandle(handle)) {
        fail("CloseHandle");
    }
}

LONGLONG counter_now() {
    LARGE_INTEGER tick{};

    if (!QueryPerformanceCounter(&tick)) {
        fail("QueryPerformanceCounter");
    }

    return tick.QuadPart;
}

double counter_frequency() {
    LARGE_INTEGER frequency{};

    if (!QueryPerformanceFrequency(&frequency)) {
        fail("QueryPerformanceFrequency");
    }

    if (frequency.QuadPart <= 0) {
        throw std::runtime_error("Invalid performance-counter frequency");
    }

    return static_cast<double>(frequency.QuadPart);
}

double milliseconds(LONGLONG ticks, double frequency) {
    return static_cast<double>(ticks) * 1000.0 / frequency;
}

File open(const std::string& path, bool writable, bool fresh) {
    const HANDLE handle = CreateFileA(path.c_str(), GENERIC_READ | (writable ? GENERIC_WRITE : 0),
        writable ? 0 : FILE_SHARE_READ, nullptr, fresh ? CREATE_NEW : OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL, nullptr);

    if (handle == INVALID_HANDLE_VALUE) {
        fail("CreateFileA");
    }

    File file(handle, close_handle);
    SetLastError(ERROR_SUCCESS);
    const DWORD type = GetFileType(handle);

    if (type == FILE_TYPE_UNKNOWN) {
        const DWORD error = GetLastError();

        if (error != ERROR_SUCCESS) {
            throw std::system_error(static_cast<int>(error), std::system_category(), "GetFileType");
        }
    }

    if (type != FILE_TYPE_DISK) {
        throw std::runtime_error("Expected a regular disk file");
    }

    BY_HANDLE_FILE_INFORMATION information{};

    if (!GetFileInformationByHandle(handle, &information)) {
        fail("GetFileInformationByHandle");
    }

    if (information.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
        throw std::runtime_error("Database path is a directory");
    }

    return file;
}

void seek(File& file, std::uint64_t offset) {
    const auto max_offset = static_cast<std::uint64_t>(std::numeric_limits<LONGLONG>::max());

    if (offset > max_offset) {
        throw std::runtime_error("File offset overflow");
    }

    LARGE_INTEGER position{};
    position.QuadPart = static_cast<LONGLONG>(offset);

    if (!SetFilePointerEx(file.get(), position, nullptr, FILE_BEGIN)) {
        fail("SetFilePointerEx");
    }
}

std::uint64_t size(File& file) {
    LARGE_INTEGER result{};

    if (!GetFileSizeEx(file.get(), &result)) {
        fail("GetFileSizeEx");
    }

    if (result.QuadPart < 0) {
        throw std::runtime_error("Negative file size");
    }

    return static_cast<std::uint64_t>(result.QuadPart);
}

void save(File& file) {
    if (!FlushFileBuffers(file.get())) {
        fail("FlushFileBuffers");
    }
}

void read_exact(File& file, codec::Bytes& bytes, std::size_t offset, std::size_t length) {
    while (length != 0) {
        const auto request = static_cast<DWORD>(std::min(length,
            static_cast<std::size_t>(std::numeric_limits<DWORD>::max())));
        DWORD transferred = 0;

        if (!ReadFile(file.get(), bytes.data() + offset, request, &transferred, nullptr)) {
            fail("ReadFile");
        }

        if (transferred == 0) {
            throw std::runtime_error("Unexpected end of file");
        }

        offset += transferred;
        length -= transferred;
    }
}

codec::Bytes read_bytes(File& file, std::size_t length) {
    codec::Bytes bytes(length);
    read_exact(file, bytes, 0, length);

    return bytes;
}

void write_all(File& file, const codec::Bytes& bytes) {
    std::size_t offset = 0;

    while (offset < bytes.size()) {
        const auto request = static_cast<DWORD>(std::min(bytes.size() - offset,
            static_cast<std::size_t>(std::numeric_limits<DWORD>::max())));
        DWORD transferred = 0;

        if (!WriteFile(file.get(), bytes.data() + offset, request, &transferred, nullptr)) {
            fail("WriteFile");
        }

        if (transferred == 0) {
            throw std::runtime_error("WriteFile made no progress");
        }

        offset += transferred;
    }
}

Stats scan(File& file, const std::function<void(const codec::Record&)>& visitor = {},
          bool exclude_visitor = false) {
    const double frequency = counter_frequency();
    const auto started = counter_now();
    LONGLONG output_ticks = 0;
    Stats result;
    result.file_size = size(file);

    if (result.file_size < codec::kHeaderSize) {
        throw std::runtime_error("Truncated database header");
    }

    seek(file, 0);
    result.count = codec::deserialize_header(read_bytes(file, codec::kHeaderSize));

    if (result.count > (result.file_size - codec::kHeaderSize) / codec::kRecordHeaderSize) {
        throw std::runtime_error("Record count exceeds the available file data");
    }

    std::uint64_t position = codec::kHeaderSize;

    for (std::uint64_t i = 0; i < result.count; ++i) {
        if (result.file_size - position < codec::kRecordHeaderSize) {
            throw std::runtime_error("Truncated record header");
        }

        auto bytes = read_bytes(file, codec::kRecordHeaderSize);
        const auto length = codec::text_length(bytes);

        if (length > result.file_size - position - codec::kRecordHeaderSize) {
            throw std::runtime_error("Text length extends beyond the end of the database");
        }

        bytes.resize(codec::kRecordHeaderSize + length);
        read_exact(file, bytes, codec::kRecordHeaderSize, length);
        const auto record = codec::deserialize(bytes);
        position += bytes.size();

        if (visitor) {
            if (exclude_visitor) {
                const auto before = counter_now();
                visitor(record);
                output_ticks += counter_now() - before;
            } else {
                visitor(record);
            }
        }
    }

    if (position != result.file_size) {
        throw std::runtime_error("Trailing data or incorrect record count");
    }

    result.processed_bytes = position;
    result.elapsed_ms = milliseconds(counter_now() - started - output_ticks, frequency);

    return result;
}

void verify_header(File& file, std::uint64_t count, std::uint64_t expected_size) {
    if (size(file) != expected_size) {
        throw std::runtime_error("Verification failed: file size mismatch");
    }

    seek(file, 0);

    if (codec::deserialize_header(read_bytes(file, codec::kHeaderSize)) != count) {
        throw std::runtime_error("Verification failed: record count mismatch");
    }
}

void rollback(File& file, std::uint64_t count, std::uint64_t original_size) noexcept {
    try {
        seek(file, 0);
        write_all(file, codec::serialize_header(count));
        seek(file, original_size);

        if (!SetEndOfFile(file.get())) {
            fail("SetEndOfFile(rollback)");
        }

        save(file);
        verify_header(file, count, original_size);

        try {
            output("[ERROR] add failed; rollback succeeded\n", true);
        } catch (...) {
            // Reporting must not replace the original add error.
        }
    } catch (const std::system_error& error) {
        printWinError(error.what(), static_cast<unsigned long>(error.code().value()));

        try {
            output("[ERROR] rollback failed; database may be damaged\n", true);
        } catch (...) {
            // Preserve the original exception even if stderr is unavailable.
        }
    } catch (...) {
        try {
            output("[ERROR] rollback failed; database may be damaged\n", true);
        } catch (...) {
            // Preserve the original exception even if stderr is unavailable.
        }
    }
}

codec::Uuid from_native_uuid(const UUID& uuid) {
    codec::Uuid id{};

    for (unsigned i = 0; i < 4; ++i) {
        id[i] = static_cast<std::uint8_t>(uuid.Data1 >> ((3 - i) * 8));
    }

    id[4] = static_cast<std::uint8_t>(uuid.Data2 >> 8);
    id[5] = static_cast<std::uint8_t>(uuid.Data2);
    id[6] = static_cast<std::uint8_t>(uuid.Data3 >> 8);
    id[7] = static_cast<std::uint8_t>(uuid.Data3);

    std::copy_n(uuid.Data4, 8, id.begin() + 8);

    return id;
}

UUID to_native_uuid(const codec::Uuid& id) {
    UUID uuid{};

    for (unsigned i = 0; i < 4; ++i) {
        uuid.Data1 = (uuid.Data1 << 8) | id[i];
    }

    uuid.Data2 = static_cast<unsigned short>((id[4] << 8) | id[5]);
    uuid.Data3 = static_cast<unsigned short>((id[6] << 8) | id[7]);

    std::copy_n(id.begin() + 8, 8, uuid.Data4);

    return uuid;
}

codec::Uuid create_uuid() {
    UUID uuid{};
    const RPC_STATUS status = UuidCreate(&uuid);

    if (status != RPC_S_OK) {
        throw std::system_error(static_cast<int>(status), std::system_category(), "UuidCreate");
    }

    return from_native_uuid(uuid);
}

codec::Uuid parse_uuid(const std::string& text) {
    if (text.size() != 36) {
        throw std::invalid_argument("UUID must have 36 characters");
    }

    UUID uuid{};
    // RPC accepts a mutable pointer; give it a copy of the input string.
    auto input = text;
    const RPC_STATUS status = UuidFromStringA(reinterpret_cast<RPC_CSTR>(input.data()), &uuid);

    if (status == RPC_S_INVALID_STRING_UUID) {
        throw std::invalid_argument("Invalid UUID; expected 8-4-4-4-12 hex digits");
    }

    if (status != RPC_S_OK) {
        throw std::system_error(static_cast<int>(status), std::system_category(), "UuidFromStringA");
    }

    return from_native_uuid(uuid);
}

void free_rpc_string(unsigned char* text) noexcept {
    const RPC_STATUS status = RpcStringFreeA(&text);

    if (status != RPC_S_OK) {
        printWinError("RpcStringFreeA", status);
    }
}

std::string format_uuid(const codec::Uuid& id) {
    const auto uuid = to_native_uuid(id);
    RPC_CSTR text = nullptr;
    const RPC_STATUS status = UuidToStringA(&uuid, &text);

    if (status != RPC_S_OK) {
        throw std::system_error(static_cast<int>(status), std::system_category(), "UuidToStringA");
    }

    // Release the RPC string even if constructing std::string throws.
    const std::unique_ptr<unsigned char, decltype(&free_rpc_string)> owned(text, free_rpc_string);

    return std::string(reinterpret_cast<const char*>(owned.get()));
}

void output(const std::string& text, bool error) {
    auto& stream = error ? std::cerr : std::cout;

    stream << text;
    stream.flush();

    if (!stream) {
        throw std::runtime_error("Cannot write output");
    }
}

void printWinError(const char* operation, unsigned long error) noexcept {
    // Copied from lr-1/src/worker.cpp; keep the original code before cleanup.
    char message[512] = {};

    FormatMessageA(
        FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, error, 0, message, static_cast<DWORD>(sizeof(message)), nullptr);

    std::cerr << "[ERROR] " << operation << ": " << error << " : "
              << (message[0] ? message : "Description unavailable") << '\n';
}

Stats create(const std::string& path) {
    auto file = open(path, true, true);
    const double frequency = counter_frequency();
    const auto started = counter_now();

    write_all(file, codec::serialize_header(0));
    save(file);
    const double elapsed = milliseconds(counter_now() - started, frequency);

    verify_header(file, 0, codec::kHeaderSize);
    close(file);

    return {0, codec::kHeaderSize, codec::kHeaderSize, elapsed};
}

AddResult add(const std::string& path, const std::string& text) {
    if (text.size() > codec::kMaxTextLength || !codec::valid_ascii(text)) {
        throw std::invalid_argument("Text must be valid ASCII, at most 1 MiB");
    }

    auto file = open(path, true, false);
    const auto original = scan(file);
    const codec::Record record{create_uuid(), text};
    const auto bytes = codec::serialize(record);
    const auto max_size = static_cast<std::uint64_t>(std::numeric_limits<LONGLONG>::max());

    if (original.count == std::numeric_limits<std::uint64_t>::max() ||
        original.file_size > max_size - bytes.size()) {
        throw std::runtime_error("Database size/count overflow");
    }

    const double frequency = counter_frequency();
    // Allocation errors before the first write need no rollback.
    const auto header = codec::serialize_header(original.count + 1);
    const auto started = counter_now();
    double write_ms = 0;

    try {
        seek(file, original.file_size);
        write_all(file, bytes);
        save(file);
        seek(file, 0);
        write_all(file, header);
        save(file);
        write_ms = milliseconds(counter_now() - started, frequency);

        verify_header(file, original.count + 1, original.file_size + bytes.size());
        seek(file, original.file_size);
        const auto actual = read_bytes(file, bytes.size());
        const auto verified = codec::deserialize(actual);

        if (actual != bytes || verified.id != record.id || verified.text != record.text) {
            throw std::runtime_error("Verification failed: appended record mismatch");
        }
    } catch (...) {
        rollback(file, original.count, original.file_size);
        throw;
    }

    close(file);

    return {record.id, {original.count + 1, original.file_size + bytes.size(),
        original.file_size + bytes.size() + codec::kHeaderSize, original.elapsed_ms + write_ms}};
}

Stats list(const std::string& path, const std::function<void(const codec::Record&)>& visitor) {
    auto file = open(path, false, false);
    auto stats = scan(file, visitor, true);
    close(file);

    return stats;
}

FindResult find(const std::string& path, const codec::Uuid& id) {
    auto file = open(path, false, false);
    FindResult result;

    result.stats = scan(file, [&](const codec::Record& record) {
        if (!result.found && record.id == id) {
            result.record = record;
            result.found = true;
        }
    });

    close(file);

    return result;
}

GenerateResult generate(const std::string& path, std::uint64_t count,
                        const std::function<codec::Record(std::uint64_t)>& make_record) {
    if (!count || !make_record) {
        throw std::invalid_argument("Generator requires count > 0 and a record factory");
    }

    auto file = open(path, true, true);
    GenerateResult result;
    const double frequency = counter_frequency();
    const auto started = counter_now();
    std::uint64_t expected_size = codec::kHeaderSize;

    try {
        write_all(file, codec::serialize_header(0));

        for (std::uint64_t i = 0; i < count; ++i) {
            const auto record = make_record(i);
            const auto bytes = codec::serialize(record);
            const auto max_size = static_cast<std::uint64_t>(std::numeric_limits<LONGLONG>::max());

            if (expected_size > max_size - bytes.size()) {
                throw std::runtime_error("Generated file size overflow");
            }

            if (i == 0) {
                result.first = record.id;
            }

            if (i == count / 2) {
                result.middle = record.id;
            }

            if (i == count - 1) {
                result.last = record.id;
            }

            write_all(file, bytes);

            // Verify each record directly instead of computing a checksum.
            seek(file, expected_size);

            if (read_bytes(file, bytes.size()) != bytes) {
                throw std::runtime_error("Verification failed: generated record mismatch");
            }

            expected_size += bytes.size();
        }

        save(file);
        seek(file, 0);
        write_all(file, codec::serialize_header(count));
        save(file);
        const auto elapsed = milliseconds(counter_now() - started, frequency);
        const auto actual = scan(file);

        if (actual.count != count || actual.file_size != expected_size) {
            throw std::runtime_error("Verification failed: generated database mismatch");
        }

        result.stats = {count, expected_size, expected_size * 2, elapsed};
    } catch (...) {
        try {
            output("[ERROR] generate failed; output file is incomplete, do not use it\n", true);
        } catch (...) {
            // Preserve the generation error.
        }

        throw;
    }

    close(file);

    return result;
}

} // namespace file_io
