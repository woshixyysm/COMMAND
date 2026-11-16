#pragma once

#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace cmdpp {

struct CaseInsensitiveLess {
    bool operator()(const std::string& a, const std::string& b) const;
};

// DOS environment: names are case-insensitive and stored upper-case.
class Environment {
public:
    static Environment from_host();

    std::optional<std::string> get(std::string_view name) const;
    void set(std::string name, std::string value);
    bool erase(std::string_view name);

    // Sorted (case-insensitively) NAME/VALUE pairs.
    std::vector<std::pair<std::string, std::string>> list() const;
    std::size_t size_bytes() const;

    // Entries for a child process environment block.
    std::vector<std::string> block() const;

private:
    std::map<std::string, std::string, CaseInsensitiveLess> vars_;
};

} // namespace cmdpp
