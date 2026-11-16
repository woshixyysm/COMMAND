#include "cmdpp/platform.hpp"

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

#include <cerrno>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <vector>

namespace cmdpp::platform {
namespace {

std::string sanitize_prefix(const std::string& prefix) {
    std::string p;
    for (char c : prefix) {
        if (std::isalnum(static_cast<unsigned char>(c))) p.push_back(c);
        if (p.size() >= 8) break;
    }
    if (p.empty()) p = "cmd";
    return p;
}

std::vector<std::string> split_args(const std::string& text) {
    std::vector<std::string> out;
    std::string cur;
    bool quoted = false;
    bool have = false;
    for (char c : text) {
        if (c == '"') {
            quoted = !quoted;
            have = true;
            continue;
        }
        if (!quoted && (c == ' ' || c == '\t')) {
            if (have) {
                out.push_back(cur);
                cur.clear();
                have = false;
            }
            continue;
        }
        cur.push_back(c);
        have = true;
    }
    if (have) out.push_back(cur);
    return out;
}

} // namespace

std::string cwd() {
    char buf[4096];
    if (!getcwd(buf, sizeof buf)) return {};
    return std::string(buf);
}

bool set_cwd(const std::string& native) {
    return chdir(native.c_str()) == 0;
}

std::string drive_letter() {
    return "C";
}

bool set_clock(const Clock&) {
    return false; // not portable without privileges
}

VolumeInfo volume_info(const std::string&) {
    return VolumeInfo{};
}

FileAttrs file_attributes(const std::filesystem::path& p) {
    FileAttrs attrs;
    const std::string name = p.filename().string();
    attrs.hidden = !name.empty() && name[0] == '.';
    struct stat st {};
    if (::stat(p.c_str(), &st) == 0) {
        attrs.readonly = (st.st_mode & S_IWUSR) == 0;
        attrs.system = false;
        attrs.archive = S_ISREG(st.st_mode);
    }
    return attrs;
}

void clear_screen() {
    std::fputs("\x1b[2J\x1b[H", stdout);
    std::fflush(stdout);
}

int read_key(std::FILE* f) {
    if (!is_tty(f)) {
        int c = std::fgetc(f);
        return c == EOF ? 0 : c;
    }
    termios oldt{};
    tcgetattr(STDIN_FILENO, &oldt);
    termios raw = oldt;
    raw.c_lflag &= static_cast<tcflag_t>(~(ICANON | ECHO));
    tcsetattr(STDIN_FILENO, TCSANOW, &raw);
    int c = std::fgetc(stdin);
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    return c == EOF ? 0 : c;
}

std::string temp_file(const std::string& prefix) {
    std::vector<std::string> dirs;
    for (const char* name : {"TMPDIR", "TMP", "TEMP"}) {
        const char* value = std::getenv(name);
        if (value && *value) dirs.emplace_back(value);
    }
    const char* termux_prefix = std::getenv("PREFIX");
    if (termux_prefix && *termux_prefix) dirs.emplace_back(std::string(termux_prefix) + "/tmp");
    dirs.emplace_back("/tmp");
    dirs.emplace_back(".");
    for (const auto& dir : dirs) {
        std::string tmpl = dir + "/" + sanitize_prefix(prefix) + "XXXXXX";
        std::vector<char> buf(tmpl.begin(), tmpl.end());
        buf.push_back('\0');
        int fd = mkstemp(buf.data());
        if (fd >= 0) {
            close(fd);
            return std::string(buf.data());
        }
    }
    return {};
}

std::string executable_path() {
    char buf[4096];
    ssize_t n = readlink("/proc/self/exe", buf, sizeof buf - 1);
    if (n <= 0) return {};
    buf[n] = '\0';
    return std::string(buf);
}

SpawnResult spawn(const SpawnRequest& req) {
    SpawnResult result;
    int errpipe[2];
    if (pipe(errpipe) != 0) return result;
    fcntl(errpipe[1], F_SETFD, FD_CLOEXEC);

    std::vector<std::string> args = split_args(req.args);
    std::vector<std::string> argv_storage;
    argv_storage.push_back(req.program);
    for (auto& a : args) argv_storage.push_back(a);
    std::vector<char*> argv;
    for (auto& s : argv_storage) argv.push_back(s.data());
    argv.push_back(nullptr);

    std::vector<char*> envp;
    for (auto& s : req.env) envp.push_back(const_cast<char*>(s.c_str()));
    envp.push_back(nullptr);

    std::fflush(nullptr); // do not duplicate buffered shell output in the child
    pid_t pid = fork();
    if (pid < 0) {
        close(errpipe[0]);
        close(errpipe[1]);
        return result;
    }
    if (pid == 0) {
        close(errpipe[0]);
        if (!req.cwd.empty() && chdir(req.cwd.c_str()) != 0) {
            int err = errno;
            ssize_t ignored = ::write(errpipe[1], &err, sizeof err);
            (void)ignored;
            _exit(127);
        }
        std::FILE* in = req.in ? req.in : stdin;
        std::FILE* out = req.out ? req.out : stdout;
        std::FILE* err = req.err ? req.err : stderr;
        if (dup2(fileno(in), STDIN_FILENO) < 0 || dup2(fileno(out), STDOUT_FILENO) < 0 ||
            dup2(fileno(err), STDERR_FILENO) < 0) {
            int error = errno;
            ssize_t ignored = ::write(errpipe[1], &error, sizeof error);
            (void)ignored;
            _exit(127);
        }
        execve(req.program.c_str(), argv.data(), envp.data());
        int error = errno;
        ssize_t ignored = ::write(errpipe[1], &error, sizeof error);
        (void)ignored;
        _exit(127);
    }

    close(errpipe[1]);
    int exec_error = 0;
    ssize_t n = read(errpipe[0], &exec_error, sizeof exec_error);
    close(errpipe[0]);

    int status = 0;
    while (waitpid(pid, &status, 0) < 0 && errno == EINTR) {
    }
    if (n > 0) return result; // exec failed; shell treats it as a bad command
    result.started = true;
    result.exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : 1;
    return result;
}

} // namespace cmdpp::platform
