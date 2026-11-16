#include "cmdpp/shell.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <system_error>

#include "cmdpp/text.hpp"

namespace cmdpp {
namespace {

constexpr const char* kVersionLine = "Windows 95 OPK3. [Version 4.10.1053]";

struct Prescan {
    std::string text;
    Redirection redir;
    bool syntax_error = false;
};

// DOS uses ';' in PATH; a POSIX host PATH arrives with ':'.
std::vector<std::string_view> split_search_path(std::string_view value) {
    char separator = ';';
#if !defined(_WIN32)
    if (value.find(';') == std::string_view::npos) separator = ':';
#endif
    std::vector<std::string_view> parts;
    std::size_t start = 0;
    while (start <= value.size()) {
        std::size_t end = value.find(separator, start);
        parts.push_back(value.substr(start, end == std::string_view::npos ? std::string_view::npos : end - start));
        if (end == std::string_view::npos) break;
        start = end + 1;
    }
    return parts;
}

std::string strip_cr(std::string s) {
    while (!s.empty() && (s.back() == '\r' || s.back() == '\n')) s.pop_back();
    return s;
}

Prescan prescan_line(std::string_view line) {
    Prescan result;
    const std::size_t n = line.size();

    std::size_t quotes = 0;
    for (char c : line)
        if (c == '"') ++quotes;
    const bool quote_active = (quotes % 2) == 0;

    bool in_quotes = false;
    std::size_t i = 0;
    while (i < n) {
        const char c = line[i];
        if (quote_active && c == '"') {
            in_quotes = !in_quotes;
            result.text.push_back(c);
            ++i;
            continue;
        }
        if (!in_quotes && (c == '<' || c == '>')) {
            bool append = false;
            if (c == '>' && i + 1 < n && line[i + 1] == '>') {
                append = true;
                ++i;
            }
            ++i;
            i = text::skip_delims(line, i);
            std::string file;
            bool bad = false;
            while (i < n) {
                const char d = line[i];
                if (text::is_delim(d) || d == '<' || d == '>') break;
                if (d == '"') {
                    bad = true;
                    break;
                }
                file.push_back(d);
                ++i;
            }
            if (file.empty() || bad) {
                result.syntax_error = true;
                return result;
            }
            if (c == '<') {
                result.redir.in_set = true;
                result.redir.in_file = std::move(file);
            } else {
                result.redir.out_set = true;
                result.redir.append = append;
                result.redir.out_file = std::move(file);
            }
            continue;
        }
        result.text.push_back(c);
        ++i;
    }
    return result;
}

void close_stream(std::FILE*& f) {
    if (f) {
        std::fclose(f);
        f = nullptr;
    }
}

} // namespace

const Shell::CommandSpec kCommandTable[] = {
    {"DIR", &Shell::cmd_dir, "DIR", false},
    {"CALL", &Shell::cmd_call, "CALL", false},
    {"CHCP", &Shell::cmd_chcp, "CHCP", false},
    {"RENAME", &Shell::cmd_rename, "RENAME", false},
    {"REN", &Shell::cmd_rename, "RENAME", false},
    {"ERASE", &Shell::cmd_erase, "DEL", false},
    {"DEL", &Shell::cmd_erase, "DEL", false},
    {"TYPE", &Shell::cmd_type, "TYPE", false},
    {"REM", &Shell::cmd_rem, "REM", true},
    {"COPY", &Shell::cmd_copy, "COPY", false},
    {"PAUSE", &Shell::cmd_pause, "PAUSE", true},
    {"DATE", &Shell::cmd_date, "DATE", false},
    {"TIME", &Shell::cmd_time, "TIME", false},
    {"VER", &Shell::cmd_ver, "VER", false},
    {"VOL", &Shell::cmd_vol, "VOL", false},
    {"CD", &Shell::cmd_chdir, "CHDIR", false},
    {"CHDIR", &Shell::cmd_chdir, "CHDIR", false},
    {"MD", &Shell::cmd_mkdir, "MKDIR", false},
    {"MKDIR", &Shell::cmd_mkdir, "MKDIR", false},
    {"RD", &Shell::cmd_rmdir, "RMDIR", false},
    {"RMDIR", &Shell::cmd_rmdir, "RMDIR", false},
    {"BREAK", &Shell::cmd_break, "BREAK", false},
    {"VERIFY", &Shell::cmd_verify, "VERIFY", false},
    {"SET", &Shell::cmd_set, "SET", true},
    {"PROMPT", &Shell::cmd_prompt, "PROMPT", true},
    {"PATH", &Shell::cmd_path, "PATH", false},
    {"EXIT", &Shell::cmd_exit, "EXIT", false},
    {"CTTY", &Shell::cmd_ctty, "CTTY", false},
    {"ECHO", &Shell::cmd_echo, "ECHO", true},
    {"GOTO", &Shell::cmd_goto, "GOTO", true},
    {"SHIFT", &Shell::cmd_shift, "SHIFT", false},
    {"IF", &Shell::cmd_if, "IF", true},
    {"FOR", &Shell::cmd_for, "FOR", true},
    {"CLS", &Shell::cmd_cls, "CLS", false},
    {"TRUENAME", &Shell::cmd_truename, "TRUENAME", false},
    {"LOADHIGH", &Shell::cmd_loadhigh, "LOADHIGH", false},
    {"LH", &Shell::cmd_loadhigh, "LOADHIGH", false},
    {"LFNFOR", &Shell::cmd_lfnfor, "LFNFOR", false},
};

Shell::Shell(Environment env, std::string self_path) : env_(std::move(env)), self_path_(std::move(self_path)) {
    if (!env_.get("COMSPEC") && !self_path_.empty()) env_.set("COMSPEC", display_path(std::filesystem::path(self_path_)));
}

platform::Clock Shell::now() const {
    if (clock_offset_ == 0) return platform::local_clock();
    int64_t base = platform::epoch_seconds(platform::local_clock());
    return platform::from_epoch(base + clock_offset_);
}

void Shell::out(std::string_view s) {
    platform::write(out_, s);
}

void Shell::out_line(std::string_view s) {
    platform::write(out_, s);
    platform::write(out_, "\n");
    platform::flush(out_);
}

void Shell::show_banner() {
    out_line("Microsoft(R) Windows 95");
    out_line("   (C)Copyright Microsoft Corp 1981-1996.");
    out_line();
}

std::string Shell::display_path(const std::filesystem::path& p) const {
    std::string s = p.generic_string();
    if (s.empty()) s = ".";
    std::string out;
    if (s.size() >= 2 && s[1] == ':') {
        out.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(s[0]))));
        out += s.substr(1);
    } else {
        out = platform::drive_letter() + ":";
        if (s.empty() || s[0] != '/') out.push_back('/');
        out += s;
    }
    for (char& c : out)
        if (c == '/') c = '\\';
    return out;
}

