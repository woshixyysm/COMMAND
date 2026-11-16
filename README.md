# commandpp

A portable, from-scratch C++20 reimplementation of the command-interpreter behaviour of **Windows 95 COMMAND.COM** (reference build: 4.10.1053).

The project implements the command parser, internal commands, batch-file execution, environment handling, redirection and pipelines. It runs as a native program on Windows and POSIX systems; it does not emulate 16-bit DOS or call DOS interrupts.

## Build

A C++20 compiler and CMake 3.20 or newer are required.

### Windows

```powershell
cmake -S . -B build
cmake --build build --config Release
```

Run the test suite with:

```powershell
powershell -File tests\run-tests.ps1 -Exe .\build\Release\commandpp.exe
```

### Linux and Android (Termux)

```sh
./build-linux.sh
bash tests/run-tests.sh ./commandpp
```

The build script uses `clang++`, `g++` or `c++`, in that order, unless `CXX` is set.

## Usage

```text
commandpp [/P] [/C command | /K command] [/?]
```

- `/C command` executes one command or batch file and exits.
- `/K command` executes the command, then starts the interactive shell.
- `/P` enables permanent mode, in which `EXIT` does not terminate the shell.
- `/?` displays command help.

With no command switch, commandpp starts an interactive session.

## Implemented commands

The current implementation includes:

- **File and directory:** `DIR`, `COPY`, `DEL`/`ERASE`, `REN`/`RENAME`, `TYPE`, `CD`/`CHDIR`, `MD`/`MKDIR`, `RD`/`RMDIR`, `TRUENAME`
- **Batch and flow control:** `CALL`, `GOTO`, `SHIFT`, `IF`, `FOR`, `PAUSE`, `REM`, `ECHO`
- **Environment and session:** `SET`, `PATH`, `PROMPT`, `DATE`, `TIME`, `VER`, `VOL`, `CLS`, `EXIT`, `BREAK`, `VERIFY`, `CHCP`, `CTTY`, `LOADHIGH`/`LH`, `LFNFOR`

External programs are resolved through the current directory and `PATH`. On POSIX systems, executable names are resolved using the host environment.

## Compatibility notes

The goal is to reproduce observable COMMAND.COM behaviour, not to provide a modern `cmd.exe` replacement. Examples of DOS-era semantics retained by the implementation include:

- Percent-variable and batch-argument expansion in batch files.
- Batch control flow and the distinction between nested `CALL` and batch replacement.
- COMMAND.COM-style redirection and pipe parsing.
- DOS-style `COPY` options, concatenation and Ctrl-Z text-file handling.
- DOS command names, switches and error messages where implemented.

The implementation is still a compatibility work in progress. Host filesystem and process behaviour necessarily differ from real DOS/Windows 95 in some areas.

## Platform limitations

- The portable shell exposes a single logical `C:` drive. On POSIX, it maps to the host root.
- POSIX paths and host process execution are adapted to the shell's DOS-style command environment.
- Long filenames are used directly; synthetic 8.3 aliases are not provided.
- `CHCP` accepts a limited set of code-page numbers but does not change the host process encoding.
- Date/time changes are session-local where the host does not permit changing the system clock.
- DOS memory management, resident/transient relocation, ROM builds, and legacy disk hooks are outside the current scope.

## Source layout

- `include/cmdpp/` — public project headers.
- `src/` — portable shell, command implementations and platform backends.
- `tests/` — Windows and POSIX behaviour tests.

### About the local `real/` directory

In the development workspace, `real/` refers to the separate MS-DOS 7 reference-source tree used for source-level comparison. It is not part of commandpp's implementation, is not required by the build or tests, and is not included in this repository. The portable implementation remains in `src/`; reference material is kept separate.

## Notices

See [NOTICE](NOTICE) for project attribution and trademark information.
