#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "cmdpp/platform.hpp"

namespace cmdpp::text {

std::string upper(std::string_view s);
std::string lower(std::string_view s);
bool iequals(std::string_view a, std::string_view b);
bool istarts_with(std::string_view s, std::string_view prefix);

bool is_delim(char c); // space, tab, comma, semicolon, equals
std::string_view trim(std::string_view s);
std::string_view trim_right(std::string_view s);
std::size_t skip_delims(std::string_view s, std::size_t i);

// Quote aware tokenizer: delimiters separate tokens, double quotes group text
// and are removed (DOS parser behaviour).
std::vector<std::string> split_args(std::string_view s);

// Split on space/tab/comma/semicolon only (used for FOR lists and names).
std::vector<std::string> split_list(std::string_view s);

std::string join(const std::vector<std::string>& parts, char sep = ' ');

// DOS 8.3 style wildcard match (? and *), case-insensitive.
bool wildcard_match(std::string_view pattern, std::string_view name);
bool has_wildcard(std::string_view s);

// Split an 8.3 style name at its first dot.
void split_name(std::string_view name, std::string_view& base, std::string_view& ext);

// Rename helper: apply the wildcard pattern of `pattern` to `name`, taking
// literal characters from `target`.
std::string wildcard_transform(std::string_view name, std::string_view pattern, std::string_view target);

std::string number_with_commas(uint64_t value);

// "Mon 11-16-2026" (used by $D and DATE).
std::string date_text(const platform::Clock& c);
// "12:39:31.45" (used by $T and TIME).
std::string time_text(const platform::Clock& c);
// "11-16-2026"
std::string short_date(const platform::Clock& c);
// "11-16-26"
std::string short_date2(const platform::Clock& c);
// "11:11a"
std::string file_time_text(const platform::Clock& c);

} // namespace cmdpp::text
