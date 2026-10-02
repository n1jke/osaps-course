#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace codec {

using Bytes = std::vector<std::uint8_t>;
using Uuid = std::array<std::uint8_t, 16>;

constexpr std::size_t kHeaderSize = 12;
constexpr std::size_t kRecordHeaderSize = 20;
constexpr std::uint32_t kVersion = 1;
constexpr std::uint32_t kMaxTextLength = 1024 * 1024; // 1 MiB

struct Record {
    Uuid id{};
    std::string text;
};

Bytes serialize(const Record& record);
Record deserialize(const Bytes& bytes);
Bytes serialize_header(std::uint64_t count);
std::uint64_t deserialize_header(const Bytes& bytes);
std::uint32_t text_length(const Bytes& prefix);
bool valid_ascii(const std::string& text);

std::string format_record(const Record& record, const std::string& id);

} // namespace codec
