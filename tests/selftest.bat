@echo off
echo SELFTEST-BEGIN
set TST=ok
if "%TST%"=="ok" echo SET-OK
if exist selftest.bat echo EXIST-OK
if not exist nope.zzz echo NOTEXIST-OK
for %%f in (selftest.bat) do echo FOR-OK %%f
call child.bat ARG1
echo CALL-RETURNED
goto after
echo GOTO-FAIL
:after
echo GOTO-OK
cmd /c exit 7
if errorlevel 7 echo ERRLVL-OK
if errorlevel 8 echo ERRLVL-FAIL
echo 100%%
echo SELFTEST-END
