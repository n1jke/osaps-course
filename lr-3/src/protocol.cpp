#include "protocol.h"
#include <chrono>
#include <stdexcept>

namespace protocol {
uint64_t now_ms() {
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count());
}
void append_u32(std::vector<uint8_t>& bytes, uint32_t value) {
    for (unsigned shift = 0; shift < 32; shift += 8) {
        bytes.push_back(static_cast<uint8_t>(value >> shift));
    }
}
uint32_t read_u32(const uint8_t* bytes) {
    uint32_t value = 0;
    for (unsigned i = 0; i < 4; ++i) {
        value |= static_cast<uint32_t>(bytes[i]) << (8 * i);
    }
    return value;
}
std::vector<uint8_t> encode(const Message& m) {
    if (m.payload.size() > MAX_SIZE - HEADER_SIZE) {
        throw std::runtime_error("Payload exceeds protocol limit");
    }
    std::vector<uint8_t> bytes;
    bytes.reserve(HEADER_SIZE + m.payload.size());
    append_u32(bytes, m.version);
    append_u32(bytes, m.type);
    append_u32(bytes, m.pid);
    append_u32(bytes, m.request);
    append_u32(bytes, static_cast<uint32_t>(m.payload.size()));
    append_u32(bytes, m.run);
    append_u32(bytes, m.client);
    append_u32(bytes, static_cast<uint32_t>(m.sent_at));
    append_u32(bytes, static_cast<uint32_t>(m.sent_at >> 32));
    bytes.insert(bytes.end(), m.payload.begin(), m.payload.end());
    return bytes;
}
Message decode(const std::vector<uint8_t>& bytes) {
    if (bytes.size() < HEADER_SIZE || bytes.size() > MAX_SIZE) {
        throw std::runtime_error("Invalid message size");
    }
    const auto* p = bytes.data();
    if (read_u32(p + 16) != bytes.size() - HEADER_SIZE) {
        throw std::runtime_error("payloadSize does not match actual size");
    }
    Message m;
    m.version = read_u32(p);
    m.type = read_u32(p + 4);
    m.pid = read_u32(p + 8);
    m.request = read_u32(p + 12);
    m.run = read_u32(p + 20);
    m.client = read_u32(p + 24);
    m.sent_at = read_u32(p + 28) | (static_cast<uint64_t>(read_u32(p + 32)) << 32);
    if (m.version != VERSION) {
        throw std::runtime_error("Unknown protocol version");
    }
    if (m.type != LOG && m.type != DONE) {
        throw std::runtime_error("Unknown message type");
    }
    m.payload.assign(bytes.begin() + HEADER_SIZE, bytes.end());
    return m;
}
std::string escape(const std::vector<uint8_t>& text) {
    const char* hex = "0123456789ABCDEF";
    std::string result;
    for (uint8_t c : text) {
        if (c == '\\' || c == '|') {
            result += '\\';
            result += static_cast<char>(c);
        } else if (c < 32 || c > 126) {
            result += "\\x";
            result += hex[c >> 4];
            result += hex[c & 15];
        } else {
            result += static_cast<char>(c);
        }
    }
    return result;
}
}
