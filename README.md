# commandpp

A portable, from-scratch C++20 implementation of the command-interpreter behaviour of **Windows 95 COMMAND.COM**. The primary compatibility reference is Windows 95 OPK3, version **4.10.1053**.

The project implements the shell's command dispatch, batch processing, environment handling, redirection and pipelines, together with a set of built-in commands. It builds as a native executable on Windows and POSIX systems; it does not emulate a 16-bit DOS environment or depend on DOS interrupts.

## Features

- Interactive shell and `/C` / `/K` command execution.
- Batch files, parameters (`%0`–`%9`), environment expansion, `CALL`, `GOTO`, `SHIFT`, `IF` and `FOR`.
- Input/output redirection (`<`, `>`, `>>`) and pipelines (`|`).
- DOS-style command lookup, environment variables, prompt handling and path syntax.
- Native execution of external programs through the host platform.

### Built-in commands

`CD`/`CHDIR`, `MD`/`MKDIR`, `RD`/`RMDIR`, `DIR`, `COPY`, `DEL`/`ERASE`, `REN`/`RENAME`, `TYPE`, `SET`, `PATH`, `PROMPT`, `ECHO`, `DATE`, `TIME`, `VER`, `VOL`, `CLS`, `BREAK`, `VERIFY`, `CHCP`, `CTTY`, `TRUENAME`, `LOADHIGH`/`LH`, `LFNFOR`, `CALL`, `GOTO`, `SHIFT`, `IF`, `FOR`, `PAUSE`, `REM` and `EXIT`.

Command-specific help is available with `/?`.

## Build

### CMake

Requires CMake 3.20 or later and a C++20 compiler.

```sh
cmake -S . -B build
cmake --build build
```

### Linux / Android (Termux)

```sh
./build-linux.sh
```

The script uses `clang++`, `g++` or `c++`, in that order. Set `CXX` to select a compiler explicitly.

## Usage

```text
commandpp [[drive:]path] [device] [/E:nnnnn] [/L:nnnn] [/U:nnn] [/P] [/MSG]
          [/LOW] [/Y [/C|K command]]
```

- `/C command`: execute a command and exit.
- `/K command`: execute a command, then continue interactively.
- Without either switch, start an interactive session.

## Compatibility notes

The implementation aims to reproduce observable COMMAND.COM behaviour, not the behaviour of modern `cmd.exe`. This includes batch-only `%VAR%` expansion, batch parameter handling, command-specific parsing, DOS-style wildcard matching, positional `COPY /A` and `/B` modes, and temporary-file-based pipelines.

Platform differences remain:

- The portable shell exposes a single drive, mapped to the host filesystem.
- POSIX host `PATH` entries are translated to and from DOS-style path-list syntax.
- Long filenames are used directly; synthetic 8.3 short filenames are not provided.
- `DATE` and `TIME` do not generally change the host system clock.
- `CHCP` accepts a limited set of code pages but does not switch the host encoding.
- DOS-specific memory models, resident/transient relocation, ROM support, and Win95-specific disk hooks are outside the current scope.

## Tests

Run the platform-appropriate test suite after building:

```sh
bash tests/run-tests.sh ./commandpp
```

On Windows:

```powershell
powershell -File tests\run-tests.ps1 -Exe .\commandpp.exe
```

The tests cover batch control flow, environment expansion, redirection, pipelines, file commands, reverse directory sorting and selected `COPY` semantics.

## Attribution

This is an independent implementation. See [NOTICE](NOTICE) for attribution and trademark information.
