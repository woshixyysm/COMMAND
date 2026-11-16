#include "cmdpp/shell.hpp"
#include "cmdpp/text.hpp"

#include <cstdio>
#include <filesystem>
#include <system_error>

#include "fileutil.hpp"

namespace cmdpp {
namespace {

enum class CopyMode { Default, Ascii, Binary };

struct PathItem {
    std::string spec;
    CopyMode mode = CopyMode::Default;
    bool plus_before = false;
};

struct SourceFile {
    std::filesystem::path path;
    std::string name;
    std::string pattern;
    CopyMode mode = CopyMode::Default;
};

bool is_switch_token(std::string_view token) {
    return !token.empty() && (token[0] == '/' || token[0] == '-');
}

bool is_device(const std::string& name, std::string& kind) {
    std::string base = name;
    auto slash = base.find_last_of("\\/");
    if (slash != std::string::npos) base = base.substr(slash + 1);
    auto dot = base.find('.');
    if (dot != std::string::npos) base = base.substr(0, dot);
    base = text::upper(base);
    if (base == "CON") {
        kind = "CON";
        return true;
    }
    if (base == "NUL") {
        kind = "NUL";
        return true;
    }
    return false;
}

bool same_path(const std::filesystem::path& a, const std::filesystem::path& b) {
    std::error_code ec;
    if (!std::filesystem::exists(a, ec) || !std::filesystem::exists(b, ec)) return false;
    auto ca = std::filesystem::weakly_canonical(a, ec);
    if (ec) return false;
    auto cb = std::filesystem::weakly_canonical(b, ec);
    if (ec) return false;
    return text::iequals(ca.string(), cb.string());
}

std::string leaf_of(std::string spec) {
    if (spec.size() >= 2 && spec.front() == '"' && spec.back() == '"') spec = spec.substr(1, spec.size() - 2);
    for (char& c : spec)
        if (c == '\\') c = '/';
    auto slash = spec.find_last_of('/');
    if (slash != std::string::npos) spec = spec.substr(slash + 1);
    return spec;
}

CopyMode effective_mode(CopyMode mode, bool concatenating) {
    if (mode != CopyMode::Default) return mode;
    return concatenating ? CopyMode::Ascii : CopyMode::Binary;
}

// ASCII sources stop at ^Z; the ^Z itself is written once, by the destination.
void copy_stream(std::FILE* src, std::FILE* dst, bool ascii_source) {
    char buffer[65536];
    for (;;) {
        std::size_t n = std::fread(buffer, 1, sizeof buffer, src);
        if (n == 0) break;
        if (ascii_source) {
            std::size_t stop = n;
            for (std::size_t i = 0; i < n; ++i) {
                if (buffer[i] == '\x1a') {
                    stop = i;
                    break;
                }
            }
            if (stop) std::fwrite(buffer, 1, stop, dst);
            if (stop < n) return;
        } else {
            std::fwrite(buffer, 1, n, dst);
        }
    }
}

void copy_console(std::FILE* src, std::FILE* dst) {
    int c = 0;
    while ((c = std::fgetc(src)) != EOF) {
        if (c == 0x1A) break;
        if (dst) std::fputc(c, dst);
    }
}

} // namespace

void Shell::cmd_copy(const Invocation& inv) {
    int yes_mode = 0; // 1 = /Y, 2 = /-Y
    bool verify = false;
    std::vector<PathItem> items;
    CopyMode mode = CopyMode::Default;
    bool pending_plus = false;

    for (const auto& token : inv.tokens) {
        if (is_switch_token(token) && token.size() >= 2) {
            const std::string up = text::upper(token);
            if (up == "/A") {
                mode = CopyMode::Ascii;
                continue;
            }
            if (up == "/B") {
                mode = CopyMode::Binary;
                continue;
            }
            if (up == "/V") {
                verify = true;
                continue;
            }
            if (up == "/Y") {
                yes_mode = 1;
                continue;
            }
            if (up == "/-Y") {
                yes_mode = 2;
                continue;
            }
        }
        std::string piece;
        for (char c : token) {
            if (c == '+') {
                if (!piece.empty()) {
                    items.push_back(PathItem{piece, mode, pending_plus});
                    piece.clear();
                }
                pending_plus = true;
            } else {
                piece.push_back(c);
            }
        }
        if (!piece.empty()) {
            items.push_back(PathItem{piece, mode, pending_plus});
            pending_plus = false;
        }
    }
    (void)verify;

    if (items.empty()) {
        message("Required parameter missing");
        return;
    }

    std::vector<PathItem> source_items;
    std::string destination;
    if (items.size() == 1) {
        source_items = items;
    } else if (!items.back().plus_before) {
        destination = items.back().spec;
        source_items.assign(items.begin(), items.end() - 1);
    } else {
        source_items = items;
    }
    if (source_items.empty()) {
        message("Required parameter missing");
        return;
    }
    for (std::size_t i = 0; i < source_items.size(); ++i) {
        if (i == 0) {
            if (source_items[i].plus_before) {
                message("Too many parameters");
                return;
            }
        } else if (!source_items[i].plus_before) {
            message("Too many parameters");
            return;
        }
    }

    if (yes_mode == 0) {
        if (auto copy_cmd = env_.get("COPYCMD")) {
            const std::string up = text::upper(*copy_cmd);
            if (up.find("/-Y") != std::string::npos)
                yes_mode = 2;
            else if (up.find("/Y") != std::string::npos)
                yes_mode = 1;
        }
    }

    bool any_ambiguous = false;
    for (const auto& item : source_items)
        if (text::has_wildcard(leaf_of(item.spec))) any_ambiguous = true;

    std::string dest_kind;
    const bool dest_device = !destination.empty() && is_device(destination, dest_kind);

    std::filesystem::path dest_path;
    bool dest_is_dir = false;
    bool dest_wild = false;
    if (!destination.empty() && !dest_device) {
        bool drive_bad = false;
        dest_path = resolve_path(destination, drive_bad);
        if (drive_bad) {
            message("Invalid drive specification");
            return;
        }
        std::error_code ec;
        dest_is_dir = std::filesystem::is_directory(dest_path, ec);
        if (!dest_is_dir && !dest_path.filename().empty())
            dest_wild = text::has_wildcard(dest_path.filename().string());
    }

    // DOS: a wildcard source with an unambiguous file destination means append.
    const bool concatenating =
        source_items.size() > 1 ||
        (any_ambiguous && !destination.empty() && !dest_device && !dest_is_dir && !dest_wild);

    std::vector<SourceFile> sources;
    bool source_is_console = false;
    for (const auto& item : source_items) {
        std::string kind;
        if (is_device(item.spec, kind)) {
            if (kind == "CON") source_is_console = true;
            continue;
        }
        auto parsed = detail::parse_spec(*this, item.spec);
        if (!parsed.valid) {
            message(parsed.error);
            return;
        }
        auto entries = detail::collect_entries(parsed.dir, parsed.pattern);
        bool matched = false;
        for (auto& entry : entries) {
            if (entry.is_dir) continue;
            sources.push_back(SourceFile{entry.path, entry.name, parsed.pattern, item.mode});
            matched = true;
        }
        if (!matched && !concatenating) {
            message("File not found");
            return;
        }
    }

    std::uintmax_t copied = 0;
    bool all_yes = false;
    const bool prompt_overwrite = yes_mode != 1;
    auto confirm_overwrite = [&](const std::filesystem::path& target) {
        std::error_code ec;
        if (!prompt_overwrite || all_yes || !std::filesystem::exists(target, ec)) return true;
        int answer = detail::choice(*this, "Overwrite " + display_path(target) + " (Yes/No/All)?");
        if (answer <= 0) return false;
        if (answer == 2) all_yes = true;
        return true;
    };

    if (source_is_console) {
        if (destination.empty()) {
            message("Required parameter missing");
            return;
        }
        if (dest_device && dest_kind == "NUL") {
            copy_console(in_, nullptr);
            out_line("1 file(s) copied");
            return;
        }
        std::filesystem::path target = dest_is_dir ? dest_path / "CON" : dest_path;
        if (!confirm_overwrite(target)) {
            out_line("0 file(s) copied");
            return;
        }
        std::FILE* dst = std::fopen(target.string().c_str(), "wb");
        if (!dst) {
            message("Access denied");
            return;
        }
        copy_console(in_, dst);
        std::fclose(dst);
        out_line("1 file(s) copied");
        return;
    }

    if (dest_device) {
        std::FILE* dst = dest_kind == "NUL" ? nullptr : out_;
        for (const auto& source : sources) {
            std::FILE* src = std::fopen(source.path.string().c_str(), "rb");
            if (!src) {
                message("File not found");
                continue;
            }
            if (dst) copy_stream(src, dst, effective_mode(source.mode, true) == CopyMode::Ascii);
            std::fclose(src);
        }
        out_line("1 file(s) copied");
        return;
    }

    if (sources.empty()) {
        if (concatenating)
            out_line("0 file(s) copied");
        else
            message("File not found");
        return;
    }

    if (concatenating) {
        const std::filesystem::path target = destination.empty() ? sources.front().path : dest_path;
        const bool same_as_first = same_path(target, sources.front().path);
        if (!same_as_first && !confirm_overwrite(target)) {
            out_line("0 file(s) copied");
            return;
        }
        // DOS: the destination is also a later source, so it is truncated before use.
        for (std::size_t i = 1; i < sources.size(); ++i) {
            if (same_path(target, sources[i].path)) message("Content of destination lost before copy");
        }
        std::FILE* dst = std::fopen(target.string().c_str(), same_as_first ? "ab" : "wb");
        if (!dst) {
            message("Access denied");
            return;
        }
        const std::size_t start = same_as_first ? 1 : 0;
        for (std::size_t i = start; i < sources.size(); ++i) {
            std::FILE* src = std::fopen(sources[i].path.string().c_str(), "rb");
            if (!src) {
                message("File not found");
                continue;
            }
            copy_stream(src, dst, effective_mode(sources[i].mode, true) == CopyMode::Ascii);
            std::fclose(src);
        }
        if (effective_mode(mode, true) == CopyMode::Ascii) std::fputc('\x1a', dst);
        std::fclose(dst);
        out_line("1 file(s) copied");
        return;
    }

    const std::filesystem::path dest_dir = dest_is_dir ? dest_path : dest_path.parent_path();
    const std::string dest_leaf = dest_path.filename().string();

    for (const auto& source : sources) {
        std::filesystem::path target;
        if (destination.empty()) {
            target = std::filesystem::current_path() / source.name;
        } else if (dest_is_dir) {
            target = dest_path / source.name;
        } else if (dest_wild) {
            target = dest_dir / text::wildcard_transform(source.name, source.pattern, dest_leaf);
        } else {
            target = dest_path;
        }
        if (same_path(source.path, target)) {
            message("File cannot be copied onto itself");
            continue;
        }
        if (!confirm_overwrite(target)) continue;
        std::FILE* src = std::fopen(source.path.string().c_str(), "rb");
        if (!src) {
            message("File not found");
            continue;
        }
        std::FILE* dst = std::fopen(target.string().c_str(), "wb");
        if (!dst) {
            std::fclose(src);
            message("Access denied");
            continue;
        }
        copy_stream(src, dst, effective_mode(source.mode, false) == CopyMode::Ascii);
        if (effective_mode(mode, false) == CopyMode::Ascii) std::fputc('\x1a', dst);
        std::fclose(dst);
        std::fclose(src);
        ++copied;
    }

    out_line(std::to_string(copied) + " file(s) copied");
}

} // namespace cmdpp
