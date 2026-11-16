#pragma once

#include <cstdio>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "cmdpp/env.hpp"
#include "cmdpp/platform.hpp"

namespace cmdpp {

struct Redirection {
    bool in_set = false;
    bool out_set = false;
    bool append = false;
    std::string in_file;
    std::string out_file;
};

struct Invocation {
    std::string name;
    std::string tail;              // raw text after the command name
    std::vector<std::string> tokens; // tokenized tail, quotes removed
};

struct BatchFrame {
    std::string display;           // batch name as typed (%0)
    std::filesystem::path native;  // resolved path
    std::vector<std::string> lines;
    std::size_t next = 0;
    std::vector<std::string> args; // %0 .. %N
    bool echo_saved = true;
    std::FILE* owned_in = nullptr; // redirection inherited from the invoking line
    std::FILE* owned_out = nullptr;
};

class Shell {
public:
    Shell(Environment env, std::string self_path);

    int run_interactive();
    int run_single(const std::string& line);

    void set_permanent(bool value) { permanent_ = value; }
    int exit_code() const { return error_level_ & 0xFF; }
    bool exit_requested() const { return exit_; }

    // Startup copyright banner, printed when the interpreter runs interactively
    // (the original prints it for a new shell, but not for /C).
    void show_banner();

    // Current clock, honouring DATE/TIME changes made in this session.
    platform::Clock now() const;

    // ---- used by command implementations ----
    Environment& env() { return env_; }
    std::FILE* in() const { return in_; }
    std::FILE* out_stream() const { return out_; }

    void out(std::string_view s);
    void out_line(std::string_view s = {});
    void message(std::string_view s) { out_line(s); }
    void set_streams(std::FILE* in, std::FILE* out) {
        in_ = in;
        out_ = out;
    }
    std::FILE* in_stream() const { return in_; }

    bool echo() const { return echo_; }
    void set_echo(bool v) { echo_ = v; }
    bool verify() const { return verify_; }
    void set_verify(bool v) { verify_ = v; }
    bool break_flag() const { return break_; }
    void set_break_flag(bool v) { break_ = v; }
    bool lfnfor() const { return lfnfor_; }
    void set_lfnfor(bool v) { lfnfor_ = v; }
    int code_page() const { return code_page_; }
    void set_code_page(int v) { code_page_ = v; }

    int error_level() const { return error_level_; }
    void set_error_level(int v) { error_level_ = v; }

    std::string prompt_text() const;
    std::string current_dir_display() const;
    std::string display_path(const std::filesystem::path& p) const;

    // Resolves a DOS style path. Sets drive_bad when the drive is not the
    // current one (the portable shell only knows one drive).
    std::filesystem::path resolve_path(std::string_view dos_text, bool& drive_bad) const;

    bool in_batch() const { return !batches_.empty(); }

    // ---- internal commands ----
    using CommandFn = void (Shell::*)(const Invocation&);
    struct CommandSpec {
        const char* name;
        CommandFn fn;
        const char* help_key;
        bool limit_help;
    };

    void cmd_dir(const Invocation&);
    void cmd_call(const Invocation&);
    void cmd_chcp(const Invocation&);
    void cmd_rename(const Invocation&);
    void cmd_erase(const Invocation&);
    void cmd_type(const Invocation&);
    void cmd_rem(const Invocation&);
    void cmd_copy(const Invocation&);
    void cmd_pause(const Invocation&);
    void cmd_date(const Invocation&);
    void cmd_time(const Invocation&);
    void cmd_ver(const Invocation&);
    void cmd_vol(const Invocation&);
    void cmd_chdir(const Invocation&);
    void cmd_mkdir(const Invocation&);
    void cmd_rmdir(const Invocation&);
    void cmd_break(const Invocation&);
    void cmd_verify(const Invocation&);
    void cmd_set(const Invocation&);
    void cmd_prompt(const Invocation&);
    void cmd_path(const Invocation&);
    void cmd_exit(const Invocation&);
    void cmd_ctty(const Invocation&);
    void cmd_echo(const Invocation&);
    void cmd_goto(const Invocation&);
    void cmd_shift(const Invocation&);
    void cmd_if(const Invocation&);
    void cmd_for(const Invocation&);
    void cmd_cls(const Invocation&);
    void cmd_truename(const Invocation&);
    void cmd_loadhigh(const Invocation&);
    void cmd_lfnfor(const Invocation&);

private:
    struct ProgramHit {
        std::filesystem::path path;
        bool batch = false;
    };

    // execution plumbing
    void execute_line(const std::string& line);
    void execute_segment(std::string_view text, std::FILE* fin, std::FILE* fout);
    void dispatch(const std::string& name, const std::string& tail);
    std::vector<std::string> split_pipes(std::string_view line, bool& error) const;
    void run_pipeline(const std::vector<std::string>& segments);

    // batch processing
    bool step_batch();
    void pop_batch();
    void abort_batch_chain();
    bool begin_batch(const std::string& display, const std::string& arg_tail, bool via_call);
    std::string expand_batch_line(std::string_view raw) const;

    // external commands
    std::optional<ProgramHit> find_program(const std::string& name) const;
    void run_external(const std::string& name, const std::string& tail);

    // helpers
    void print_help(const char* key);
    bool help_requested(const Invocation& inv, const CommandSpec& spec) const;
    static const CommandSpec* find_command(std::string_view name);
    static const CommandSpec* find_attached(std::string_view token, std::string& remainder);

    Environment env_;
    std::string self_path_;
    std::FILE* in_ = stdin;
    std::FILE* out_ = stdout;
    bool echo_ = true;
    bool verify_ = false;
    bool break_ = false;
    bool lfnfor_ = true;
    bool exit_ = false;
    bool permanent_ = false;
    bool call_active_ = false;
    bool for_active_ = false;
    int error_level_ = 0;
    int code_page_ = 437;
    int64_t clock_offset_ = 0;
    unsigned long long batch_generation_ = 0;
    std::vector<BatchFrame> batches_;
};

// Help text for `/?` (implemented in help.cpp).
std::string_view command_help(std::string_view key);

} // namespace cmdpp
