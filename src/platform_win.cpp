#include "cmdpp/platform.hpp"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <conio.h>
#include <direct.h>
#include <io.h>

#include <cctype>
#include <cstring>
#include <vector>

namespace cmdpp::platform {
namespace {

HANDLE handle_of(std::FILE* f) {
    return reinterpret_cast<HANDLE>(_get_osfhandle(_fileno(f)));
}

} // namespace

std::string cwd() {
    char buf[MAX_PATH * 4];
    DWORD n = GetCurrentDirectoryA(static_cast<DWORD>(sizeof buf), buf);
    if (n == 0 || n >= sizeof buf) return {};
    return std::string(buf, n);
}

bool set_cwd(const std::string& native) {
    return SetCurrentDirectoryA(native.c_str()) != 0;
}

std::string drive_letter() {
    std::string dir = cwd();
    if (dir.size() >= 2 && dir[1] == ':')
        return std::string(1, static_cast<char>(std::toupper(static_cast<unsigned char>(dir[0]))));
    return "C";
}

bool set_clock(const Clock& c) {
    SYSTEMTIME st{};
    st.wYear = static_cast<WORD>(c.year);
    st.wMonth = static_cast<WORD>(c.month);
    st.wDay = static_cast<WORD>(c.day);
    st.wHour = static_cast<WORD>(c.hour);
    st.wMinute = static_cast<WORD>(c.minute);
    st.wSecond = static_cast<WORD>(c.second);
    return SetLocalTime(&st) != 0;
}

VolumeInfo volume_info(const std::string& drive) {
    VolumeInfo info;
    std::string root = drive;
    if (root.empty()) root = drive_letter() + ":";
    if (root.back() != '\\') root += '\\';
    char label[MAX_PATH + 1] = {0};
    DWORD serial = 0;
    if (GetVolumeInformationA(root.c_str(), label, sizeof label, &serial, nullptr, nullptr, nullptr, 0)) {
        if (label[0]) {
            info.has_label = true;
            info.label = label;
        }
        info.has_serial = true;
        char buf[32];
        std::snprintf(buf, sizeof buf, "%04X-%04X", static_cast<unsigned>(serial >> 16),
                      static_cast<unsigned>(serial & 0xFFFFu));
        info.serial = buf;
    }
    return info;
}

FileAttrs file_attributes(const std::filesystem::path& p) {
    FileAttrs attrs;
    DWORD a = GetFileAttributesA(p.string().c_str());
    if (a != INVALID_FILE_ATTRIBUTES) {
        attrs.readonly = (a & FILE_ATTRIBUTE_READONLY) != 0;
        attrs.hidden = (a & FILE_ATTRIBUTE_HIDDEN) != 0;
        attrs.system = (a & FILE_ATTRIBUTE_SYSTEM) != 0;
        attrs.archive = (a & FILE_ATTRIBUTE_ARCHIVE) != 0;
    }
    return attrs;
}

void clear_screen() {
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO info{};
    if (h == INVALID_HANDLE_VALUE || !GetConsoleScreenBufferInfo(h, &info)) {
        std::fputs("\x1b[2J\x1b[H", stdout);
        std::fflush(stdout);
        return;
    }
    DWORD cells = static_cast<DWORD>(info.dwSize.X) * static_cast<DWORD>(info.dwSize.Y);
    COORD home{0, 0};
    DWORD written = 0;
    FillConsoleOutputCharacterA(h, ' ', cells, home, &written);
    FillConsoleOutputAttribute(h, info.wAttributes, cells, home, &written);
    SetConsoleCursorPosition(h, home);
}

int read_key(std::FILE* f) {
    if (!is_tty(f)) {
        int c = std::fgetc(f);
        return c == EOF ? 0 : c;
    }
    int c = _getch();
    if (c == 0 || c == 0xE0) {
        _getch();
        return 0;
    }
    return c;
}

std::string temp_file(const std::string& prefix) {
    char dir[MAX_PATH * 4];
    DWORD n = GetTempPathA(static_cast<DWORD>(sizeof dir), dir);
    std::string base = (n > 0 && n < sizeof dir) ? std::string(dir) : std::string(".");
    char name[MAX_PATH];
    std::string p = prefix;
    p.resize(3, 'x');
    if (GetTempFileNameA(base.c_str(), p.substr(0, 3).c_str(), 0, name)) return std::string(name);
    return {};
}

std::string executable_path() {
    char buf[MAX_PATH * 4];
    DWORD n = GetModuleFileNameA(nullptr, buf, static_cast<DWORD>(sizeof buf));
    return std::string(buf, n);
}

SpawnResult spawn(const SpawnRequest& req) {
    SpawnResult result;
    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof sa;
    sa.bInheritHandle = TRUE;

    STARTUPINFOA si{};
    si.cb = sizeof si;
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = handle_of(req.in ? req.in : stdin);
    si.hStdOutput = handle_of(req.out ? req.out : stdout);
    si.hStdError = handle_of(req.err ? req.err : stderr);

    std::string cmdline = "\"" + req.program + "\"";
    if (!req.args.empty()) {
        cmdline += ' ';
        cmdline += req.args;
    }
    std::vector<char> cmd(cmdline.begin(), cmdline.end());
    cmd.push_back('\0');

    std::vector<char> envblock;
    for (const auto& entry : req.env) {
        envblock.insert(envblock.end(), entry.begin(), entry.end());
        envblock.push_back('\0');
    }
    envblock.push_back('\0');

    std::string cwd_text = req.cwd.empty() ? cwd() : req.cwd;

    PROCESS_INFORMATION pi{};
    BOOL ok = CreateProcessA(req.program.c_str(), cmd.data(), &sa, &sa, TRUE, 0, envblock.data(),
                             cwd_text.c_str(), &si, &pi);
    if (!ok) return result;

    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD code = 0;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);

    result.started = true;
    result.exit_code = static_cast<int>(code);
    return result;
}

} // namespace cmdpp::platform
