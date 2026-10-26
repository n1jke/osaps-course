#pragma once
#include <cstdint>

namespace config {
constexpr uint32_t STUDENT_ID = 112;
constexpr uint32_t VARIANT_ID = 26;
constexpr uint32_t MESSAGE_COUNT = 1000 + STUDENT_ID;
static_assert(STUDENT_ID <= 999, "STUDENT_ID must contain the last three digits");
}
