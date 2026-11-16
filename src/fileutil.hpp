#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include "cmdpp/shell.hpp"

namespace cmdpp::detail {

struct Entry {
    std::filesystem::path path;
    std::string name;
    std::uintmax_t size = 0;
    bool is_dir = false;
    platform::FileAttrs attrs;
    std::filesystem::file_time_type mtime;
};

struct SpecPath {
    std::filesystem::path dir;
    std::string pattern;
    bool valid = false;
    std::string error;
};

std::vector<Entry> list_entries(const std::filesystem::path& dir);
std::vector<Entry> collect_entries(const std::filesystem::path& dir, const std::string& pattern);
SpecPath parse_spec(const Shell& shell, std::string_view token);

// Reads a Y/N answer; returns true for yes.
bool confirm(Shell& shell, const std::string& prompt);
// Returns 0 = no, 1 = yes, 2 = all, -1 = eof.
int choice(Shell& shell, const std::string& prompt);

} // namespace cmdpp::detail
