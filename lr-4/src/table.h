#pragma once

#include "shared.h"

#include <cstddef>

namespace table {

// Call only while holding the appropriate reader or writer access.
void read(shared::Data& data, DWORD duration_ms);
void write(shared::Data& data, std::size_t index);

} // namespace table
