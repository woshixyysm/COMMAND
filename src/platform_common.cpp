#include "cmdpp/platform.hpp"

#include <ctime>

#if defined(_WIN32)
#include <io.h>
#else
#include <unistd.h>
#endif

namespace cmdpp::platform {

void write(std::FILE* f, std::string_view s) {
    if (!f || s.empty()) return;
    std::fwrite(s.data(), 1, s.size(), f);
}

void flush(std::FILE* f) {
    if (f) std::fflush(f);
}

bool is_tty(std::FILE* f) {
    if (!f) return false;
#if defined(_WIN32)
    return _isatty(_fileno(f)) != 0;
#else
    return ::isatty(::fileno(f)) != 0;
#endif
}

std::optional<std::string> read_line(std::FILE* f) {
    if (!f) return std::nullopt;
    std::string line;
    int c = 0;
    bool any = false;
    while ((c = std::fgetc(f)) != EOF) {
        any = true;
        if (c == 0x1A) { // ^Z terminates console/file input
            if (line.empty()) return std::nullopt;
            break;
        }
        if (c == '\n') break;
        line.push_back(static_cast<char>(c));
    }
    if (c == EOF && !any) return std::nullopt;
    if (!line.empty() && line.back() == '\r') line.pop_back();
    return line;
}

static std::tm to_tm(const Clock& c) {
    std::tm t{};
    t.tm_year = c.year - 1900;
    t.tm_mon = c.month - 1;
    t.tm_mday = c.day;
    t.tm_hour = c.hour;
    t.tm_min = c.minute;
    t.tm_sec = c.second;
    t.tm_isdst = -1;
    return t;
}

static Clock from_tm(const std::tm& t) {
    Clock c;
    c.year = t.tm_year + 1900;
    c.month = t.tm_mon + 1;
    c.day = t.tm_mday;
    c.hour = t.tm_hour;
    c.minute = t.tm_min;
    c.second = t.tm_sec;
    c.weekday = t.tm_wday;
    return c;
}

Clock local_clock() {
    std::time_t now = std::time(nullptr);
    std::tm t{};
#if defined(_WIN32)
    localtime_s(&t, &now);
#else
    localtime_r(&now, &t);
#endif
    return from_tm(t);
}

int64_t epoch_seconds(const Clock& c) {
    std::tm t = to_tm(c);
    return static_cast<int64_t>(std::mktime(&t));
}

Clock from_epoch(int64_t seconds) {
    std::time_t t = static_cast<std::time_t>(seconds);
    std::tm out{};
#if defined(_WIN32)
    localtime_s(&out, &t);
#else
    localtime_r(&t, &out);
#endif
    return from_tm(out);
}

} // namespace cmdpp::platform
