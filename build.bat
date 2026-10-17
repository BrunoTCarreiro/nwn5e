@echo off
rem Builds build\nwn5e.exe and build\nwn5e.dll (32-bit, static CRT) with VS 2019 Build Tools.
setlocal
set VCVARS=C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvars32.bat
if not exist "%VCVARS%" goto :no_vcvars
call "%VCVARS%" >nul || exit /b 1
cd /d "%~dp0"
if not exist build mkdir build
if not exist build\obj mkdir build\obj

set CFLAGS=/nologo /O2 /MT /W3 /EHsc /std:c++17 /DWIN32_LEAN_AND_MEAN /DUNICODE /D_UNICODE /Fobuild\obj\

cl %CFLAGS% /LD /Ithird_party\minhook\include ^
  src\core\dllmain.cpp src\core\log.cpp src\core\config.cpp src\core\hooks.cpp ^
  src\core\rules_crit.cpp src\core\rules_crit_confirm.cpp ^
  third_party\minhook\src\buffer.c third_party\minhook\src\hook.c ^
  third_party\minhook\src\trampoline.c third_party\minhook\src\hde\hde32.c ^
  /Fe:build\nwn5e.dll /link /DLL user32.lib || exit /b 1

cl %CFLAGS% src\loader\main.cpp /Fe:build\nwn5e.exe ^
  /link /SUBSYSTEM:WINDOWS user32.lib advapi32.lib shell32.lib || exit /b 1

copy /y nwn5e.ini build\nwn5e.ini >nul
echo built build\nwn5e.exe build\nwn5e.dll
exit /b 0

:no_vcvars
echo vcvars32.bat not found: "%VCVARS%"
exit /b 1
