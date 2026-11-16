#include "cmdpp/text.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>

namespace cmdpp::text {
namespace {

void to_fcb(std::string_view s, char out[11], bool pattern) {
    std::fill(out, out + 11, ' ');
    auto dot = s.find('.');
    std::string_view base = (dot == std::string_view::npos) ? s : s.substr(0, dot);
    std::string_view ext = (dot == std::string_view::npos) ? std::string_view{} : s.substr(dot + 1);

    std::size_t bi = 0;
    for (char c : base) {
        if (bi >= 8) break;
        if (pattern && c == '*') {
            while (bi < 8) out[bi++] = '?';
            break;
        }
        out[bi++] = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }
    std::size_t ei = 0;
    for (char c : ext) {
        if (ei >= 3) break;
        if (pattern && c == '*') {
            while (ei < 3) out[8 + ei++] = '?';
            break;
        }
        out[8 + ei++] = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }
    // DOS rule: a trailing '*' in the name part also matches any extension.
    if (pattern && dot == std::string_view::npos && !base.empty() && base.back() == '*') {
        out[8] = out[9] = out[10] = '?';
    }
}

bool fcb_match(const char pat[11], const char name[11]) {
    for (int i = 0; i < 11; ++i) {
        if (pat[i] == '?') continue;
        if (pat[i] != name[i]) return false;
    }
    return true;
}

std::string transform_part(std::string_view src, std::string_view target) {
    std::string out;
    std::size_t si = 0;
    for (char tc : target) {
        if (tc == '*') {
            while (si < src.size()) out.push_back(src[si++]);
        } else if (tc == '?') {
            if (si < src.size()) out.push_back(src[si++]);
        } else {
            out.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(tc))));
        }
    }
    return out;
}

} // namespace

std::string upper(std::string_view s) {
    std::string r(s);
    for (char& c : r) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return r;
}

std::string lower(std::string_view s) {
    std::string r(s);
    for (char& c : r) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return r;
}

bool iequals(std::string_view a, std::string_view b) {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i)
        if (std::toupper(static_cast<unsigned char>(a[i])) != std::toupper(static_cast<unsigned char>(b[i])))
            return false;
    return true;
}

bool istarts_with(std::string_view s, std::string_view prefix) {
    if (s.size() < prefix.size()) return false;
    return iequals(s.substr(0, prefix.size()), prefix);
}

bool is_delim(char c) {
    return c == ' ' || c == '\t' || c == ',' || c == ';' || c == '=';
}

std::string_view trim(std::string_view s) {
    std::size_t b = 0;
    std::size_t e = s.size();
    while (b < e && (s[b] == ' ' || s[b] == '\t')) ++b;
    while (e > b && (s[e - 1] == ' ' || s[e - 1] == '\t')) --e;
    return s.substr(b, e - b);
}

std::string_view trim_right(std::string_view s) {
    std::size_t e = s.size();
    while (e > 0 && (s[e - 1] == ' ' || s[e - 1] == '\t')) --e;
    return s.substr(0, e);
}

std::size_t skip_delims(std::string_view s, std::size_t i) {
    while (i < s.size() && is_delim(s[i])) ++i;
    return i;
}

std::vector<std::string> split_args(std::string_view s) {
    std::vector<std::string> out;
    std::string cur;
    bool quoted = false;
    bool have = false;
    for (char c : s) {
        if (c == '"') {
            quoted = !quoted;
            have = true;
            continue;
        }
        if (!quoted && is_delim(c)) {
            if (have) {
                out.push_back(cur);
                cur.clear();
                have = false;
            }
            continue;
        }
        cur.push_back(c);
        have = true;
    }
    if (have) out.push_back(cur);
    return out;
}

std::vector<std::string> split_list(std::string_view s) {
    std::vector<std::string> out;
    std::string cur;
    bool have = false;
    for (char c : s) {
        if (c == ' ' || c == '\t' || c == ',' || c == ';') {
            if (have) {
                out.push_back(cur);
                cur.clear();
                have = false;
            }
            continue;
        }
        cur.push_back(c);
        have = true;
    }
    if (have) out.push_back(cur);
    return out;
}

std::string join(const std::vector<std::string>& parts, char sep) {
    std::string out;
    for (std::size_t i = 0; i < parts.size(); ++i) {
        if (i) out.push_back(sep);
        out += parts[i];
    }
    return out;
}

bool has_wildcard(std::string_view s) {
    return s.find('*') != std::string_view::npos || s.find('?') != std::string_view::npos;
}

bool wildcard_match(std::string_view pattern, std::string_view name) {
    char pat[11];
    char nam[11];
    to_fcb(pattern, pat, true);
    to_fcb(name, nam, false);
    return fcb_match(pat, nam);
}

void split_name(std::string_view name, std::string_view& base, std::string_view& ext) {
    auto dot = name.find('.');
    if (dot == std::string_view::npos) {
        base = name;
        ext = {};
    } else {
        base = name.substr(0, dot);
        ext = name.substr(dot + 1);
    }
}

std::string wildcard_transform(std::string_view name, std::string_view pattern, std::string_view target) {
    std::string_view nb, ne, pb, pe, tb, te;
    split_name(name, nb, ne);
    split_name(pattern, pb, pe);
    split_name(target, tb, te);
    (void)pb;
    (void)pe;
    std::string out = transform_part(nb, tb);
    std::string ext = transform_part(ne, te);
    if (!ext.empty() || target.find('.') != std::string_view::npos) {
        out.push_back('.');
        out += ext;
    }
    return out;
}

std::string number_with_commas(uint64_t value) {
    std::string digits = std::to_string(value);
    std::string out;
    int count = 0;
    for (auto it = digits.rbegin(); it != digits.rend(); ++it) {
        if (count && count % 3 == 0) out.push_back(',');
        out.push_back(*it);
        ++count;
    }
    std::reverse(out.begin(), out.end());
    return out;
}

std::string short_date(const platform::Clock& c) {
    char buf[24];
    std::snprintf(buf, sizeof buf, "%02d-%02d-%04d", c.month, c.day, c.year);
    return buf;
}

std::string short_date2(const platform::Clock& c) {
    char buf[24];
    std::snprintf(buf, sizeof buf, "%02d-%02d-%02d", c.month, c.day, c.year % 100);
    return buf;
}

static const char* const kDays[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};

std::string date_text(const platform::Clock& c) {
    int wd = c.weekday;
    if (wd < 0 || wd > 6) wd = 0;
    return std::string(kDays[wd]) + " " + short_date(c);
}

std::string time_text(const platform::Clock& c) {
    char buf[32];
    std::snprintf(buf, sizeof buf, "%02d:%02d:%02d.%02d", c.hour, c.minute, c.second, c.hundredths);
    return buf;
}

std::string file_time_text(const platform::Clock& c) {
    int hour = c.hour % 12;
    if (hour == 0) hour = 12;
    char buf[24];
    std::snprintf(buf, sizeof buf, "%02d:%02d%c", hour, c.minute, c.hour < 12 ? 'a' : 'p');
    return buf;
}

} // namespace cmdpp::text
