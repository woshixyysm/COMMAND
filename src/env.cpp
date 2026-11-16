#include "cmdpp/env.hpp"

#include <cctype>

#if defined(_WIN32)
#include <cstdlib>
#define CMD_ENVIRON _environ
#else
extern char** environ;
#define CMD_ENVIRON environ
#endif

namespace cmdpp {
namespace {

std::string upper_copy(std::string_view s) {
    std::string r(s);
    for (char& c : r) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return r;
}

#if !defined(_WIN32)
// The shell speaks DOS PATH syntax internally; children need POSIX syntax.
std::string to_dos_path(std::string value) {
    if (value.find(';') == std::string::npos)
        for (char& c : value)
            if (c == ':') c = ';';
    return value;
}

std::string to_posix_path(const std::string& value) {
    std::string out = value;
    for (char& c : out)
        if (c == ';') c = ':';
    return out;
}
#endif

} // namespace

bool CaseInsensitiveLess::operator()(const std::string& a, const std::string& b) const {
    return upper_copy(a) < upper_copy(b);
}

Environment Environment::from_host() {
    Environment env;
    char** p = CMD_ENVIRON;
    if (!p) return env;
    for (; *p; ++p) {
        std::string entry(*p);
        auto eq = entry.find('=');
        if (eq == std::string::npos || eq == 0) continue;
        std::string name = entry.substr(0, eq);
        std::string value = entry.substr(eq + 1);
#if !defined(_WIN32)
        if (upper_copy(name) == "PATH") value = to_dos_path(std::move(value));
#endif
        env.vars_[name] = std::move(value);
    }
    return env;
}

std::optional<std::string> Environment::get(std::string_view name) const {
    auto it = vars_.find(std::string(name));
    if (it == vars_.end()) return std::nullopt;
    return it->second;
}

void Environment::set(std::string name, std::string value) {
    if (name.empty() || name.find('=') != std::string::npos) return;
    vars_.erase(name);
    vars_.emplace(upper_copy(name), std::move(value));
}

bool Environment::erase(std::string_view name) {
    return vars_.erase(std::string(name)) > 0;
}

std::vector<std::pair<std::string, std::string>> Environment::list() const {
    return {vars_.begin(), vars_.end()};
}

std::size_t Environment::size_bytes() const {
    std::size_t n = 1;
    for (const auto& kv : vars_) n += kv.first.size() + kv.second.size() + 2;
    return n;
}

std::vector<std::string> Environment::block() const {
    std::vector<std::string> out;
    out.reserve(vars_.size());
    for (const auto& kv : vars_) {
        std::string value = kv.second;
#if !defined(_WIN32)
        if (upper_copy(kv.first) == "PATH") value = to_posix_path(value);
#endif
        out.push_back(kv.first + "=" + value);
    }
    return out;
}

} // namespace cmdpp
