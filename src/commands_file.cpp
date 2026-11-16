#include "fileutil.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <system_error>

#include "cmdpp/text.hpp"

namespace cmdpp::detail {

std::vector<Entry> list_entries(const std::filesystem::path& dir) {
    std::vector<Entry> out;
    std::error_code ec;
    std::filesystem::directory_iterator it(dir, ec);
    std::filesystem::directory_iterator end;
    if (ec) return out;
    while (it != end) {
        std::error_code fec;
        Entry entry;
        entry.path = it->path();
        entry.name = entry.path.filename().string();
        if (!entry.name.empty()) {
            entry.is_dir = it->is_directory(fec);
            fec.clear();
            if (!entry.is_dir) {
                entry.size = static_cast<std::uintmax_t>(it->file_size(fec));
                if (fec) entry.size = 0;
            }
            entry.attrs = platform::file_attributes(entry.path);
            entry.mtime = it->last_write_time(fec);
            out.push_back(std::move(entry));
        }
        it.increment(ec);
        if (ec) break;
    }
    return out;
}

std::vector<Entry> collect_entries(const std::filesystem::path& dir, const std::string& pattern) {
    std::vector<Entry> out;
    for (auto& entry : list_entries(dir)) {
        if (pattern == "*.*" || pattern == "*" || text::wildcard_match(pattern, entry.name))
            out.push_back(std::move(entry));
    }
    return out;
}

SpecPath parse_spec(const Shell& shell, std::string_view token) {
    SpecPath spec;
    std::string raw(token);
    if (raw.size() >= 2 && raw.front() == '"' && raw.back() == '"') raw = raw.substr(1, raw.size() - 2);
    bool bad = false;
    auto path = shell.resolve_path(raw, bad);
    if (bad) {
        spec.error = "Invalid drive specification";
        return spec;
    }
    std::error_code ec;
    const bool exists = std::filesystem::exists(path, ec);
    const bool is_dir = exists && std::filesystem::is_directory(path, ec);
    const bool trailing = !raw.empty() && (raw.back() == '\\' || raw.back() == '/');
    const std::string leaf = path.filename().string();

    if (trailing) {
        spec.dir = path;
        spec.pattern = "*.*";
        if (!std::filesystem::is_directory(path, ec)) {
            spec.error = "Invalid directory";
            return spec;
        }
        spec.valid = true;
        return spec;
    }
    if (leaf.empty()) {
        spec.dir = path;
        spec.pattern = "*.*";
        if (!std::filesystem::is_directory(path, ec)) {
            spec.error = "Invalid directory";
            return spec;
        }
        spec.valid = true;
        return spec;
    }
    if (is_dir && !text::has_wildcard(leaf)) {
        spec.dir = path;
        spec.pattern = "*.*";
        spec.valid = true;
        return spec;
    }
    spec.dir = path.parent_path();
    if (spec.dir.empty()) spec.dir = std::filesystem::current_path();
    spec.pattern = leaf;
    if (!std::filesystem::is_directory(spec.dir, ec)) {
        spec.error = "Invalid path or file name";
        return spec;
    }
    spec.valid = true;
    return spec;
}

bool confirm(Shell& shell, const std::string& prompt) {
    int result = choice(shell, prompt);
    return result == 1 || result == 2;
}

int choice(Shell& shell, const std::string& prompt) {
    std::FILE* out = shell.out_stream();
    platform::write(out, prompt);
    platform::write(out, " ");
    platform::flush(out);
    auto line = platform::read_line(shell.in());
    if (!line) return -1;
    auto trimmed = text::trim(*line);
    if (trimmed.empty()) return 0;
    switch (std::toupper(static_cast<unsigned char>(trimmed[0]))) {
        case 'Y': return 1;
        case 'A': return 2;
        default: return 0;
    }
}

} // namespace cmdpp::detail

