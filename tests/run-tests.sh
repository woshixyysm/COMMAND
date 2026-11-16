#!/usr/bin/env bash
# Validates the Linux/Termux build. Usage: bash run-tests.sh [path-to-commandpp]
set -u
here="$(cd "$(dirname "$0")" && pwd)"
exe="${1:-$here/../commandpp}"
case "$exe" in
    /*) ;;
    *) exe="$PWD/$exe" ;;
esac
if [ ! -x "$exe" ]; then
    echo "error: $exe is not executable" >&2
    exit 1
fi

failures=0

assert_contains() {
    local name="$1" haystack="$2" needle="$3"
    if [[ "$haystack" == *"$needle"* ]]; then
        echo "ok:   $name"
    else
        echo "FAIL: $name (missing '$needle')"
        failures=$((failures + 1))
    fi
}

assert_not_contains() {
    local name="$1" haystack="$2" needle="$3"
    if [[ "$haystack" == *"$needle"* ]]; then
        echo "FAIL: $name (unexpected '$needle')"
        failures=$((failures + 1))
    else
        echo "ok:   $name"
    fi
}

assert_equal() {
    local name="$1" actual="$2" expected="$3"
    if [ "$actual" = "$expected" ]; then
        echo "ok:   $name"
    else
        echo "FAIL: $name (got '$actual', expected '$expected')"
        failures=$((failures + 1))
    fi
}

assert_size() {
    local name="$1" file="$2" expected="$3"
    assert_equal "$name" "$(wc -c < "$file" | tr -d ' ')" "$expected"
}

cd "$here" || exit 1

output="$("$exe" /C selftest-posix.bat 2>&1)"
code=$?
assert_contains 'selftest: set/if' "$output" 'SET-OK'
assert_contains 'selftest: if exist' "$output" 'EXIST-OK'
assert_contains 'selftest: if not exist' "$output" 'NOTEXIST-OK'
assert_contains 'selftest: for' "$output" 'FOR-OK selftest-posix.bat'
assert_contains 'selftest: call' "$output" 'CHILD ARG1'
assert_contains 'selftest: call returns' "$output" 'CALL-RETURNED'
assert_contains 'selftest: goto' "$output" 'GOTO-OK'
assert_not_contains 'selftest: goto skips' "$output" 'GOTO-FAIL'
assert_contains 'selftest: errorlevel' "$output" 'ERRLVL-OK'
assert_not_contains 'selftest: errorlevel lower' "$output" 'ERRLVL-FAIL'
assert_contains 'selftest: percent escape' "$output" '100%'
assert_equal 'exit code 1' "$code" 1

"$exe" /C 'echo hi there > redirect.out' >/dev/null
if grep -q 'hi there' redirect.out; then echo 'ok:   redirect >'; else echo 'FAIL: redirect >'; failures=$((failures + 1)); fi
"$exe" /C 'echo second >> redirect.out' >/dev/null
if grep -q 'second' redirect.out; then echo 'ok:   redirect >>'; else echo 'FAIL: redirect >>'; failures=$((failures + 1)); fi
rm -f redirect.out

output="$("$exe" /C 'dir /b | sort' 2>&1)"
assert_contains 'pipe' "$output" 'selftest-posix.bat'

# External POSIX programs must be reachable through PATH.
output="$("$exe" /C 'ls' 2>&1)"
assert_contains 'external ls' "$output" 'selftest-posix.bat'

output="$("$exe" /C 'type nope.txt' 2>&1)"
assert_contains 'type missing' "$output" 'File not found'

"$exe" /C 'copy child.bat copied.bat /Y' >/dev/null
if [ -f copied.bat ]; then echo 'ok:   copy'; else echo 'FAIL: copy'; failures=$((failures + 1)); fi
"$exe" /C 'ren copied.bat renamed.bat' >/dev/null
if [ -f renamed.bat ] && [ ! -f copied.bat ]; then echo 'ok:   rename'; else echo 'FAIL: rename'; failures=$((failures + 1)); fi
"$exe" /C 'del renamed.bat' >/dev/null
if [ ! -f renamed.bat ]; then echo 'ok:   delete'; else echo 'FAIL: delete'; failures=$((failures + 1)); fi

# ---- DIR reverse sorting (strict weak ordering) -------------------------
printf 'A\n' > so_a.txt
printf 'B\n' > so_b.txt
printf 'C\n' > so_c.txt
sorted="$("$exe" /C 'dir /b /o-n so_*.txt' | tr '\n' ',' | sed 's/,$//')"
assert_equal 'dir /o-n reverse name sort' "$sorted" 'so_c.txt,so_b.txt,so_a.txt'

# ---- COPY semantics -----------------------------------------------------
mkdir -p copyd
printf 'AAA\n' > copyd/a.txt
printf 'BBB\n' > copyd/b.txt
rm -f a.txt b.txt joined.txt out_b.bin out_a.txt out_pos.txt

"$exe" /C 'copy copyd/*.txt' >/dev/null
assert_equal 'copy wildcard/no dest: a.txt' "$(tr -d '\r\n' < a.txt)" 'AAA'
assert_equal 'copy wildcard/no dest: b.txt' "$(tr -d '\r\n' < b.txt)" 'BBB'
assert_equal 'copy wildcard/no dest: source intact' "$(tr -d '\r\n' < copyd/a.txt)" 'AAA'

"$exe" /C 'copy copyd/*.txt joined.txt /Y' >/dev/null
assert_contains 'copy wildcard to file concatenates' "$(cat joined.txt)" 'AAA'

output="$("$exe" /C 'copy copyd/a.txt copyd/b.txt copyd/a.txt' 2>&1)"
assert_contains 'copy too many parameters' "$output" 'Too many parameters'

printf '\x41\x42\x1a\x43\x44\x1a' > bin.dat
"$exe" /C 'copy /b bin.dat out_b.bin' >/dev/null
assert_size 'copy /b keeps ^Z bytes' out_b.bin 6
"$exe" /C 'copy /a bin.dat out_a.txt' >/dev/null
assert_size 'copy /a stops at ^Z' out_a.txt 3
"$exe" /C 'copy /b bin.dat /a out_pos.txt' >/dev/null
assert_size 'copy /A /B are positional' out_pos.txt 7

printf '@echo off\nset COPYCMD=/Y\ncopy copyd/a.txt joined.txt\n' > copycmd.bat
printf 'N\n' | "$exe" /C 'copycmd.bat' >/dev/null
assert_equal 'COPYCMD=/Y applies inside batch' "$(tr -d '\r\n' < joined.txt)" 'AAA'

rm -rf copyd a.txt b.txt joined.txt out_b.bin out_a.txt out_pos.txt bin.dat copycmd.bat copied.bat renamed.bat so_a.txt so_b.txt so_c.txt

if [ "$failures" -gt 0 ]; then
    echo "$failures test(s) failed"
    exit 1
fi
echo 'All tests passed'