std::string Shell::current_dir_display() const {
    std::string s = display_path(std::filesystem::current_path());
    if (s.size() == 2) s.push_back('\\');
    return s;
}

std::filesystem::path Shell::resolve_path(std::string_view dos_text, bool& drive_bad) const {
    drive_bad = false;
    std::string s(dos_text);
    if (s.size() >= 2 && s.front() == '"' && s.back() == '"') s = s.substr(1, s.size() - 2);
    for (char& c : s)
        if (c == '\\') c = '/';
    if (s.size() >= 2 && s[1] == ':') {
        char drive = static_cast<char>(std::toupper(static_cast<unsigned char>(s[0])));
        std::string current = platform::drive_letter();
        if (drive != current[0]) {
            drive_bad = true;
            return {};
        }
        std::string rest = s.substr(2);
        if (rest.empty()) return std::filesystem::current_path();
        std::filesystem::path rel(rest);
        if (rel.is_absolute()) rel = rel.relative_path();
        return (std::filesystem::current_path().root_path() / rel).lexically_normal();
    }
    std::filesystem::path p(s);
    if (p.is_absolute()) return p.lexically_normal();
    return (std::filesystem::current_path() / p).lexically_normal();
}

std::string Shell::prompt_text() const {
    auto stored = env_.get("PROMPT");
    std::string spec = stored ? *stored : "$P$G";
    std::string out;
    for (std::size_t i = 0; i < spec.size(); ++i) {
        const char c = spec[i];
        if (c != '$' || i + 1 >= spec.size()) {
            out.push_back(c);
            continue;
        }
        const char d = spec[++i];
        switch (d) {
            case 'P': out += current_dir_display(); break;
            case 'N': out += platform::drive_letter(); break;
            case 'G': out.push_back('>'); break;
            case 'L': out.push_back('<'); break;
            case 'B': out.push_back('|'); break;
            case 'Q': out.push_back('='); break;
            case '$': out.push_back('$'); break;
            case 'T': out += text::time_text(now()); break;
            case 'D': out += text::date_text(now()); break;
            case 'V': out += kVersionLine; break;
            case 'E': out.push_back('\x1b'); break;
            case 'H':
                if (!out.empty()) out.pop_back();
                break;
            case '_': out.push_back('\n'); break;
            default: out.push_back(d); break;
        }
    }
    return out;
}

