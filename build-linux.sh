#!/usr/bin/env bash
# Builds the portable COMMAND.COM port on Linux (also works in Termux).
# Usage: ./build-linux.sh [output-name]
set -euo pipefail
cd "$(dirname "$0")"

OUT="${1:-commandpp}"
SOURCES=(
    src/platform_common.cpp
    src/platform_posix.cpp
    src/env.cpp
    src/text.cpp
    src/shell.cpp
    src/commands.cpp
    src/commands_file.cpp
    src/commands_copy.cpp
    src/help.cpp
    src/main.cpp
)

CXX_BIN="${CXX:-}"
if [ -z "$CXX_BIN" ]; then
    for candidate in clang++ g++ c++; do
        if command -v "$candidate" >/dev/null 2>&1; then
            CXX_BIN="$candidate"
            break
        fi
    done
fi
if [ -z "$CXX_BIN" ]; then
    echo "error: no C++ compiler found (clang++/g++/c++)" >&2
    exit 1
fi

echo "compiler: $("$CXX_BIN" --version | head -n 1)"
"$CXX_BIN" -std=c++20 -O2 -Wall -Wextra -Iinclude -Isrc "${SOURCES[@]}" -o "$OUT"
echo "built ./$OUT"
