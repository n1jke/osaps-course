#pragma once

#include <winsock2.h>

#include <chrono>
#include <stdexcept>
#include <string_view>

namespace socket_io {

using Deadline = std::chrono::steady_clock::time_point;

class Timeout : public std::runtime_error {
public:
    explicit Timeout(const char* operation);
};

class Winsock {
public:
    Winsock();
    ~Winsock();
    Winsock(const Winsock&) = delete;
    Winsock& operator=(const Winsock&) = delete;
};

sockaddr_in address(const char* host, unsigned port);

class Socket {
public:
    Socket();
    ~Socket();
    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;

    void connect(const sockaddr_in& remote, Deadline deadline);
    void send_all(std::string_view data, Deadline deadline, std::size_t& sent);
    int receive(char* buffer, int size, Deadline deadline);

private:
    SOCKET handle_;
};

}