int Shell::run_interactive() {
    while (!exit_) {
        if (batches_.empty()) {
            if (echo_) {
                out(prompt_text());
                platform::flush(out_);
            }
            auto line = platform::read_line(in_);
            if (!line) break;
            execute_line(*line);
        } else {
            step_batch();
        }
    }
    return exit_code();
}

int Shell::run_single(const std::string& line) {
    execute_line(line);
    while (!batches_.empty() && !exit_) step_batch();
    return exit_code();
}

std::vector<std::string> Shell::split_pipes(std::string_view line, bool& error) const {
    error = false;
    std::size_t quotes = 0;
    for (char c : line)
        if (c == '"') ++quotes;
    const bool quote_active = (quotes % 2) == 0;

    std::vector<std::string> segments;
    std::string current;
    bool in_quotes = false;
    std::size_t count = 1;
    for (char c : line) {
        if (quote_active && c == '"') {
            in_quotes = !in_quotes;
            current.push_back(c);
            continue;
        }
        if (!in_quotes && c == '|') {
            segments.push_back(current);
            current.clear();
            ++count;
            continue;
        }
        current.push_back(c);
    }
    segments.push_back(std::move(current));
    if (count == 1) {
        segments.clear();
        return segments;
    }
    for (const auto& s : segments) {
        if (text::trim(s).empty()) {
            error = true;
            return {};
        }
    }
    return segments;
}

void Shell::execute_line(const std::string& line_in) {
    std::string line = strip_cr(line_in);
    if (line.empty()) return;
    bool pipe_error = false;
    auto segments = split_pipes(line, pipe_error);
    if (pipe_error) {
        message("Syntax error");
        return;
    }
    if (segments.empty()) {
        execute_segment(line, in_, out_);
        return;
    }
    run_pipeline(segments);
}

void Shell::run_pipeline(const std::vector<std::string>& segments) {
    const std::size_t n = segments.size();
    std::vector<std::string> temps(n - 1);
    struct Cleanup {
        std::vector<std::string>& files;
        ~Cleanup() {
            for (const auto& file : files)
                if (!file.empty()) std::remove(file.c_str());
        }
    } cleanup{temps};

    for (std::size_t i = 0; i + 1 < n; ++i) {
        temps[i] = platform::temp_file("cpm");
        if (temps[i].empty()) {
            message("Could not allocate memory for pipe.");
            return;
        }
    }
    for (std::size_t i = 0; i < n && !exit_; ++i) {
        std::FILE* fin = in_;
        std::FILE* fout = out_;
        std::FILE* open_in = nullptr;
        std::FILE* open_out = nullptr;
        bool failed = false;
        if (i > 0) {
            open_in = std::fopen(temps[i - 1].c_str(), "rb");
            if (!open_in) {
                message("Could not allocate memory for pipe.");
                failed = true;
            } else {
                fin = open_in;
            }
        }
        if (!failed && i + 1 < n) {
            open_out = std::fopen(temps[i].c_str(), "wb");
            if (!open_out) {
                message("Could not allocate memory for pipe.");
                failed = true;
            } else {
                fout = open_out;
            }
        }
        if (!failed) execute_segment(segments[i], fin, fout);
        if (open_in) std::fclose(open_in);
        if (open_out) std::fclose(open_out);
    }
}

void Shell::execute_segment(std::string_view text, std::FILE* fin, std::FILE* fout) {
    Prescan pre = prescan_line(text);

    std::FILE* saved_in = in_;
    std::FILE* saved_out = out_;
    std::FILE* own_in = nullptr;
    std::FILE* own_out = nullptr;
    in_ = fin;
    out_ = fout;

    auto restore = [&] {
        in_ = saved_in;
        out_ = saved_out;
        close_stream(own_in);
        close_stream(own_out);
    };

    if (pre.syntax_error) {
        message("Invalid filename or file not found");
        restore();
        return;
    }
    if (pre.redir.in_set) {
        bool bad = false;
        auto p = resolve_path(pre.redir.in_file, bad);
        if (!bad) own_in = std::fopen(p.string().c_str(), "r");
        if (!own_in) {
            message(bad ? "Invalid drive specification" : "File not found");
            restore();
            return;
        }
        in_ = own_in;
    }
    if (pre.redir.out_set) {
        bool bad = false;
        auto p = resolve_path(pre.redir.out_file, bad);
        if (!bad) own_out = std::fopen(p.string().c_str(), pre.redir.append ? "a" : "w");
        if (!own_out) {
            message(bad ? "Invalid drive specification" : "Invalid path or file name");
            restore();
            return;
        }
        out_ = own_out;
    }

    std::string_view clean = pre.text;
    std::size_t i = text::skip_delims(clean, 0);
    if (i >= clean.size()) {
        restore();
        return;
    }
    std::size_t j = i;
    while (j < clean.size() && !text::is_delim(clean[j])) ++j;
    std::string name(clean.substr(i, j - i));
    std::size_t t = j;
    while (t < clean.size() && (clean[t] == ' ' || clean[t] == '\t')) ++t;
    std::string tail(clean.substr(t));

    unsigned long long generation = batch_generation_;
    dispatch(name, tail);
    if (batch_generation_ != generation && !batches_.empty()) {
        // Redirection stays in effect for the whole batch file.
        BatchFrame& frame = batches_.back();
        if (own_in) {
            frame.owned_in = own_in;
            own_in = nullptr;
        }
        if (own_out) {
            frame.owned_out = own_out;
            own_out = nullptr;
        }
    }
    restore();
}

