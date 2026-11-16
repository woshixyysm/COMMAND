#include <cstdio>
#include <string>

#include "cmdpp/env.hpp"
#include "cmdpp/platform.hpp"
#include "cmdpp/shell.hpp"
#include "cmdpp/text.hpp"

namespace {

std::string join_from(int argc, char** argv, int start) {
    std::string result;
    for (int i = start; i < argc; ++i) {
        if (!result.empty()) result.push_back(' ');
        result += argv[i];
    }
    return result;
}

void print_command_help() {
    auto text = cmdpp::command_help("COMMAND");
    std::fwrite(text.data(), 1, text.size(), stdout);
}

} // namespace

int main(int argc, char** argv) {
    cmdpp::Environment env = cmdpp::Environment::from_host();
    cmdpp::Shell shell(std::move(env), cmdpp::platform::executable_path());

    bool permanent = false;
    bool single = false;
    bool keep = false;
    bool help = false;
    std::string command;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg.empty()) continue;
        if (arg[0] != '/' && arg[0] != '-') {
            // [drive:]path or device - accepted and ignored (portable shell).
            continue;
        }
        const std::string up = cmdpp::text::upper(arg.substr(1));
        if (up == "?") {
            help = true;
        } else if (up == "C" || up == "K") {
            command = join_from(argc, argv, i + 1);
            single = up == "C";
            keep = up == "K";
            break;
        } else if (up == "P") {
            permanent = true;
        } else if (up == "LOW" || up == "MSG" || up == "Y") {
            // Accepted for compatibility; no effect.
        } else if (up.rfind("E:", 0) == 0 || up.rfind("L:", 0) == 0 || up.rfind("U:", 0) == 0) {
            // Buffer/environment sizing switches; no effect.
        }
    }

    if (help) {
        print_command_help();
        return 0;
    }

    shell.set_permanent(permanent);

    if (single) return shell.run_single(command);
    shell.show_banner();
    if (keep) {
        shell.run_single(command);
        if (shell.exit_requested()) return shell.exit_code();
    }
    return shell.run_interactive();
}
