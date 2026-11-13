#include "results.h"

#include <windows.h>

#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>

namespace results {
namespace {

std::string_view name(Outcome outcome) {
    switch (outcome) {
    case Outcome::ok: return "ok";
    case Outcome::timeout: return "timeout";
    default: return "error";
    }
}

void quote(std::ostream& out, std::string_view value) {
    out << '"';
    for (char c : value) {
        if (c == '"') {
            out << '"';
        }
        out << c;
    }
    out << '"';
}

}

std::int64_t ticks() {
    LARGE_INTEGER value{};
    QueryPerformanceCounter(&value);
    return value.QuadPart;
}

std::int64_t frequency() {
    LARGE_INTEGER value{};
    if (!QueryPerformanceFrequency(&value) || value.QuadPart <= 0) {
        throw std::runtime_error("QueryPerformanceFrequency failed");
    }
    return value.QuadPart;
}

double milliseconds(std::int64_t start, std::int64_t end, std::int64_t frequency) {
    return static_cast<double>(end - start) * 1000 / frequency;
}

std::string make_run_id() {
    return std::to_string(GetCurrentProcessId()) + "-" + std::to_string(ticks());
}

void log(const std::string& run_id, std::string_view peer, unsigned attempt, std::string_view message) {
    SYSTEMTIME now{};
    GetLocalTime(&now);
    std::ostringstream line;
    line << std::setfill('0') << std::setw(2) << now.wHour << ':' << std::setw(2) << now.wMinute
        << ':' << std::setw(2) << now.wSecond << '.' << std::setw(3) << now.wMilliseconds
        << " CLIENT pid=" << GetCurrentProcessId() << " run_id=" << run_id
        << " peer=" << peer << " attempt=" << attempt << ' ' << message;
    std::cerr << line.str() << '\n';
}

void csv_header(std::ostream& out) {
    out << "run_id,attempt,host,port,method,path,http_status,outcome,error_stage,connect_ms,attempt_ms,"
           "sent_http_bytes,received_http_bytes,body_bytes,request_complete,response_complete\n";
}

void csv_row(std::ostream& out, const std::string& run_id, std::string_view host, unsigned port,
    std::string_view method, std::string_view path, const Attempt& attempt) {
    out << run_id << ',' << attempt.number << ',' << host << ',' << port << ',' << method << ',';
    quote(out, path);
    out << ',';
    if (attempt.status) {
        out << *attempt.status;
    }
    out << ',' << name(attempt.outcome) << ',' << attempt.stage << ',' << std::fixed << std::setprecision(6);
    if (attempt.connect_ms) {
        out << *attempt.connect_ms;
    }
    out << ',' << attempt.attempt_ms << ',' << attempt.sent << ',' << attempt.received << ',' << attempt.body
        << ',' << static_cast<int>(attempt.request_complete) << ',' << static_cast<int>(attempt.response_complete) << '\n';
}

void Totals::add(const Attempt& attempt) {
    requests += attempt.request_complete;
    responses += attempt.response_complete;
    attempt_ms += attempt.attempt_ms;
    sent += attempt.sent;
    received += attempt.received;
    body += attempt.body;
    switch (attempt.outcome) {
    case Outcome::ok:
        ++successful;
        successful_ms += attempt.attempt_ms;
        successful_body += attempt.body;
        break;
    case Outcome::timeout:
        ++timeouts;
        break;
    case Outcome::error:
        ++errors;
        break;
    }
}

void summary(std::ostream& out, const Totals& totals, unsigned count, bool is_get) {
    out << "attempts=" << count << " requests=" << totals.requests << " responses=" << totals.responses
        << " successful=" << totals.successful << " errors=" << totals.errors << " timeouts=" << totals.timeouts
        << " sent_http_bytes=" << totals.sent << " received_http_bytes=" << totals.received
        << " body_bytes=" << totals.body << std::fixed << std::setprecision(3)
        << " mean_attempt_ms=" << totals.attempt_ms / count;
    if (is_get && totals.successful_ms > 0) {
        out << " successful_get_body_bytes_per_second=" << totals.successful_body * 1000.0 / totals.successful_ms;
    }
    out << '\n';
}

}
