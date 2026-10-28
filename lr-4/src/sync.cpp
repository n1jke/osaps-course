#include "sync.h"

#include <initializer_list>
#include <iostream>

namespace sync {
namespace {

bool wait(HANDLE handle, const char* name, shared::Data& data) {
    const DWORD result = WaitForSingleObject(handle, shared::wait_ms);
    if (result == WAIT_OBJECT_0) {
        return true;
    }
    if (result == WAIT_FAILED) {
        std::cerr << name << ": wait failed, error=" << GetLastError() << '\n';
    } else if (result == WAIT_ABANDONED) {
        std::cerr << name << ": mutex abandoned\n";
        ReleaseMutex(handle);
    } else {
        InterlockedIncrement(&data.timeouts);
        std::cerr << name << ": wait timed out\n";
    }
    return false;
}

bool release_mutex(HANDLE handle) {
    if (!ReleaseMutex(handle)) {
        std::cerr << "ReleaseMutex failed, error=" << GetLastError() << '\n';
        return false;
    }
    return true;
}

bool release_semaphore(HANDLE handle) {
    if (!ReleaseSemaphore(handle, 1, nullptr)) {
        std::cerr << "ReleaseSemaphore failed, error=" << GetLastError() << '\n';
        return false;
    }
    return true;
}

} // namespace

bool create(Objects& o, const std::string& prefix) {
    auto mutex = [&](HANDLE& handle, const char* suffix) {
        handle = CreateMutexA(nullptr, FALSE, (prefix + suffix).c_str());
        if (!handle) {
            std::cerr << "CreateMutex failed, error=" << GetLastError() << '\n';
            return false;
        }
        if (GetLastError() == ERROR_ALREADY_EXISTS) {
            std::cerr << "synchronization object already exists\n";
            return false;
        }
        return true;
    };
    auto semaphore = [&](HANDLE& handle, const char* suffix) {
        handle = CreateSemaphoreA(nullptr, 1, 1, (prefix + suffix).c_str());
        if (!handle) {
            std::cerr << "CreateSemaphore failed, error=" << GetLastError() << '\n';
            return false;
        }
        if (GetLastError() == ERROR_ALREADY_EXISTS) {
            std::cerr << "synchronization object already exists\n";
            return false;
        }
        return true;
    };
    return mutex(o.readers_mutex, "_readers") && mutex(o.writers_mutex, "_writers") &&
           semaphore(o.read_gate, "_gate") && semaphore(o.resource, "_resource");
}

bool open(Objects& o, const std::string& prefix) {
    o.readers_mutex = OpenMutexA(SYNCHRONIZE | MUTEX_MODIFY_STATE, FALSE, (prefix + "_readers").c_str());
    if (!o.readers_mutex) { std::cerr << "OpenMutex readers failed, error=" << GetLastError() << '\n'; return false; }
    o.writers_mutex = OpenMutexA(SYNCHRONIZE | MUTEX_MODIFY_STATE, FALSE, (prefix + "_writers").c_str());
    if (!o.writers_mutex) { std::cerr << "OpenMutex writers failed, error=" << GetLastError() << '\n'; return false; }
    o.read_gate = OpenSemaphoreA(SYNCHRONIZE | SEMAPHORE_MODIFY_STATE, FALSE, (prefix + "_gate").c_str());
    if (!o.read_gate) { std::cerr << "OpenSemaphore gate failed, error=" << GetLastError() << '\n'; return false; }
    o.resource = OpenSemaphoreA(SYNCHRONIZE | SEMAPHORE_MODIFY_STATE, FALSE, (prefix + "_resource").c_str());
    if (!o.resource) { std::cerr << "OpenSemaphore resource failed, error=" << GetLastError() << '\n'; return false; }
    return true;
}

void close(Objects& o) {
    for (HANDLE handle : {o.readers_mutex, o.writers_mutex, o.read_gate, o.resource}) {
        if (handle && !CloseHandle(handle)) {
            std::cerr << "CloseHandle failed, error=" << GetLastError() << '\n';
        }
    }
    o = {};
}

bool enter_read(Objects& o, shared::Data& d) {
    if (!wait(o.read_gate, "read gate", d)) return false;
    if (!wait(o.readers_mutex, "readers mutex", d)) {
        release_semaphore(o.read_gate);
        return false;
    }
    ++d.readers;
    if (d.readers == 1 && !wait(o.resource, "resource", d)) {
        --d.readers;
        release_mutex(o.readers_mutex);
        release_semaphore(o.read_gate);
        return false;
    }
    const bool mutex_ok = release_mutex(o.readers_mutex);
    const bool gate_ok = release_semaphore(o.read_gate);
    return mutex_ok && gate_ok;
}

bool leave_read(Objects& o, shared::Data& d) {
    if (!wait(o.readers_mutex, "readers mutex", d)) return false;
    --d.readers;
    bool ok = true;
    if (d.readers == 0) ok = release_semaphore(o.resource);
    return release_mutex(o.readers_mutex) && ok;
}

bool enter_write(Objects& o, shared::Data& d) {
    if (!wait(o.writers_mutex, "writers mutex", d)) return false;
    ++d.writers;
    if (d.writers == 1 && !wait(o.read_gate, "read gate", d)) {
        --d.writers;
        release_mutex(o.writers_mutex);
        return false;
    }
    if (!release_mutex(o.writers_mutex)) return false;
    if (wait(o.resource, "resource", d)) return true;

    // Undo registration after a timeout; the whole experiment still fails.
    if (wait(o.writers_mutex, "writers mutex", d)) {
        --d.writers;
        if (d.writers == 0) release_semaphore(o.read_gate);
        release_mutex(o.writers_mutex);
    }
    return false;
}

bool leave_write(Objects& o, shared::Data& d) {
    if (!release_semaphore(o.resource)) return false;
    if (!wait(o.writers_mutex, "writers mutex", d)) return false;
    --d.writers;
    bool ok = true;
    if (d.writers == 0) ok = release_semaphore(o.read_gate);
    return release_mutex(o.writers_mutex) && ok;
}

} // namespace sync
