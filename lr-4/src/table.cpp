#include "table.h"

namespace table {

void read(shared::Data& data, DWORD duration_ms) {
    InterlockedIncrement(&data.active_readers);

    if (InterlockedCompareExchange(&data.active_writers, 0, 0) != 0) {
        InterlockedIncrement(&data.violations);
    }

    for (const auto& record : data.records) {
        if (record.check != shared::check_value(record.value)) {
            InterlockedIncrement(&data.violations);
        }
    }

    Sleep(duration_ms);
    InterlockedDecrement(&data.active_readers);
}

void write(shared::Data& data, std::size_t index) {
    if (InterlockedIncrement(&data.active_writers) != 1 ||
        InterlockedCompareExchange(&data.active_readers, 0, 0) != 0) {
        InterlockedIncrement(&data.violations);
    }

    auto& record = data.records[index];
    ++record.value;
    record.check = shared::check_value(record.value);

    InterlockedDecrement(&data.active_writers);
}

} // namespace table