void Shell::dispatch(const std::string& name, const std::string& tail) {
    Invocation inv{name, tail, text::split_args(tail)};
    if (const CommandSpec* spec = find_command(name)) {
        if (help_requested(inv, *spec)) {
            print_help(spec->help_key);
            return;
        }
        (this->*spec->fn)(inv);
        return;
    }
    std::string remainder;
    if (const CommandSpec* spec = find_attached(name, remainder)) {
        std::string new_tail = remainder;
        if (!tail.empty()) {
            new_tail.push_back(' ');
            new_tail += tail;
        }
        dispatch(spec->name, new_tail);
        return;
    }
    run_external(name, tail);
}

bool Shell::help_requested(const Invocation& inv, const CommandSpec& spec) const {
    bool found = false;
    for (const auto& token : inv.tokens) {
        if (token == "/?" || token == "-?") {
            found = true;
            break;
        }
    }
    if (!found) return false;
    if (spec.limit_help) return inv.tokens.size() == 1;
    return true;
}

void Shell::print_help(const char* key) {
    out(command_help(key));
}

const Shell::CommandSpec* Shell::find_command(std::string_view name) {
    for (const auto& spec : kCommandTable) {
        if (text::iequals(name, spec.name)) return &spec;
    }
    return nullptr;
}

const Shell::CommandSpec* Shell::find_attached(std::string_view token, std::string& remainder) {
    const CommandSpec* best = nullptr;
    std::size_t best_len = 0;
    for (const auto& spec : kCommandTable) {
        const std::size_t len = std::strlen(spec.name);
        if (token.size() <= len) continue;
        if (!text::istarts_with(token, spec.name)) continue;
        const char next = token[len];
        if (next != '.' && next != '\\' && next != '/' && next != ':') continue;
        if (len > best_len) {
            best = &spec;
            best_len = len;
            remainder = std::string(token.substr(len));
        }
    }
    return best;
}

std::optional<Shell::ProgramHit> Shell::find_program(const std::string& name) const {
    auto make = [](const std::filesystem::path& p) -> std::optional<ProgramHit> {
        std::error_code ec;
        if (!std::filesystem::is_regular_file(p, ec)) return std::nullopt;
        bool batch = text::iequals(p.extension().string(), ".bat");
        return ProgramHit{p, batch};
    };
    auto try_with_ext = [&](const std::filesystem::path& base) -> std::optional<ProgramHit> {
        if (!base.extension().empty()) return make(base);
        for (const char* ext : {".COM", ".EXE", ".BAT"}) {
            std::filesystem::path candidate = base;
            candidate += ext;
            if (auto hit = make(candidate)) return hit;
        }
#if !defined(_WIN32)
        if (auto hit = make(base)) return hit;
#endif
        return std::nullopt;
    };

    bool drive_bad = false;
    if (name.find_first_of(":\\/") != std::string::npos) {
        auto path = resolve_path(name, drive_bad);
        if (drive_bad) return std::nullopt;
        return try_with_ext(path);
    }
    if (auto hit = try_with_ext(std::filesystem::current_path() / name)) return hit;

    auto path_value = env_.get("PATH");
    if (!path_value) return std::nullopt;
    for (std::string_view component : split_search_path(*path_value)) {
        std::filesystem::path dir;
        if (component.empty()) {
            dir = std::filesystem::current_path();
        } else {
            drive_bad = false;
            dir = resolve_path(component, drive_bad);
            if (drive_bad) continue;
        }
        if (auto hit = try_with_ext(dir / name)) return hit;
    }
    return std::nullopt;
}

