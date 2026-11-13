#pragma once

#include <cstdint>
#include <iosfwd>
#include <optional>
#include <string>
#include <string_view>

namespace results {

enum class Outcome { ok, timeout, error };

struct Attempt {
    unsigned number = 0;
    std::optional<unsigned> status;
    Outcome outcome = Outcome::error;
    std::string_view stage = "socket";
    std::string error;
    std::optional<double> connect_ms;
    double attempt_ms = 0;
    std::size_t sent = 0;
    std::size_t received = 0;
    std::size_t body = 0;
    bool request_complete = false;
    bool response_complete = false;
};

struct Totals {
    unsigned requests = 0;
    unsigned responses = 0;
    unsigned successful = 0;
    unsigned errors = 0;
    unsigned timeouts = 0;
    double attempt_ms = 0;
    double successful_ms = 0;
    std::uint64_t sent = 0;
    std::uint64_t received = 0;
    std::uint64_t body = 0;
    std::uint64_t successful_body = 0;

    void add(const Attempt& attempt);
};

std::int64_t ticks();
std::int64_t frequency();
double milliseconds(std::int64_t start, std::int64_t end, std::int64_t frequency);
std::string make_run_id();
void log(const std::string& run_id, std::string_view peer, unsigned attempt, std::string_view message);
void csv_header(std::ostream& out);
void csv_row(std::ostream& out, const std::string& run_id, std::string_view host, unsigned port,
    std::string_view method, std::string_view path, const Attempt& attempt);
void summary(std::ostream& out, const Totals& totals, unsigned count, bool is_get);

}
