# Validates the modern COMMAND.COM port against observable DOS behaviour.
# Usage: powershell -File run-tests.ps1 [-Exe .\commandpp.exe]
param(
    [string]$Exe = (Join-Path (Split-Path -Parent $MyInvocation.MyCommand.Path) '..\commandpp.exe')
)

$ErrorActionPreference = 'Stop'
$testsDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$exe = (Resolve-Path $Exe).Path
$failures = 0

function Assert-Contains {
    param([string]$Name, [string[]]$Output, [string]$Needle)
    if (($Output -join "`n") -notlike "*$Needle*") {
        Write-Host "FAIL: $Name (missing '$Needle')" -ForegroundColor Red
        $script:failures++
    } else {
        Write-Host "ok:   $Name" -ForegroundColor Green
    }
}

function Assert-NotContains {
    param([string]$Name, [string[]]$Output, [string]$Needle)
    if (($Output -join "`n") -like "*$Needle*") {
        Write-Host "FAIL: $Name (unexpected '$Needle')" -ForegroundColor Red
        $script:failures++
    } else {
        Write-Host "ok:   $Name" -ForegroundColor Green
    }
}

function Assert-Equal {
    param([string]$Name, $Actual, $Expected)
    if ("$Actual" -ne "$Expected") {
        Write-Host "FAIL: $Name (got '$Actual', expected '$Expected')" -ForegroundColor Red
        $script:failures++
    } else {
        Write-Host "ok:   $Name" -ForegroundColor Green
    }
}

