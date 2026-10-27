#pragma once

#include "shared.h"

#include <string>

namespace sync {

struct Objects {
    HANDLE readers_mutex = nullptr;
    HANDLE writers_mutex = nullptr;
    HANDLE read_gate = nullptr;
    HANDLE resource = nullptr;
};

bool create(Objects& objects, const std::string& prefix);
bool open(Objects& objects, const std::string& prefix);
void close(Objects& objects);
bool enter_read(Objects& objects, shared::Data& data);
bool leave_read(Objects& objects, shared::Data& data);
bool enter_write(Objects& objects, shared::Data& data);
bool leave_write(Objects& objects, shared::Data& data);

} // namespace sync