void Shell::run_external(const std::string& name, const std::string& tail) {
    auto hit = find_program(name);
    if (!hit) {
        message("Bad command or file name");
        return;
    }
    if (hit->batch) {
        if (!begin_batch(name, tail, call_active_)) message("Bad command or file name");
        return;
    }
    platform::SpawnRequest req;
    req.program = hit->path.string();
    req.args = tail;
    req.cwd = platform::cwd();
    req.env = env_.block();
    req.in = in_;
    req.out = out_;
    req.err = out_;
    auto result = platform::spawn(req);
    if (!result.started) {
        message("Bad command or file name");
        return;
    }
    error_level_ = result.exit_code & 0xFF;
}

bool Shell::begin_batch(const std::string& display, const std::string& arg_tail, bool via_call) {
    auto hit = find_program(display);
    if (!hit || !hit->batch) return false;

    std::ifstream input(hit->path, std::ios::binary);
    if (!input) return false;
    std::string data((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());

    std::vector<std::string> lines;
    std::string current;
    for (char c : data) {
        if (c == '\x1a') break;
        if (c == '\r') continue;
        if (c == '\n') {
            lines.push_back(current);
            current.clear();
        } else {
            current.push_back(c);
        }
    }
    if (!current.empty()) lines.push_back(current);

    if (!via_call && !batches_.empty()) {
        // Running a batch from a batch without CALL terminates the current one.
        for (auto& frame : batches_) {
            close_stream(frame.owned_in);
            close_stream(frame.owned_out);
        }
        batches_.clear();
    }

    BatchFrame frame;
    frame.display = display;
    frame.native = hit->path;
    frame.lines = std::move(lines);
    frame.args.push_back(display);
    for (auto& arg : text::split_args(arg_tail)) frame.args.push_back(arg);
    frame.echo_saved = echo_;
    batches_.push_back(std::move(frame));
    ++batch_generation_;
    return true;
}

std::string Shell::expand_batch_line(std::string_view raw) const {
    static const std::vector<std::string> empty_args;
    const auto& args = batches_.empty() ? empty_args : batches_.back().args;

    std::string out;
    std::size_t i = 0;
    while (i < raw.size()) {
        const char c = raw[i];
        if (c != '%') {
            out.push_back(c);
            ++i;
            continue;
        }
        if (i + 1 >= raw.size()) break; // trailing '%' is dropped
        const char d = raw[i + 1];
        if (d == '%') {
            out.push_back('%');
            i += 2;
            continue;
        }
        if (d >= '0' && d <= '9') {
            const std::size_t index = static_cast<std::size_t>(d - '0');
            if (index < args.size()) out += args[index];
            i += 2;
            continue;
        }
        const std::size_t close = raw.find('%', i + 2);
        if (close == std::string_view::npos) {
            // Unterminated variable: DOS drops the '%' and copies the rest.
            out.append(raw.substr(i + 1));
            break;
        }
        std::string name(raw.substr(i + 1, close - i - 1));
        if (auto value = env_.get(name)) out += *value;
        i = close + 1;
    }
    return out;
}

bool Shell::step_batch() {
    while (!batches_.empty()) {
        BatchFrame& frame = batches_.back();
        if (frame.next >= frame.lines.size()) {
            pop_batch();
            if (exit_) return false;
            continue;
        }
        std::string line = expand_batch_line(frame.lines[frame.next++]);
        const bool suppress = !line.empty() && line[0] == '@';
        if (suppress) line.erase(0, 1);
        if (echo_ && !suppress) {
            out(prompt_text());
            out(line);
            out_line();
        }
        std::FILE* saved_in = in_;
        std::FILE* saved_out = out_;
        if (frame.owned_in) in_ = frame.owned_in;
        if (frame.owned_out) out_ = frame.owned_out;
        execute_line(line);
        in_ = saved_in;
        out_ = saved_out;
        return true;
    }
    return false;
}

void Shell::pop_batch() {
    if (batches_.empty()) return;
    BatchFrame& frame = batches_.back();
    echo_ = frame.echo_saved;
    close_stream(frame.owned_in);
    close_stream(frame.owned_out);
    batches_.pop_back();
}

void Shell::abort_batch_chain() {
    if (batches_.empty()) return;
    echo_ = batches_.front().echo_saved;
    for (auto& frame : batches_) {
        close_stream(frame.owned_in);
        close_stream(frame.owned_out);
    }
    batches_.clear();
}

} // namespace cmdpp
