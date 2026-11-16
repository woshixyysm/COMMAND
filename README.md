# commandpp — portable COMMAND.COM (core) in modern C++

A from-scratch, behaviour-compatible re-implementation of the **core** of
MS-DOS / Windows 95 `COMMAND.COM`: the internal commands, batch processing,
command-line prescan (redirection and pipes), environment handling and the
command prompt. It is written in portable C++20 and runs natively on Windows
and POSIX systems (no 16-bit emulation, no `INT 21h`).

Reference sources used for the behaviour:

| Reference | Purpose |
|---|---|
| `..\cmd\command\tdATA.ASM` (`COMTAB`) | internal command table |
| `..\cmd\command\tbatch.asm`, `tbatch2.asm` | line reading, `%0-%9`, `%VAR%`, ECHO, GOTO, CALL, SHIFT, IF |
| `..\cmd\command\for.asm` | FOR syntax and substitution |
| `..\cmd\command\tmisc1.asm` | `PRESCAN` (quotes, `<`, `>`, `>>`, `|`) |
| `..\cmd\command\tpipe.asm` | pipe temp-file model, DATE/TIME |
| `..\cmd\command\dir.asm`, `copy.asm`, `tcmd1b.asm`, `tcmd2a/b.asm` | DIR/COPY/other internal commands |
| `..\cmd\command\tenv*.asm` | SET, PATH, PROMPT, CD/MD/RD |
| `COMMAND.COM` (Win95 4.10.1053) | message texts, `VER` string, `LFNFOR` |

`..\real\` is the unrelated DOSSHELL 2 source tree and is not used here.

## Build

### Windows (MinGW-w64 g++ / MSVC / clang-cl)

With MinGW-w64 g++ 13 (or any C++20 compiler):

```powershell
cd modern
g++ -std=c++20 -O2 -Wall -Wextra -Iinclude -Isrc `
    src/platform_common.cpp src/platform_win.cpp src/env.cpp src/text.cpp `
    src/shell.cpp src/commands.cpp src/commands_file.cpp src/commands_copy.cpp `
    src/help.cpp src/main.cpp -o commandpp.exe
```

or with CMake:

```powershell
cmake -S . -B build -G "MinGW Makefiles"
cmake --build build
```

MSVC / clang-cl use the same CMake project (`-G "Visual Studio 17 2022"` or `-T ClangCL`);
both compile clean apart from the CRT `fopen` deprecation notice.

### Linux / Android (Termux)

```bash
./build-linux.sh                 # picks clang++ if present, else g++
bash tests/run-tests.sh ./commandpp
```

`build-linux.sh` is a bash script; `tests/run-tests.sh` expects a POSIX shell. Push the
tree to a device and build as the Termux user so ownership stays correct:

```bash
adb push modern /data/data/com.termux/files/home/modern
adb shell chown -R u0_a391:u0_a391 /data/data/com.termux/files/home/modern
adb shell "su u0_a391 -c 'export PREFIX=/data/data/com.termux/files/usr \
  PATH=\$PREFIX/bin:/system/bin TMPDIR=\$PREFIX/tmp LD_LIBRARY_PATH=\$PREFIX/lib; \
  cd /data/data/com.termux/files/home/modern && CXX=clang++ bash build-linux.sh'"
```

Builds done as `u0_a391` already carry the right SELinux label (`u:object_r:app_data_file:s0`),
so no `restorecon`/`ctxfix` pass is needed; verify with
`restorecon -R -n /data/data/com.termux/files/home/modern`.

Verified on Windows (g++/cl, clang-cl) and on arm64 Android 16 / Termux clang 24: the
whole test suite passes on all of them.

## Usage

```
commandpp [[drive:]path] [device] [/E:nnnnn] [/L:nnnn] [/U:nnn] [/P] [/MSG]
          [/LOW] [/Y [/C|K command]]
```

`/C command` runs one command (batch files run to completion) and returns the
DOS error level. `/K command` runs it and keeps the shell. Everything else
starts an interactive session.

Running interactively prints the original two-line copyright banner first;
`/C` does not (same as `COMMAND.COM`). The banner text and attribution notes
are described in [NOTICE](NOTICE).

## Implemented internal commands

`DIR` (`/P /W /A /O /S /B /L /V`, `DIRCMD`), `COPY` (`/A /B /V /Y /-Y`,
`COPYCMD`, concatenation with `+`, `CON`/`NUL`), `DEL`/`ERASE` (`/P`),
`RENAME`/`REN` (wildcards), `TYPE`, `CD`/`CHDIR`, `MD`/`MKDIR`,
`RD`/`RMDIR`, `SET`, `PATH`, `PROMPT`, `ECHO`, `DATE`, `TIME`, `VER`,
`VOL`, `CLS`, `BREAK`, `VERIFY`, `CHCP`, `CTTY`, `TRUENAME`,
`LOADHIGH`/`LH`, `LFNFOR`, `CALL`, `GOTO`, `SHIFT`, `IF`, `FOR`, `PAUSE`,
`REM`, `EXIT`, and `/?` help for every command.

