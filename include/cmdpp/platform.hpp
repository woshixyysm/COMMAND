// Minimal OS abstraction used by the portable COMMAND.COM reimplementation.
#pragma once

#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace cmdpp::platform {

struct Clock {
    int year = 1970, month = 1, day = 1;
    int hour = 0, minute = 0, second = 0, hundredths = 0;
    int weekday = 0; // 0 = Sunday
};

struct FileAttrs {
    bool readonly = false;
    bool hidden = false;
    bool system = false;
    bool archive = false;
};

struct VolumeInfo {
    bool has_label = false;
    std::string label;
    bool has_serial = false;
    std::string serial;
};

struct SpawnRequest {
    std::string program;           // native absolute path
    std::string args;              // DOS style argument tail
    std::string cwd;               // native working directory
    std::vector<std::string> env;  // "NAME=VALUE" entries
    std::FILE* in = nullptr;
    std::FILE* out = nullptr;
    std::FILE* err = nullptr;
};

struct SpawnResult {
    bool started = false;
    int exit_code = 0;
};

Clock local_clock();
int64_t epoch_seconds(const Clock& c);
Clock from_epoch(int64_t seconds);

void write(std::FILE* f, std::string_view s);
void flush(std::FILE* f);
std::optional<std::string> read_line(std::FILE* f);
int read_key(std::FILE* f);
bool is_tty(std::FILE* f);

std::string cwd();
bool set_cwd(const std::string& native);
std::string drive_letter();
bool set_clock(const Clock& c);
VolumeInfo volume_info(const std::string& drive);
FileAttrs file_attributes(const std::filesystem::path& p);
void clear_screen();
std::string temp_file(const std::string& prefix);
std::string executable_path();
SpawnResult spawn(const SpawnRequest& req);

} // namespace cmdpp::platform
