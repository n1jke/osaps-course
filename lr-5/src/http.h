#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

namespace http {

enum class Method { get, head };

std::string_view name(Method method);
std::string request(Method method, std::string_view path, std::string_view host, unsigned port);
std::string escape(std::string_view bytes);

class Response {
public:
    explicit Response(Method method, bool preview = false);

    void consume(std::string_view bytes);
    void finish();

    bool complete() const { return complete_; }
    std::optional<unsigned> status() const { return status_; }
    const std::string& headers() const { return headers_; }
    std::size_t body_size() const { return body_size_; }
    const std::string& preview() const { return preview_; }

private:
    void parse_headers();
    void consume_body(std::string_view bytes);

    Method method_;
    bool save_preview_;
    bool headers_done_ = false;
    bool complete_ = false;
    std::optional<unsigned> status_;
    std::optional<std::size_t> content_length_;
    std::size_t body_size_ = 0;
    std::string headers_;
    std::string preview_;
};

}