External `.COM`/`.EXE`/`.BAT` programs are located through the current
directory and `PATH` (COM, EXE, BAT order) and executed with the shell's
environment, working directory and redirected handles. On POSIX hosts there
are no DOS extensions, so a bare name (for example `ls`, `grep`, `sort`) is
resolved as-is through `PATH`; on Windows only programs that exist on the
Windows `PATH` can run (`ls` is therefore "Bad command or file name" unless a
POSIX toolset such as Git's `usr/bin` is on `PATH`).

## Behaviour kept from the original

These details follow the assembly source rather than modern `cmd.exe`:

* `%VAR%` expansion happens **only for batch lines**, not at the prompt.
  An undefined variable expands to nothing; `%%` collapses to `%`; a
  trailing unmatched `%` is dropped (and for `%name` without a closing `%`
  the `%` is dropped while the name is copied).
* `%0`..`%9` come from the current batch frame; `SHIFT` moves them.
* Batch lines are echoed as `prompt + expanded line` when `ECHO` is on;
  `@` suppresses the echo of that line; the `ECHO` state at batch entry is
  restored when the batch ends.
* `GOTO` re-scans the batch file from the beginning (labels are
  case-insensitive); a missing label prints `Label not found` and aborts
  the batch chain.
* A batch started from a batch **without** `CALL` replaces the caller;
  `CALL` nests and returns.
* `IF` compares raw strings case-insensitively; quotes are ordinary
  characters. `ERRORLEVEL n` tests `last >= n`. Only the exit code of an
  external program updates the error level, as in the original (internal
  commands do not).
* `FOR %x IN (set) DO cmd` requires the exact `IN`/`DO` tokens, expands
  wildcards through the filesystem, cannot be nested, and substitutes every
  `%x` in the command text. `%%x` is the batch form.
* `PRESCAN` removes `<`, `>`, `>>` and `|` anywhere in the line (quotes
  protect them only when the number of quotes is even), stops redirect
  file names at delimiters and `<`/`>`, and rejects quoted redirect names.
  Pipes run through temporary files, exactly like `TPIPE`.
* `COPY` follows `copypr2.asm`: the source list is split on `+`, a trailing
  name with no `+` before it is the destination, and more than one source
  without `+` is `Too many parameters`. Concatenation happens only when a
  `+` was used, or when a wildcard source is copied to a single unambiguous
  file destination. `COPY *.TXT` (no destination) copies every match into the
  current directory and never overwrites the source with a concatenation.
* `/A` and `/B` are positional and stateful, as in `Source_Set`/`SetAsc`:
  they affect the preceding name and every following name until the next
  switch. Default mode is binary; while concatenating the default becomes
  ASCII unless `/B` is given. ASCII sources stop at `^Z` and an ASCII
  destination gets a single trailing `^Z`.
* `COPYCMD` is consulted on every `COPY` execution, so a value set earlier in
  a batch (`SET COPYCMD=/Y`) applies to later `COPY` lines; an explicit
  `/Y` or `/-Y` on the command line wins.
* Redirection and pipes apply to internal command output and error messages
  as well ("all messages get redirected"), and a redirection attached to a
  batch invocation stays in effect for the whole batch.
* Pipes use temporary files: `%TEMP%` on Windows, `$TMPDIR` / `$PREFIX/tmp`
  / `/tmp` / `.` on POSIX, and they are always removed afterwards.
* `DATE`/`TIME` print `Current date is Mon 11-16-2026` /
  `Current time is 12:39:31.45` and prompt with `(mm-dd-yy)` / no format.
* `SET` lists sorted `NAME=VALUE`; names are stored upper-case but looked up
  case-insensitively. `PATH ;` clears the path.

## Platform notes / limitations

* The portable shell exposes a single drive, reported as `C:`. Other drive
  letters produce `Invalid drive specification`. On POSIX, `C:` maps to `/`,
  so `C:\tmp` is `/tmp` and the prompt shows `C:\home\user`.
* `PATH` is kept in DOS syntax (`;`) inside the shell; on POSIX the host
  `PATH` (`:`) is converted on import and converted back for child
  processes, so both `PATH=/usr/bin;/bin` and an inherited Unix `PATH` work.
* Long file names are used as-is; there are no synthetic 8.3 short names.
  DOS-style wildcard matching is approximated with the 8.3 rule.
* Setting the system clock is not portable: `DATE`/`TIME` changes are kept
  as a session-local offset (the OS clock is only changed on Windows when
  the process has the privilege).
* `VOL` reads the real volume label/serial on Windows; elsewhere it reports
  no label.
* Resident/transient relocation, self-checksum, ROM builds, DBCS code pages,
  `LOCK`/`UNLOCK` and the Win95 32-bit disk hooks are out of scope.
* `CHCP` reports 437/850/852/866 and rejects others (no code-page switching).

## Tests

```powershell
powershell -File tests\run-tests.ps1            # uses ..\commandpp.exe
powershell -File tests\run-tests.ps1 -Exe C:\path\to\commandpp.exe
```

The suite runs `tests\selftest.bat` (SET/IF/EXIST/FOR/CALL/GOTO/ERRORLEVEL/
`%%`), redirection (`>` and `>>`), a pipe, `TYPE` errors, and
`COPY`/`RENAME`/`DEL` round-trips.
