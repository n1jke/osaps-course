#pragma once

#include "codec.h"

#include <functional>
#include <string>

namespace file_io {

struct Stats {
    std::uint64_t count = 0;
    std::uint64_t file_size = 0;
    std::uint64_t processed_bytes = 0;
    double elapsed_ms = 0;
};

struct AddResult {
    codec::Uuid id{};
    Stats stats;
};

struct FindResult {
    bool found = false;
    codec::Record record;
    Stats stats;
};

struct GenerateResult {
    codec::Uuid first{};
    codec::Uuid middle{};
    codec::Uuid last{};
    Stats stats;
};

codec::Uuid create_uuid();
codec::Uuid parse_uuid(const std::string& text);
std::string format_uuid(const codec::Uuid& id);

void output(const std::string& text, bool error = false);
void printWinError(const char* operation, unsigned long error) noexcept;

Stats create(const std::string& path);
AddResult add(const std::string& path, const std::string& text);
Stats list(const std::string& path, const std::function<void(const codec::Record&)>& visitor);
FindResult find(const std::string& path, const codec::Uuid& id);
GenerateResult generate(const std::string& path, std::uint64_t count,
                        const std::function<codec::Record(std::uint64_t)>& make_record);

} // namespace file_io