namespace cmdpp {
namespace {

using detail::Entry;

std::string right_align(const std::string& text, std::size_t width) {
    if (text.size() >= width) return text;
    return std::string(width - text.size(), ' ') + text;
}

platform::Clock clock_of(std::filesystem::file_time_type tp) {
    using namespace std::chrono;
    auto system_time =
        time_point_cast<system_clock::duration>(tp - std::filesystem::file_time_type::clock::now() + system_clock::now());
    std::time_t tt = system_clock::to_time_t(system_time);
    std::tm t{};
#if defined(_WIN32)
    localtime_s(&t, &tt);
#else
    localtime_r(&tt, &t);
#endif
    platform::Clock c;
    c.year = t.tm_year + 1900;
    c.month = t.tm_mon + 1;
    c.day = t.tm_mday;
    c.hour = t.tm_hour;
    c.minute = t.tm_min;
    c.second = t.tm_sec;
    c.weekday = t.tm_wday;
    return c;
}

std::string name_cell(const Entry& entry, bool lower) {
    std::string name = lower ? text::lower(entry.name) : entry.name;
    if (name == "." || name == "..") {
        name.resize(12, ' ');
        return name;
    }
    std::string_view base, ext;
    text::split_name(name, base, ext);
    if (base.size() <= 8 && ext.size() <= 3 && name.size() <= 12) {
        std::string cell(base);
        cell.resize(8, ' ');
        cell.push_back(' ');
        std::string extension(ext);
        extension.resize(3, ' ');
        cell += extension;
        return cell;
    }
    if (name.size() < 12) name.resize(12, ' ');
    return name;
}

struct DirOptions {
    bool wide = false;
    bool bare = false;
    bool recurse = false;
    bool lower = false;
    bool verbose = false;
    bool pause = false;
    char sort = 'N';
    bool reverse = false;
    bool dirs_first = false;
    bool filter_set = false;
    bool show_all = false;
    std::vector<std::pair<char, bool>> attrs; // letter, negated
};

bool attr_value(const Entry& entry, char letter) {
    switch (letter) {
        case 'D': return entry.is_dir;
        case 'R': return entry.attrs.readonly;
        case 'H': return entry.attrs.hidden;
        case 'S': return entry.attrs.system;
        case 'A': return entry.attrs.archive;
        default: return false;
    }
}

std::string attr_string(const Entry& entry) {
    std::string s;
    s.push_back(entry.is_dir ? 'D' : ' ');
    s.push_back(entry.attrs.readonly ? 'R' : '-');
    s.push_back(entry.attrs.hidden ? 'H' : '-');
    s.push_back(entry.attrs.system ? 'S' : '-');
    s.push_back(entry.attrs.archive ? 'A' : '-');
    return s;
}

bool passes_filter(const DirOptions& opt, const Entry& entry) {
    if (!opt.filter_set) return !entry.attrs.hidden && !entry.attrs.system;
    if (opt.show_all) return true;
    for (const auto& [letter, negated] : opt.attrs) {
        const bool value = attr_value(entry, letter);
        if (value == negated) return false;
    }
    return true;
}

bool is_switch_token(std::string_view token) {
    return !token.empty() && (token[0] == '/' || token[0] == '-');
}

} // namespace

void Shell::cmd_dir(const Invocation& inv) {
    DirOptions opt;
    std::vector<std::string> specs;

    std::vector<std::string> tokens;
    if (auto dir_cmd = env_.get("DIRCMD")) {
        auto preset = text::split_args(*dir_cmd);
        auto disabled = [&](char letter) {
            for (const auto& t : inv.tokens) {
                if (t.size() >= 2 && t[0] == '-' && std::toupper(static_cast<unsigned char>(t[1])) == letter)
                    return true;
            }
            return false;
        };
        for (auto& p : preset) {
            if (p.size() >= 2 && is_switch_token(p) &&
                disabled(std::toupper(static_cast<unsigned char>(p[1]))))
                continue;
            tokens.push_back(p);
        }
    }
    tokens.insert(tokens.end(), inv.tokens.begin(), inv.tokens.end());

    bool bad_switch = false;
    for (const auto& token : tokens) {
        if (!is_switch_token(token)) {
            specs.push_back(token);
            continue;
        }
        const bool negated = token[0] == '-';
        std::string body = token.substr(1);
        const char option = body.empty() ? '\0' : static_cast<char>(std::toupper(static_cast<unsigned char>(body[0])));
        std::string rest = body.size() > 1 ? body.substr(1) : std::string();
        if (!rest.empty() && rest[0] == ':') rest = rest.substr(1);
        switch (option) {
            case 'P': opt.pause = !negated; break;
            case 'W': opt.wide = !negated; break;
            case 'S': opt.recurse = !negated; break;
            case 'B': opt.bare = !negated; break;
            case 'L': opt.lower = !negated; break;
            case 'V': opt.verbose = !negated; break;
            case 'A': {
                opt.filter_set = true;
                if (rest.empty()) {
                    opt.show_all = !negated;
                } else {
                    bool negated_letter = false;
                    for (char c : rest) {
                        if (c == '-') {
                            negated_letter = true;
                            continue;
                        }
                        const char up = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
                        if (up == 'D' || up == 'R' || up == 'H' || up == 'S' || up == 'A')
                            opt.attrs.push_back({up, negated_letter});
                        negated_letter = false;
                    }
                }
                break;
            }
            case 'O': {
                if (rest.empty()) {
                    opt.sort = 'N';
                    break;
                }
                bool reverse = false;
                for (char c : rest) {
                    if (c == '-') {
                        reverse = true;
                        continue;
                    }
                    const char up = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
                    if (up == 'G') {
                        opt.dirs_first = !reverse;
                    } else if (up == 'N' || up == 'E' || up == 'S' || up == 'D' || up == 'A') {
                        opt.sort = (up == 'A') ? 'D' : up;
                        opt.reverse = reverse;
                    }
                    reverse = false;
                }
                break;
            }
            default:
                bad_switch = true;
                break;
        }
    }
    if (bad_switch) {
        message("Invalid switch");
        return;
    }
    if (specs.empty()) specs.push_back("*.*");

    int page_left = 25;
    auto emit = [&](const std::string& line) {
        out_line(line);
        if (opt.pause) {
            if (--page_left <= 0) {
                out("Press any key to continue . . .");
                platform::flush(out_);
                platform::read_key(in_);
                out_line();
                page_left = 25;
            }
        }
    };

    bool printed_volume = false;
    std::uintmax_t total_files = 0;
    std::uintmax_t total_bytes = 0;
    bool any_files = false;

    auto sort_entries = [&](std::vector<Entry>& entries) {
        auto icompare = [](std::string_view x, std::string_view y) {
            const std::size_t common = std::min(x.size(), y.size());
            for (std::size_t i = 0; i < common; ++i) {
                const int ca = std::toupper(static_cast<unsigned char>(x[i]));
                const int cb = std::toupper(static_cast<unsigned char>(y[i]));
                if (ca != cb) return ca < cb ? -1 : 1;
            }
            if (x.size() == y.size()) return 0;
            return x.size() < y.size() ? -1 : 1;
        };
        auto extension_of = [](std::string_view name) {
            auto dot = name.find_last_of('.');
            return dot == std::string_view::npos ? std::string_view{} : name.substr(dot + 1);
        };
        std::sort(entries.begin(), entries.end(), [&](const Entry& a, const Entry& b) {
            int cmp = 0;
            if (opt.dirs_first && a.is_dir != b.is_dir) {
                cmp = a.is_dir ? -1 : 1;
            } else if (opt.sort == 'S') {
                cmp = a.size < b.size ? -1 : (a.size > b.size ? 1 : 0);
            } else if (opt.sort == 'D') {
                cmp = a.mtime < b.mtime ? -1 : (b.mtime < a.mtime ? 1 : 0);
            } else if (opt.sort == 'E') {
                cmp = icompare(extension_of(a.name), extension_of(b.name));
                if (cmp == 0) cmp = icompare(a.name, b.name);
            } else {
                cmp = icompare(a.name, b.name);
            }
            return opt.reverse ? cmp > 0 : cmp < 0;
        });
    };

    auto emit_listing = [&](const std::filesystem::path& dir, const std::string& pattern, bool header_dir,
                            bool& found) {
        std::vector<Entry> entries = detail::collect_entries(dir, pattern);
        std::vector<Entry> filtered;
        for (auto& entry : entries) {
            if (passes_filter(opt, entry) || entry.name == "." || entry.name == "..") filtered.push_back(entry);
        }
        if (!opt.bare && (pattern == "*.*" || pattern == "*")) {
            Entry dot;
            dot.name = ".";
            dot.is_dir = true;
            dot.mtime = std::filesystem::file_time_type::clock::now();
            Entry dotdot = dot;
            dotdot.name = "..";
            filtered.push_back(dot);
            filtered.push_back(dotdot);
        }
        if (filtered.empty()) return;
        found = true;

        if (!printed_volume && !opt.bare) {
            auto info = platform::volume_info(platform::drive_letter() + ":");
            const std::string drive = platform::drive_letter();
            emit(" Volume in drive " + drive + (info.has_label ? " is " + info.label : " has no label"));
            if (info.has_serial) emit(" Volume Serial Number is " + info.serial);
            printed_volume = true;
        }
        if (header_dir && !opt.bare) {
            std::string text = display_path(dir);
            if (text.size() == 2) text.push_back('\\');
            emit("");
            emit(" Directory of " + text);
            emit("");
        }

        sort_entries(filtered);

        std::uintmax_t file_count = 0;
        std::uintmax_t byte_count = 0;

        if (opt.verbose && !opt.bare) {
            emit("File Name         Size          Allocated      Modified      Accessed  Attrib");
        }

        if (opt.wide) {
            std::string row;
            int column = 0;
            for (std::size_t index = 0; index < filtered.size(); ++index) {
                const Entry& entry = filtered[index];
                std::string name = opt.lower ? text::lower(entry.name) : entry.name;
                if (entry.is_dir) name = "[" + name + "]";
                if (name.size() < 12) name.resize(12, ' ');
                row += name;
                if (++column == 5) {
                    row.erase(row.find_last_not_of(' ') + 1);
                    emit(row);
                    row.clear();
                    column = 0;
                }
                if (!entry.is_dir) {
                    ++file_count;
                    byte_count += entry.size;
                }
            }
            if (!row.empty()) {
                row.erase(row.find_last_not_of(' ') + 1);
                emit(row);
            }
        } else {
            for (const Entry& entry : filtered) {
                if (opt.bare) {
                    emit(opt.lower ? text::lower(entry.name) : entry.name);
                } else if (opt.verbose) {
                    auto clock = clock_of(entry.mtime);
                    std::string cell = name_cell(entry, opt.lower);
                    emit(cell + " " + right_align(text::number_with_commas(entry.size), 11) + " " +
                         right_align(text::number_with_commas(entry.size), 11) + "  " +
                         text::short_date2(clock) + "  " + text::file_time_text(clock) + "  " +
                         attr_string(entry));
                } else {
                    auto clock = clock_of(entry.mtime);
                    std::string cell = name_cell(entry, opt.lower);
                    std::string size_field = entry.is_dir ? std::string("<DIR>") : text::number_with_commas(entry.size);
                    emit(cell + " " + right_align(size_field, 9) + "  " + text::short_date2(clock) + "  " +
                         text::file_time_text(clock));
                }
                if (!entry.is_dir) {
                    ++file_count;
                    byte_count += entry.size;
                }
            }
        }

        if (!opt.bare) {
            emit(right_align(std::to_string(file_count), 9) + " file(s)  " +
                 right_align(text::number_with_commas(byte_count), 13) + " bytes");
            total_files += file_count;
            total_bytes += byte_count;
            any_files = true;
        }
    };

    for (const auto& spec : specs) {
        auto parsed = detail::parse_spec(*this, spec);
        if (!parsed.valid) {
            message(parsed.error);
            continue;
        }
        if (opt.recurse) {
            std::vector<std::filesystem::path> stack{parsed.dir};
            std::vector<std::filesystem::path> order;
            while (!stack.empty()) {
                auto dir = stack.back();
                stack.pop_back();
                order.push_back(dir);
                for (auto& entry : detail::list_entries(dir)) {
                    if (entry.is_dir && entry.name != "." && entry.name != ".." &&
                        !entry.attrs.hidden && !entry.attrs.system)
                        stack.push_back(entry.path);
                }
            }
            bool found_any = false;
            for (auto& dir : order) {
                bool found = false;
                emit_listing(dir, parsed.pattern, true, found);
                found_any = found_any || found;
            }
            if (!found_any) message("File not found");
            if (found_any && !opt.bare) {
                emit("Total files listed:");
                emit(right_align(std::to_string(total_files), 9) + " file(s)  " +
                     right_align(text::number_with_commas(total_bytes), 13) + " bytes");
                std::error_code ec;
                auto space = std::filesystem::space(parsed.dir, ec);
                if (!ec)
                    emit(right_align(text::number_with_commas(static_cast<std::uintmax_t>(space.available)), 28) +
                         " bytes free");
            }
        } else {
            bool found = false;
            emit_listing(parsed.dir, parsed.pattern, true, found);
            if (!found) {
                message("File not found");
                continue;
            }
            if (!opt.bare) {
                std::error_code ec;
                auto space = std::filesystem::space(parsed.dir, ec);
                if (!ec) emit(right_align(text::number_with_commas(static_cast<std::uintmax_t>(space.available)), 28) + " bytes free");
            }
        }
    }
    (void)any_files;
}

} // namespace cmdpp
