#include "socket_io.h"

#include <ws2tcpip.h>
#include <windows.h>

#include <string>

namespace socket_io {
namespace {

std::runtime_error error(const char* operation, int code) {
    char description[512]{};
    const auto length = FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, static_cast<DWORD>(code), 0, description, sizeof(description), nullptr);
    std::string text(description, length);
    const auto end = text.find_last_not_of(" \r\n\t");
    return std::runtime_error(std::string(operation) + " (" + std::to_string(code) + "): "
        + (end == std::string::npos ? "Winsock error" : text.substr(0, end + 1)));
}

void check_deadline(Deadline deadline, const char* operation) {
    if (std::chrono::steady_clock::now() >= deadline) {
        throw Timeout(operation);
    }
}

void wait_ready(SOCKET socket, Deadline deadline, const char* operation, bool read, bool connecting = false) {
    const auto remaining = deadline - std::chrono::steady_clock::now();
    if (remaining <= Deadline::duration::zero()) {
        throw Timeout(operation);
    }
    const auto us = std::chrono::ceil<std::chrono::microseconds>(remaining).count();
    timeval timeout{static_cast<long>(us / 1000000), static_cast<long>(us % 1000000)};

    fd_set sockets;
    FD_ZERO(&sockets);
    FD_SET(socket, &sockets);
    fd_set exceptional = sockets;

    const int ready = select(0, read ? &sockets : nullptr, read ? nullptr : &sockets,
        connecting ? &exceptional : nullptr, &timeout);
    if (ready == SOCKET_ERROR) {
        throw error("select", WSAGetLastError());
    }
    if (ready == 0) {
        throw Timeout(operation);
    }
    check_deadline(deadline, operation);
}

}

Timeout::Timeout(const char* operation)
    : std::runtime_error(std::string(operation) + " timed out") {}

Winsock::Winsock() {
    WSADATA data{};
    const int result = WSAStartup(MAKEWORD(2, 2), &data);
    if (result != 0) {
        throw error("WSAStartup", result);
    }
    if (data.wVersion != MAKEWORD(2, 2)) {
        WSACleanup();
        throw std::runtime_error("WSAStartup: Winsock 2.2 unavailable");
    }
}

Winsock::~Winsock() {
    WSACleanup();
}

sockaddr_in address(const char* host, unsigned port) {
    sockaddr_in remote{};
    remote.sin_family = AF_INET;
    remote.sin_port = htons(static_cast<u_short>(port));
    const int result = InetPtonA(AF_INET, host, &remote.sin_addr);
    if (result == 0) {
        throw std::invalid_argument("--host requires a numeric IPv4 address");
    }
    if (result == SOCKET_ERROR) {
        throw error("InetPtonA", WSAGetLastError());
    }
    return remote;
}

Socket::Socket() : handle_(socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) {
    if (handle_ == INVALID_SOCKET) {
        throw error("socket", WSAGetLastError());
    }
    u_long nonblocking = 1;
    if (ioctlsocket(handle_, FIONBIO, &nonblocking) == SOCKET_ERROR) {
        const int code = WSAGetLastError();
        closesocket(handle_);
        throw error("ioctlsocket", code);
    }
}

Socket::~Socket() {
    closesocket(handle_);
}

void Socket::connect(const sockaddr_in& remote, Deadline deadline) {
    check_deadline(deadline, "connect");
    if (::connect(handle_, reinterpret_cast<const sockaddr*>(&remote), sizeof(remote)) == SOCKET_ERROR) {
        const int code = WSAGetLastError();
        if (code != WSAEWOULDBLOCK) {
            throw error("connect", code);
        }

        wait_ready(handle_, deadline, "connect", false, true);
        int pending_error = 0;
        int size = sizeof(pending_error);
        if (getsockopt(handle_, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&pending_error), &size) == SOCKET_ERROR) {
            throw error("getsockopt(SO_ERROR)", WSAGetLastError());
        }
        if (pending_error != 0) {
            throw error("connect", pending_error);
        }
    }
    check_deadline(deadline, "connect");
}

void Socket::send_all(std::string_view data, Deadline deadline, std::size_t& sent) {
    while (sent < data.size()) {
        check_deadline(deadline, "send");
        const int count = send(handle_, data.data() + sent, static_cast<int>(data.size() - sent), 0);
        if (count == SOCKET_ERROR) {
            const int code = WSAGetLastError();
            if (code != WSAEWOULDBLOCK) {
                throw error("send", code);
            }
            wait_ready(handle_, deadline, "send", false);
        } else if (count == 0) {
            throw std::runtime_error("send made no progress");
        } else {
            sent += static_cast<std::size_t>(count);
        }
    }
}

int Socket::receive(char* buffer, int size, Deadline deadline) {
    for (;;) {
        check_deadline(deadline, "recv");
        const int count = recv(handle_, buffer, size, 0);
        if (count != SOCKET_ERROR) {
            return count;
        }
        const int code = WSAGetLastError();
        if (code != WSAEWOULDBLOCK) {
            throw error("recv", code);
        }
        wait_ready(handle_, deadline, "recv", true);
    }
}

}
