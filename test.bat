@echo off
rem Builds the 2024 rules unit tests and runs them (no game needed).
setlocal
set VCVARS=C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvars32.bat
if not exist "%VCVARS%" goto :no_vcvars
call "%VCVARS%" >nul || exit /b 1
cd /d "%~dp0"
if not exist build\obj\rules mkdir build\obj\rules

cl /nologo /O2 /MT /W3 /EHsc /std:c++17 /Isrc /Itests /Fobuild\obj\rules\ ^
  tests\rules_main.cpp tests\rules\*.cpp src\rules\*.cpp ^
  /Fe:build\rules_test.exe >build\obj\rules\compile.log || goto :compile_failed

build\rules_test.exe
exit /b %errorlevel%

:compile_failed
type build\obj\rules\compile.log
exit /b 1

:no_vcvars
echo vcvars32.bat not found: "%VCVARS%"
exit /b 1
