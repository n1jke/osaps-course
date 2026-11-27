#include "options.h"

#include <utility>

namespace {

bool read_number(std::wstring_view text, unsigned& value)
{
    if (text.empty() || text.size() > 3) {
        return false;
    }
    value = 0;
    for (const wchar_t digit : text) {
        if (digit < L'0' || digit > L'9') {
            return false;
        }
        value = value * 10 + static_cast<unsigned>(digit - L'0');
    }
    return true;
}

}

bool parse_options(const std::vector<std::wstring_view>& arguments, Options& result)
{
    Options parsed;
    bool hz_seen = false;
    bool seconds_seen = false;

    for (std::size_t i = 0; i < arguments.size(); i += 2) {
        if (i + 1 >= arguments.size()) {
            return false;
        }
        const auto key = arguments[i];
        const auto value = arguments[i + 1];
        if (key == L"--hz" && !hz_seen) {
            hz_seen = true;
            if (!read_number(value, parsed.hz) ||
                (parsed.hz != 10 && parsed.hz != 50 && parsed.hz != 100)) {
                return false;
            }
        } else if (key == L"--seconds" && !seconds_seen) {
            seconds_seen = true;
            if (!read_number(value, parsed.seconds) ||
                parsed.seconds < 1 || parsed.seconds > 300) {
                return false;
            }
        } else if (key == L"--csv" && parsed.csv.empty() &&
                   !value.empty() && value.substr(0, 2) != L"--") {
            parsed.csv = value;
        } else {
            return false;
        }
    }

    result = std::move(parsed);
    return true;
}
