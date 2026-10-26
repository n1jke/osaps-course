#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace protocol {
constexpr uint32_t VERSION = 1;
constexpr size_t HEADER_SIZE = 36;
// Bound receive buffers while supporting the required 4 KiB experiment.
constexpr size_t MAX_SIZE = 4096;
enum Type : uint32_t { LOG = 1, DONE = 2 };
struct Message {
    uint32_t version = VERSION;
    uint32_t type = LOG;
    uint32_t pid = 0;
    uint32_t request = 0;
    uint32_t run = 0;
    uint32_t client = 0;
    uint64_t sent_at = 0;
    std::vector<uint8_t> payload;
};
uint64_t now_ms();
void append_u32(std::vector<uint8_t>& bytes, uint32_t value);
uint32_t read_u32(const uint8_t* bytes);
std::vector<uint8_t> encode(const Message& message);
Message decode(const std::vector<uint8_t>& bytes);
std::string escape(const std::vector<uint8_t>& text);
}