Push-Location $testsDir
try {
    $output = & $exe /C 'selftest.bat' 2>&1
    $code = $LASTEXITCODE
    Assert-Contains 'selftest: set/if' $output 'SET-OK'
    Assert-Contains 'selftest: if exist' $output 'EXIST-OK'
    Assert-Contains 'selftest: if not exist' $output 'NOTEXIST-OK'
    Assert-Contains 'selftest: for' $output 'FOR-OK selftest.bat'
    Assert-Contains 'selftest: call' $output 'CHILD ARG1'
    Assert-Contains 'selftest: call returns' $output 'CALL-RETURNED'
    Assert-Contains 'selftest: goto' $output 'GOTO-OK'
    Assert-NotContains 'selftest: goto skips' $output 'GOTO-FAIL'
    Assert-Contains 'selftest: errorlevel' $output 'ERRLVL-OK'
    Assert-NotContains 'selftest: errorlevel lower' $output 'ERRLVL-FAIL'
    Assert-Contains 'selftest: percent escape' $output '100%'
    Assert-Equal 'exit code 7' $code 7

    $output = & $exe /C 'echo hi there > redirect.out'
    $content = Get-Content 'redirect.out' -Raw
    if ($content -match 'hi there') { Write-Host 'ok:   redirect >' -ForegroundColor Green }
    else { Write-Host 'FAIL: redirect >' -ForegroundColor Red; $failures++ }

    & $exe /C 'echo second >> redirect.out'
    $lines = Get-Content 'redirect.out'
    if (($lines -join "`n") -match 'second') { Write-Host 'ok:   redirect >>' -ForegroundColor Green }
    else { Write-Host 'FAIL: redirect >>' -ForegroundColor Red; $failures++ }

    $output = & $exe /C 'dir /b | sort'
    Assert-Contains 'pipe' $output 'selftest.bat'

    $output = & $exe /C 'type nope.txt'
    Assert-Contains 'type missing' $output 'File not found'

    & $exe /C 'copy child.bat copied.bat /Y' | Out-Null
    if (Test-Path 'copied.bat') { Write-Host 'ok:   copy' -ForegroundColor Green }
    else { Write-Host 'FAIL: copy' -ForegroundColor Red; $failures++ }

    & $exe /C 'ren copied.bat renamed.bat' | Out-Null
    if ((Test-Path 'renamed.bat') -and -not (Test-Path 'copied.bat')) { Write-Host 'ok:   rename' -ForegroundColor Green }
    else { Write-Host 'FAIL: rename' -ForegroundColor Red; $failures++ }

    & $exe /C 'del renamed.bat' | Out-Null
    if (-not (Test-Path 'renamed.bat')) { Write-Host 'ok:   delete' -ForegroundColor Green }
    else { Write-Host 'FAIL: delete' -ForegroundColor Red; $failures++ }

    # ---- DIR reverse sorting (strict weak ordering) ---------------------
    Set-Content -Path 'so_a.txt' -Value 'A'
    Set-Content -Path 'so_b.txt' -Value 'B'
    Set-Content -Path 'so_c.txt' -Value 'C'
    $sorted = & $exe /C 'dir /b /o-n so_*.txt'
    Assert-Equal 'dir /o-n reverse name sort' ($sorted -join ',') 'so_c.txt,so_b.txt,so_a.txt'

    # ---- COPY semantics -------------------------------------------------
    New-Item -ItemType Directory -Force 'copyd' | Out-Null
    [IO.File]::WriteAllText((Join-Path $testsDir 'copyd\a.txt'), "AAA`n")
    [IO.File]::WriteAllText((Join-Path $testsDir 'copyd\b.txt'), "BBB`n")
    Remove-Item 'a.txt', 'b.txt', 'joined.txt', 'out_b.bin', 'out_a.txt', 'out_pos.txt' -ErrorAction SilentlyContinue

    & $exe /C 'copy copyd\*.txt' | Out-Null
    Assert-Equal 'copy wildcard/no dest: a.txt' ((Get-Content 'a.txt' -Raw) -replace "`r?`n", '') 'AAA'
    Assert-Equal 'copy wildcard/no dest: b.txt' ((Get-Content 'b.txt' -Raw) -replace "`r?`n", '') 'BBB'
    Assert-Equal 'copy wildcard/no dest: source intact' ((Get-Content 'copyd\a.txt' -Raw) -replace "`r?`n", '') 'AAA'

    & $exe /C 'copy copyd\*.txt joined.txt /Y' | Out-Null
    Assert-Contains 'copy wildcard to file concatenates' (Get-Content 'joined.txt' -Raw) 'AAA'

    $output = & $exe /C 'copy copyd\a.txt copyd\b.txt copyd\a.txt' 2>&1
    Assert-Contains 'copy too many parameters' $output 'Too many parameters'

    [IO.File]::WriteAllBytes((Join-Path $testsDir 'bin.dat'), [byte[]](65, 66, 26, 67, 68, 26))
    & $exe /C 'copy /b bin.dat out_b.bin' | Out-Null
    Assert-Equal 'copy /b keeps ^Z bytes' ([IO.File]::ReadAllBytes((Join-Path $testsDir 'out_b.bin')).Length) 6
    & $exe /C 'copy /a bin.dat out_a.txt' | Out-Null
    Assert-Equal 'copy /a stops at ^Z' ([IO.File]::ReadAllBytes((Join-Path $testsDir 'out_a.txt')).Length) 3
    & $exe /C 'copy /b bin.dat /a out_pos.txt' | Out-Null
    Assert-Equal 'copy /A /B are positional' ([IO.File]::ReadAllBytes((Join-Path $testsDir 'out_pos.txt')).Length) 7

    Set-Content -Path 'copycmd.bat' -Value @('@echo off', 'set COPYCMD=/Y', 'copy copyd\a.txt joined.txt')
    'N' | & $exe /C 'copycmd.bat' | Out-Null
    Assert-Equal 'COPYCMD=/Y applies inside batch' ((Get-Content 'joined.txt' -Raw) -replace "`r?`n", '') 'AAA'
} finally {
    Remove-Item 'redirect.out', 'copied.bat', 'renamed.bat', 'a.txt', 'b.txt', 'joined.txt',
        'out_b.bin', 'out_a.txt', 'out_pos.txt', 'bin.dat', 'copycmd.bat',
        'so_a.txt', 'so_b.txt', 'so_c.txt' -ErrorAction SilentlyContinue
    Remove-Item 'copyd' -Recurse -Force -ErrorAction SilentlyContinue
    Pop-Location
}

if ($failures -gt 0) {
    Write-Host "$failures test(s) failed" -ForegroundColor Red
    exit 1
}
Write-Host 'All tests passed' -ForegroundColor Green
exit 0
