#include "http.h"

#include <algorithm>
#include <charconv>
#include <stdexcept>

namespace http {
namespace {

constexpr std::size_t header_limit = 32 * 1024;
constexpr std::size_t body_limit = 16 * 1024 * 1024;
constexpr std::size_t preview_limit = 256;
constexpr std::string_view header_end = "\r\n\r\n";

bool digit(char c) {
    return c >= '0' && c <= '9';
}

bool field_char(char c) {
    return digit(c) || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
        || std::string_view("!#$%&'*+-.^`_|~").find(c) != std::string_view::npos;
}

bool field_value_char(unsigned char c) {
    return c == '\t' || (c >= ' ' && c != 127);
}

bool equals_ignore_case(std::string_view a, std::string_view b) {
    return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(), [](char x, char y) {
        if (x >= 'A' && x <= 'Z') {
            x = static_cast<char>(x - 'A' + 'a');
        }
        return x == y;
    });
}

std::string_view trim(std::string_view value) {
    const auto start = value.find_first_not_of(" \t");
    if (start == std::string_view::npos) {
        return {};
    }
    return value.substr(start, value.find_last_not_of(" \t") - start + 1);
}

}

std::string_view name(Method method) {
    return method == Method::get ? "GET" : "HEAD";
}

std::string request(Method method, std::string_view path, std::string_view host, unsigned port) {
    std::string message(name(method));
    message += ' ';
    message += path;
    message += " HTTP/1.1\r\nHost: ";
    message += host;
    message += ':' + std::to_string(port) + "\r\nConnection: close\r\n\r\n";
    return message;
}

std::string escape(std::string_view bytes) {
    constexpr char hex[] = "0123456789abcdef";
    std::string text;
    for (unsigned char byte : bytes) {
        if (byte == '\\') {
            text += "\\\\";
        } else if (byte >= ' ' && byte <= '~') {
            text += static_cast<char>(byte);
        } else {
            text += "\\x";
            text += hex[byte >> 4];
            text += hex[byte & 15];
        }
    }
    return text;
}

Response::Response(Method method, bool preview)
    : method_(method), save_preview_(preview) {}

void Response::consume(std::string_view bytes) {
    if (complete_ || bytes.empty()) {
        return;
    }
    if (!headers_done_) {
        const auto previous_size = headers_.size();
        const auto added = std::min(bytes.size(), header_limit - previous_size);
        headers_.append(bytes.substr(0, added));
        const auto search_start = previous_size > 3 ? previous_size - 3 : 0;
        const auto end = headers_.find(header_end, search_start);
        if (end == std::string::npos) {
            if (headers_.size() == header_limit) {
                throw std::runtime_error("HTTP header limit exceeded");
            }
            return;
        }

        const auto size = end + header_end.size();
        headers_.resize(size);
        bytes.remove_prefix(size - previous_size);
        parse_headers();
        headers_done_ = true;
    }
    if (!complete_) {
        consume_body(bytes);
    }
}

void Response::parse_headers() {
    std::string_view lines(headers_);
    const auto line_end = lines.find("\r\n");
    const auto status_line = lines.substr(0, line_end);
    if (status_line.size() < 13 || status_line.substr(0, 9) != "HTTP/1.1 "
        || !std::all_of(status_line.begin() + 9, status_line.begin() + 12, digit)
        || status_line[12] != ' '
        || !std::all_of(status_line.begin() + 13, status_line.end(), field_value_char)) {
        throw std::runtime_error("invalid HTTP/1.1 status line");
    }
    const unsigned status = (status_line[9] - '0') * 100u
        + (status_line[10] - '0') * 10u + (status_line[11] - '0');
    if (status < 100 || status > 599) {
        throw std::runtime_error("invalid HTTP status code");
    }
    status_ = status;
    if (status < 200) {
        throw std::runtime_error("informational HTTP responses are unsupported");
    }

    const bool has_body = method_ != Method::head && status != 204 && status != 304;
    lines.remove_prefix(line_end + 2);
    while (lines != "\r\n") {
        const auto end = lines.find("\r\n");
        const auto field = lines.substr(0, end);
        lines.remove_prefix(end + 2);
        const auto colon = field.find(':');
        if (colon == 0 || colon == std::string_view::npos
            || !std::all_of(field.begin(), field.begin() + colon, field_char)
            || !std::all_of(field.begin() + colon + 1, field.end(), field_value_char)) {
            throw std::runtime_error("invalid HTTP header field");
        }
        if (!has_body) {
            continue;
        }

        const auto key = field.substr(0, colon);
        const auto value = trim(field.substr(colon + 1));
        if (equals_ignore_case(key, "transfer-encoding")) {
            throw std::runtime_error("unsupported Transfer-Encoding: " + escape(value));
        }
        if (equals_ignore_case(key, "content-encoding") && !equals_ignore_case(value, "identity")) {
            throw std::runtime_error("unsupported Content-Encoding: " + escape(value));
        }
        if (equals_ignore_case(key, "content-length")) {
            if (value.empty() || content_length_) {
                throw std::runtime_error("invalid or repeated Content-Length");
            }
            std::size_t length = 0;
            const auto end_value = value.data() + value.size();
            const auto parsed = std::from_chars(value.data(), end_value, length);
            if (parsed.ec != std::errc{} || parsed.ptr != end_value) {
                throw std::runtime_error("invalid Content-Length");
            }
            if (length > body_limit) {
                throw std::runtime_error("HTTP body limit exceeded");
            }
            content_length_ = length;
        }
    }
    complete_ = !has_body || content_length_ == 0;
}

void Response::consume_body(std::string_view bytes) {
    const auto remaining = content_length_.value_or(body_limit) - body_size_;
    const auto accepted = std::min(remaining, bytes.size());
    if (save_preview_) {
        preview_.append(bytes.substr(0, std::min(accepted, preview_limit - preview_.size())));
    }
    body_size_ += accepted;
    if (!content_length_ && bytes.size() > remaining) {
        throw std::runtime_error("HTTP body limit exceeded");
    }
    complete_ = content_length_ && body_size_ == *content_length_;
}

void Response::finish() {
    if (!headers_done_) {
        throw std::runtime_error("connection closed before complete HTTP headers");
    }
    if (!complete_ && content_length_) {
        throw std::runtime_error("connection closed before complete HTTP body");
    }
    complete_ = true;
}

}
