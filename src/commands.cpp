#include "cmdpp/shell.hpp"
#include "cmdpp/text.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <system_error>

#include "fileutil.hpp"

namespace cmdpp {
namespace {

bool is_switch(std::string_view token) {
    return !token.empty() && (token[0] == '/' || token[0] == '-');
}

std::string strip_quotes(std::string s) {
    if (s.size() >= 2 && s.front() == '"' && s.back() == '"') return s.substr(1, s.size() - 2);
    return s;
}

std::string current_state(bool value) {
    return value ? "ON" : "OFF";
}

bool parse_on_off(std::string_view text, bool& value) {
    if (text::iequals(text, "ON")) {
        value = true;
        return true;
    }
    if (text::iequals(text, "OFF")) {
        value = false;
        return true;
    }
    return false;
}

int days_in_month(int year, int month) {
    static const int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month == 2) {
        bool leap = (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
        return leap ? 29 : 28;
    }
    if (month < 1 || month > 12) return 0;
    return days[month - 1];
}

bool parse_date_text(std::string_view text, int& year, int& month, int& day) {
    int parts[3] = {0, 0, 0};
    int count = 0;
    std::string digits;
    for (char c : text) {
        if (std::isdigit(static_cast<unsigned char>(c))) {
            digits.push_back(c);
        } else if (c == '-' || c == '/' || c == '.') {
            if (digits.empty() || count >= 3) return false;
            parts[count++] = std::atoi(digits.c_str());
            digits.clear();
        } else if (c == ' ') {
            continue;
        } else {
            return false;
        }
    }
    if (digits.empty() || count != 2) return false;
    parts[count++] = std::atoi(digits.c_str());
    month = parts[0];
    day = parts[1];
    year = parts[2];
    if (year < 100) year += (year < 70) ? 2000 : 1900;
    if (month < 1 || month > 12) return false;
    if (year < 1980 || year > 2099) return false;
    if (day < 1 || day > days_in_month(year, month)) return false;
    return true;
}

bool parse_time_text(std::string_view text, int& hour, int& minute, int& second) {
    std::string s(text);
    int h = 0, m = 0, sec = 0;
    bool pm = false, am = false;
    if (!s.empty()) {
        char last = static_cast<char>(std::tolower(static_cast<unsigned char>(s.back())));
        if (last == 'p' || last == 'a') {
            pm = last == 'p';
            am = last == 'a';
            s.pop_back();
        }
    }
    std::string digits;
    int parts[3] = {0, 0, 0};
    int count = 0;
    for (char c : s) {
        if (std::isdigit(static_cast<unsigned char>(c))) {
            digits.push_back(c);
        } else if (c == ':' || c == '.') {
            if (digits.empty() || count >= 3) return false;
            parts[count++] = std::atoi(digits.c_str());
            digits.clear();
        } else if (c == ' ') {
            continue;
        } else {
            return false;
        }
    }
    if (digits.empty()) return false;
    parts[count++] = std::atoi(digits.c_str());
    if (count == 2) {
        h = parts[0];
        m = parts[1];
        sec = 0;
    } else if (count == 3) {
        h = parts[0];
        m = parts[1];
        sec = parts[2];
    } else {
        return false;
    }
    if (pm || am) {
        if (h < 1 || h > 12) return false;
        if (pm && h != 12) h += 12;
        if (am && h == 12) h = 0;
    }
    minute = m;
    second = sec;
    hour = h;
    return hour >= 0 && hour < 24 && minute >= 0 && minute < 60 && second >= 0 && second < 60;
}

} // namespace

void Shell::cmd_rem(const Invocation&) {}

void Shell::cmd_ver(const Invocation&) {
    out_line("Windows 95 OPK3. [Version 4.10.1053]");
}

void Shell::cmd_cls(const Invocation&) {
    platform::clear_screen();
}

void Shell::cmd_exit(const Invocation&) {
    if (permanent_) return;
    exit_ = true;
}

void Shell::cmd_pause(const Invocation&) {
    out("Press any key to continue . . .");
    platform::flush(out_);
    platform::read_key(in_);
    out_line();
}

void Shell::cmd_break(const Invocation& inv) {
    if (inv.tail.empty()) {
        out_line("BREAK is " + current_state(break_));
        return;
    }
    bool value = false;
    if (!parse_on_off(text::trim(inv.tail), value)) {
        message("Must specify ON or OFF");
        return;
    }
    break_ = value;
}

void Shell::cmd_verify(const Invocation& inv) {
    if (inv.tail.empty()) {
        out_line("VERIFY is " + current_state(verify_));
        return;
    }
    bool value = false;
    if (!parse_on_off(text::trim(inv.tail), value)) {
        message("Must specify ON or OFF");
        return;
    }
    verify_ = value;
}

void Shell::cmd_lfnfor(const Invocation& inv) {
    if (inv.tail.empty()) {
        out_line("LFNFOR is " + current_state(lfnfor_));
        return;
    }
    bool value = false;
    if (!parse_on_off(text::trim(inv.tail), value)) {
        message("Must specify ON or OFF");
        return;
    }
    lfnfor_ = value;
}

void Shell::cmd_echo(const Invocation& inv) {
    if (inv.tail.empty()) {
        out_line(echo_ ? "ECHO is on" : "ECHO is off");
        return;
    }
    if (text::iequals(inv.tail, "ON")) {
        echo_ = true;
        return;
    }
    if (text::iequals(inv.tail, "OFF")) {
        echo_ = false;
        return;
    }
    if (inv.tail == "." || inv.tail == ":" || inv.tail == "/" || inv.tail == "\\") {
        out_line();
        return;
    }
    out_line(inv.tail);
}

void Shell::cmd_set(const Invocation& inv) {
    if (inv.tail.empty()) {
        for (const auto& kv : env_.list()) out_line(kv.first + "=" + kv.second);
        return;
    }
    auto eq = inv.tail.find('=');
    if (eq == std::string::npos) {
        const std::string prefix = text::upper(inv.tail);
        for (const auto& kv : env_.list()) {
            if (text::istarts_with(kv.first, prefix)) out_line(kv.first + "=" + kv.second);
        }
        return;
    }
    std::string name = inv.tail.substr(0, eq);
    std::string value = inv.tail.substr(eq + 1);
    if (name.empty()) {
        message("Invalid parameter");
        return;
    }
    if (value.empty()) {
        env_.erase(name);
        return;
    }
    env_.set(name, value);
}

void Shell::cmd_prompt(const Invocation& inv) {
    if (inv.tail.empty()) {
        env_.set("PROMPT", "$P$G");
        return;
    }
    env_.set("PROMPT", inv.tail);
}

void Shell::cmd_path(const Invocation& inv) {
    const std::string arg = std::string(text::trim(inv.tail));
    if (arg.empty()) {
        auto value = env_.get("PATH");
        if (value && !value->empty()) {
            out_line("PATH=" + *value);
        } else {
            out_line("No Path");
        }
        return;
    }
    if (arg == ";" || arg == "=") {
        env_.erase("PATH");
        return;
    }
    bool invalid = false;
    char separator = ';';
#if !defined(_WIN32)
    if (arg.find(';') == std::string::npos) separator = ':';
#endif
    std::size_t start = 0;
    while (start <= arg.size()) {
        std::size_t next = arg.find(separator, start);
        std::string component = arg.substr(start, next == std::string::npos ? std::string::npos : next - start);
        start = (next == std::string::npos) ? arg.size() + 1 : next + 1;
        if (component.size() == 1 && std::isalpha(static_cast<unsigned char>(component[0]))) continue; // "C:" alone
        if (component.empty()) continue;
        bool bad = false;
        resolve_path(component, bad);
        if (bad) {
            invalid = true;
            break;
        }
    }
    if (invalid) {
        message("Invalid drive in search path");
        return;
    }
    env_.set("PATH", arg);
}

void Shell::cmd_ctty(const Invocation& inv) {
    if (inv.tokens.empty()) {
        message("Required parameter missing");
        return;
    }
    if (text::iequals(inv.tokens[0], "CON")) return;
    message("Invalid device");
}

void Shell::cmd_chcp(const Invocation& inv) {
    if (inv.tokens.empty()) {
        out_line("Active code page: " + std::to_string(code_page_));
        return;
    }
    const std::string& text_value = inv.tokens[0];
    bool numeric = !text_value.empty() &&
                   std::all_of(text_value.begin(), text_value.end(),
                               [](char c) { return std::isdigit(static_cast<unsigned char>(c)) != 0; });
    if (!numeric) {
        message("Invalid code page");
        return;
    }
    int value = std::atoi(text_value.c_str());
    if (value == 437 || value == 850 || value == 852 || value == 866) {
        code_page_ = value;
        out_line("Active code page: " + std::to_string(code_page_));
        return;
    }
    message("Invalid code page");
}

void Shell::cmd_vol(const Invocation& inv) {
    std::string drive = platform::drive_letter();
    if (!inv.tokens.empty()) {
        const std::string arg = strip_quotes(inv.tokens[0]);
        if (arg.size() >= 2 && arg[1] == ':') {
            drive = std::string(1, static_cast<char>(std::toupper(static_cast<unsigned char>(arg[0]))));
        } else if (arg.size() == 1 && std::isalpha(static_cast<unsigned char>(arg[0]))) {
            drive = std::string(1, static_cast<char>(std::toupper(static_cast<unsigned char>(arg[0]))));
        } else if (!arg.empty()) {
            message("Invalid drive specification");
            return;
        }
    }
    if (drive != platform::drive_letter()) {
        message("Invalid drive specification");
        return;
    }
    auto info = platform::volume_info(drive + ":");
    if (info.has_label) {
        out_line(" Volume in drive " + drive + " is " + info.label);
    } else {
        out_line(" Volume in drive " + drive + " has no label");
    }
    if (info.has_serial) out_line(" Volume Serial Number is " + info.serial);
}

void Shell::cmd_chdir(const Invocation& inv) {
    if (inv.tokens.empty()) {
        out_line(current_dir_display());
        return;
    }
    std::string arg = strip_quotes(inv.tokens[0]);
    if (arg.size() == 2 && arg[1] == ':') {
        if (std::toupper(static_cast<unsigned char>(arg[0])) != platform::drive_letter()[0]) {
            message("Invalid drive specification");
            return;
        }
        out_line(current_dir_display());
        return;
    }
    bool bad = false;
    auto path = resolve_path(arg, bad);
    if (bad) {
        message("Invalid drive specification");
        return;
    }
    std::error_code ec;
    if (!std::filesystem::is_directory(path, ec)) {
        message("Invalid directory");
        return;
    }
    if (!platform::set_cwd(path.string())) message("Invalid directory");
}

void Shell::cmd_mkdir(const Invocation& inv) {
    if (inv.tokens.empty()) {
        message("Required parameter missing");
        return;
    }
    bool bad = false;
    auto path = resolve_path(inv.tokens[0], bad);
    if (bad) {
        message("Invalid drive specification");
        return;
    }
    std::error_code ec;
    if (std::filesystem::exists(path, ec)) {
        message("Directory already exists");
        return;
    }
    if (!std::filesystem::create_directory(path, ec) || ec) {
        message("Unable to create directory");
    }
}

void Shell::cmd_rmdir(const Invocation& inv) {
    if (inv.tokens.empty()) {
        message("Required parameter missing");
        return;
    }
    bool bad = false;
    auto path = resolve_path(inv.tokens[0], bad);
    if (bad) {
        message("Invalid drive specification");
        return;
    }
    std::error_code ec;
    auto target = std::filesystem::weakly_canonical(path, ec);
    auto current = std::filesystem::weakly_canonical(std::filesystem::current_path(), ec);
    if (!target.empty() && !current.empty()) {
        if (target == current) {
            message("Attempt to remove current directory");
            return;
        }
        auto it_target = target.begin();
        auto it_current = current.begin();
        bool prefix = true;
        for (; it_target != target.end(); ++it_target, ++it_current) {
            if (it_current == current.end() || *it_target != *it_current) {
                prefix = false;
                break;
            }
        }
        if (prefix) {
            message("Attempt to remove current directory");
            return;
        }
    }
    if (!std::filesystem::is_directory(path, ec)) {
        message("Invalid path, not directory, or directory not empty");
        return;
    }
    std::filesystem::remove(path, ec);
    if (ec) message("Invalid path, not directory, or directory not empty");
}

void Shell::cmd_type(const Invocation& inv) {
    if (inv.tokens.empty()) {
        message("Required parameter missing");
        return;
    }
    bool bad = false;
    auto path = resolve_path(inv.tokens[0], bad);
    if (bad) {
        message("Invalid drive specification");
        return;
    }
    std::error_code ec;
    if (!std::filesystem::exists(path, ec)) {
        message("File not found");
        return;
    }
    if (std::filesystem::is_directory(path, ec)) {
        message("Access denied");
        return;
    }
    std::FILE* file = std::fopen(path.string().c_str(), "rb");
    if (!file) {
        message("Access denied");
        return;
    }
    std::string buffer;
    buffer.resize(65536);
    for (;;) {
        std::size_t n = std::fread(buffer.data(), 1, buffer.size(), file);
        if (n == 0) break;
        auto end = std::find(buffer.begin(), buffer.begin() + static_cast<std::ptrdiff_t>(n), '\x1a');
        out(std::string_view(buffer.data(), static_cast<std::size_t>(end - buffer.begin())));
        if (end != buffer.begin() + static_cast<std::ptrdiff_t>(n)) break;
    }
    std::fclose(file);
}

void Shell::cmd_truename(const Invocation& inv) {
    if (inv.tokens.empty()) {
        message("Required parameter missing");
        return;
    }
    bool bad = false;
    auto path = resolve_path(inv.tokens[0], bad);
    if (bad) {
        message("Invalid drive specification");
        return;
    }
    std::error_code ec;
    auto parent = path.parent_path();
    if (!std::filesystem::exists(path, ec) && (parent.empty() || !std::filesystem::exists(parent, ec))) {
        message("Invalid path or file name");
        return;
    }
    out_line(display_path(path));
}

void Shell::cmd_call(const Invocation& inv) {
    if (inv.tail.empty()) return;
    bool saved = call_active_;
    call_active_ = true;
    execute_line(inv.tail);
    call_active_ = saved;
}

void Shell::cmd_shift(const Invocation&) {
    if (batches_.empty()) return;
    auto& args = batches_.back().args;
    if (args.size() > 1) args.erase(args.begin() + 1);
}

void Shell::cmd_goto(const Invocation& inv) {
    if (batches_.empty()) return;
    if (inv.tokens.empty()) {
        message("Syntax error");
        return;
    }
    const std::string label = inv.tokens[0];
    auto& frame = batches_.back();
    for (std::size_t i = 0; i < frame.lines.size(); ++i) {
        std::string_view line = frame.lines[i];
        std::size_t p = 0;
        while (p < line.size() && (line[p] == ' ' || line[p] == '\t')) ++p;
        if (p >= line.size() || line[p] != ':') continue;
        std::size_t start = p + 1;
        while (start < line.size() && (line[start] == ' ' || line[start] == '\t')) ++start;
        std::size_t end = start;
        while (end < line.size() && !text::is_delim(line[end])) ++end;
        if (text::iequals(line.substr(start, end - start), label)) {
            frame.next = i + 1;
            return;
        }
    }
    message("Label not found");
    abort_batch_chain();
}

void Shell::cmd_if(const Invocation& inv) {
    std::string_view s = inv.tail;
    const std::size_t n = s.size();
    auto skip_space = [&](std::size_t i) {
        while (i < n && (s[i] == ' ' || s[i] == '\t')) ++i;
        return i;
    };

    bool negate = false;
    std::size_t i = skip_space(0);
    std::string token;
    for (;;) {
        if (i >= n) {
            message("Syntax error");
            return;
        }
        std::size_t j = i;
        while (j < n && !text::is_delim(s[j])) ++j;
        token = std::string(s.substr(i, j - i));
        if (text::iequals(token, "NOT")) {
            negate = !negate;
            i = skip_space(j);
            continue;
        }
        break;
    }

    bool condition = false;
    std::size_t rest = 0;

    if (text::iequals(token, "ERRORLEVEL")) {
        std::size_t i2 = skip_space(static_cast<std::size_t>(s.find(token, i) + token.size()));
        std::size_t j = i2;
        while (j < n && !text::is_delim(s[j])) ++j;
        std::string number(s.substr(i2, j - i2));
        bool numeric = !number.empty() &&
                       std::all_of(number.begin(), number.end(),
                                   [](char c) { return std::isdigit(static_cast<unsigned char>(c)) != 0; });
        if (!numeric) {
            message("Syntax error");
            return;
        }
        condition = error_level_ >= std::atoi(number.c_str());
        rest = j;
    } else if (text::iequals(token, "EXIST")) {
        std::size_t i2 = skip_space(static_cast<std::size_t>(s.find(token, i) + token.size()));
        std::size_t j = i2;
        while (j < n && !text::is_delim(s[j])) ++j;
        std::string name(s.substr(i2, j - i2));
        if (name.empty()) {
            message("Syntax error");
            return;
        }
        bool bad = false;
        auto path = resolve_path(name, bad);
        std::error_code ec;
        condition = !bad && std::filesystem::exists(path, ec);
        rest = j;
    } else {
        std::size_t eq = s.find('=', i);
        if (eq == std::string_view::npos || eq + 1 >= n || s[eq + 1] != '=') {
            message("Syntax error");
            return;
        }
        std::string left = std::string(text::trim(s.substr(i, eq - i)));
        std::size_t r = skip_space(eq + 2);
        std::size_t j = r;
        while (j < n && !text::is_delim(s[j])) ++j;
        std::string right(s.substr(r, j - r));
        if (right.empty()) {
            message("Syntax error");
            return;
        }
        condition = text::iequals(left, right);
        rest = j;
    }

    std::size_t command_start = skip_space(rest);
    if (command_start >= n) return;
    std::string command(s.substr(command_start));
    if (condition != negate) execute_line(command);
}

void Shell::cmd_for(const Invocation& inv) {
    if (for_active_) {
        message("FOR cannot be nested");
        return;
    }
    std::string_view s = inv.tail;
    const std::size_t n = s.size();
    auto skip_space = [&](std::size_t i) {
        while (i < n && (s[i] == ' ' || s[i] == '\t')) ++i;
        return i;
    };

    std::size_t i = skip_space(0);
    std::size_t j = i;
    while (j < n && !(s[j] == ' ' || s[j] == '\t')) ++j;
    std::string var_token(s.substr(i, j - i));
    if (var_token.size() != 2 || var_token[0] != '%') {
        message("Syntax error");
        return;
    }
    const char variable = var_token[1];

    i = skip_space(j);
    j = i;
    while (j < n && !(s[j] == ' ' || s[j] == '\t')) ++j;
    if (!text::iequals(s.substr(i, j - i), "IN")) {
        message("Syntax error");
        return;
    }

    i = skip_space(j);
    if (i >= n || s[i] != '(') {
        message("Syntax error");
        return;
    }
    std::size_t close = s.find(')', i + 1);
    if (close == std::string_view::npos) {
        message("Syntax error");
        return;
    }
    std::string list(s.substr(i + 1, close - i - 1));

    i = skip_space(close + 1);
    j = i;
    while (j < n && !(s[j] == ' ' || s[j] == '\t')) ++j;
    if (!text::iequals(s.substr(i, j - i), "DO")) {
        message("Syntax error");
        return;
    }

    i = skip_space(j);
    if (i >= n) {
        message("Syntax error");
        return;
    }
    std::string command(s.substr(i));

    std::vector<std::string> items = text::split_list(list);
    std::vector<std::string> expanded;
    for (const auto& item : items) {
        if (!text::has_wildcard(item)) {
            expanded.push_back(item);
            continue;
        }
        bool bad = false;
        auto path = resolve_path(item, bad);
        if (bad) continue;
        std::string leaf = path.filename().string();
        auto dir = path.parent_path();
        if (dir.empty()) dir = std::filesystem::current_path();
        std::string prefix = item.substr(0, item.size() - leaf.size());
        auto entries = detail::collect_entries(dir, leaf);
        std::sort(entries.begin(), entries.end(),
                  [](const detail::Entry& a, const detail::Entry& b) {
                      return text::upper(a.name) < text::upper(b.name);
                  });
        for (const auto& entry : entries) expanded.push_back(prefix + entry.name);
    }

    for_active_ = true;
    for (const auto& item : expanded) {
        std::string substituted;
        const std::string marker = std::string("%") + variable;
        for (std::size_t p = 0; p < command.size();) {
            if (command.compare(p, marker.size(), marker) == 0) {
                substituted += item;
                p += marker.size();
            } else {
                substituted.push_back(command[p]);
                ++p;
            }
        }
        if (echo_) {
            out(prompt_text());
            out_line(substituted);
        }
        execute_line(substituted);
        if (exit_) break;
    }
    for_active_ = false;
}

void Shell::cmd_date(const Invocation& inv) {
    auto shown = now();
    out_line("Current date is " + text::date_text(shown));
    std::string prompt = "Enter new date (mm-dd-yy): ";
    if (!inv.tokens.empty()) {
        prompt.clear();
    }
    std::string input;
    if (inv.tokens.empty()) {
        out(prompt);
        platform::flush(out_);
        auto line = platform::read_line(in_);
        if (!line) return;
        input = *line;
    } else {
        input = text::join(inv.tokens, ' ');
    }
    if (text::trim(input).empty()) return;
    int year = 0, month = 0, day = 0;
    if (!parse_date_text(input, year, month, day)) {
        message("Invalid date");
        return;
    }
    auto real = platform::local_clock();
    shown.year = year;
    shown.month = month;
    shown.day = day;
    clock_offset_ = platform::epoch_seconds(shown) - platform::epoch_seconds(real);
}

void Shell::cmd_time(const Invocation& inv) {
    auto shown = now();
    out_line("Current time is " + text::time_text(shown));
    std::string prompt = "Enter new time: ";
    if (!inv.tokens.empty()) prompt.clear();
    std::string input;
    if (inv.tokens.empty()) {
        out(prompt);
        platform::flush(out_);
        auto line = platform::read_line(in_);
        if (!line) return;
        input = *line;
    } else {
        input = text::join(inv.tokens, ' ');
    }
    if (text::trim(input).empty()) return;
    int hour = 0, minute = 0, second = 0;
    if (!parse_time_text(input, hour, minute, second)) {
        message("Invalid time");
        return;
    }
    auto real = platform::local_clock();
    shown.hour = hour;
    shown.minute = minute;
    shown.second = second;
    clock_offset_ = platform::epoch_seconds(shown) - platform::epoch_seconds(real);
}

void Shell::cmd_loadhigh(const Invocation& inv) {
    std::vector<std::string> args;
    for (const auto& token : inv.tokens) {
        if (is_switch(token)) continue;
        args.push_back(token);
    }
    if (args.empty()) {
        message("Required parameter missing");
        return;
    }
    std::string command = text::join(args, ' ');
    if (text::iequals(std::filesystem::path(args[0]).extension().string(), ".bat")) {
        message("Cannot Loadhigh batch file");
        return;
    }
    execute_line(command);
}

void Shell::cmd_erase(const Invocation& inv) {
    bool prompt_each = false;
    std::vector<std::string> specs;
    for (const auto& token : inv.tokens) {
        if (is_switch(token)) {
            if (text::iequals(token, "/P")) prompt_each = true;
            continue;
        }
        specs.push_back(token);
    }
    if (specs.empty()) {
        message("Required parameter missing");
        return;
    }
    for (const auto& spec : specs) {
        auto parsed = detail::parse_spec(*this, spec);
        if (!parsed.valid) {
            message(parsed.error);
            continue;
        }
        auto entries = detail::collect_entries(parsed.dir, parsed.pattern);
        std::vector<detail::Entry> files;
        for (auto& entry : entries)
            if (!entry.is_dir) files.push_back(entry);
        if (files.empty()) {
            message("File not found");
            continue;
        }
        const bool all_files = parsed.pattern == "*.*" || parsed.pattern == "*";
        if (!prompt_each && all_files) {
            out_line("All files in directory will be deleted!");
            if (!detail::confirm(*this, "Are you sure (Y/N)?")) continue;
        }
        for (const auto& entry : files) {
            if (prompt_each) {
                if (!detail::confirm(*this, entry.name + ",    Delete (Y/N)?")) continue;
            }
            std::error_code ec;
            if (!std::filesystem::remove(entry.path, ec) || ec) message("Access denied");
        }
    }
}

void Shell::cmd_rename(const Invocation& inv) {
    if (inv.tokens.size() < 2) {
        message("Required parameter missing");
        return;
    }
    if (inv.tokens.size() > 2) {
        message("Too many parameters");
        return;
    }
    const std::string source = strip_quotes(inv.tokens[0]);
    const std::string target = strip_quotes(inv.tokens[1]);
    if (target.find(':') != std::string::npos || target.find('\\') != std::string::npos ||
        target.find('/') != std::string::npos) {
        message("Invalid path or file name");
        return;
    }
    auto parsed = detail::parse_spec(*this, source);
    if (!parsed.valid) {
        message(parsed.error);
        return;
    }
    auto entries = detail::collect_entries(parsed.dir, parsed.pattern);
    if (entries.empty()) {
        message("Duplicate file name or file not found");
        return;
    }
    for (const auto& entry : entries) {
        const std::string new_name =
            text::has_wildcard(target) ? text::wildcard_transform(entry.name, parsed.pattern, target) : target;
        auto destination = parsed.dir / new_name;
        std::error_code ec;
        if (std::filesystem::exists(destination, ec)) {
            message("Duplicate file name or file not found");
            continue;
        }
        std::filesystem::rename(entry.path, destination, ec);
        if (ec) message("Duplicate file name or file not found");
    }
}

} // namespace cmdpp
