#include "codec.h"

#include <algorithm>
#include <stdexcept>

namespace codec {

void put_le(Bytes& bytes, std::uint64_t value, std::size_t width) {
    for (std::size_t i = 0; i < width; ++i) {
        bytes.push_back(static_cast<std::uint8_t>(value & 0xff));
        value >>= 8;
    }
}

std::uint64_t get_le(const Bytes& bytes, std::size_t offset, std::size_t width) {
    if (offset > bytes.size() || width > bytes.size() - offset) {
        throw std::runtime_error("Truncated integer field");
    }

    std::uint64_t value = 0;

    for (std::size_t i = 0; i < width; ++i) {
        value |= static_cast<std::uint64_t>(bytes[offset + i]) << (i * 8);
    }

    return value;
}

bool valid_ascii(const std::string& text) {
    return std::all_of(text.begin(), text.end(), [](unsigned char c) {
        return c < 128;
    });
}

Bytes serialize(const Record& record) {
    if (record.text.size() > kMaxTextLength || !valid_ascii(record.text)) {
        throw std::runtime_error("Invalid text: expected ASCII, at most 1 MiB");
    }

    Bytes bytes;
    bytes.reserve(kRecordHeaderSize + record.text.size());
    bytes.insert(bytes.end(), record.id.begin(), record.id.end());
    put_le(bytes, record.text.size(), 4);
    bytes.insert(bytes.end(), record.text.begin(), record.text.end());

    return bytes;
}

std::uint32_t text_length(const Bytes& prefix) {
    if (prefix.size() < kRecordHeaderSize) {
        throw std::runtime_error("Truncated record header");
    }

    const auto length = static_cast<std::uint32_t>(get_le(prefix, 16, 4));

    if (length > kMaxTextLength) {
        throw std::runtime_error("Text exceeds 1 MiB");
    }

    return length;
}

Record deserialize(const Bytes& bytes) {
    const auto length = text_length(bytes);

    if (bytes.size() != kRecordHeaderSize + length) {
        throw std::runtime_error("Record length does not match its data");
    }

    Record record;

    // Копируем первые 16 байт записи из bytes в массив UUID record.id.
    std::copy_n(bytes.begin(), record.id.size(), record.id.begin());

    // assign заменяет текст строкой из байтов после заголовка (UUID + длина, 20 байт).
    // Диапазон: от bytes.begin() + kRecordHeaderSize до bytes.end(), не включая end.
    record.text.assign(bytes.begin() + kRecordHeaderSize, bytes.end());

    if (!valid_ascii(record.text)) {
        throw std::runtime_error("Invalid ASCII in record");
    }

    return record;
}

Bytes serialize_header(std::uint64_t count) {
    Bytes bytes;
    bytes.reserve(kHeaderSize);
    put_le(bytes, kVersion, 4);
    put_le(bytes, count, 8);

    return bytes;
}

std::uint64_t deserialize_header(const Bytes& bytes) {
    if (bytes.size() != kHeaderSize || get_le(bytes, 0, 4) != kVersion) {
        throw std::runtime_error("Invalid database header or unsupported version");
    }

    return get_le(bytes, 4, 8);
}

std::string format_record(const Record& record, const std::string& id) {
    std::string result = "id=" + id +
        " text_length=" + std::to_string(record.text.size()) + " text=\"";
    constexpr char hex[] = "0123456789abcdef";

    for (unsigned char c : record.text) {
        if (c == '"' || c == '\\') {
            result += '\\';
            result += static_cast<char>(c);
        } else if (c < 0x20 || c == 0x7f) {
            result += "\\x";
            result += hex[c >> 4];
            result += hex[c & 0x0f];
        } else {
            result += static_cast<char>(c);
        }
    }

    return result + "\"\n";
}

} // namespace codec
