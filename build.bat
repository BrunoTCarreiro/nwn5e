@echo off
rem Builds build\nwn5e.exe and build\nwn5e.dll (32-bit, static CRT) with VS 2019 Build Tools.
setlocal
set VCVARS=C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvars32.bat
if not exist "%VCVARS%" goto :no_vcvars
call "%VCVARS%" >nul || exit /b 1
cd /d "%~dp0"
if not exist build mkdir build
if not exist build\obj mkdir build\obj

set CFLAGS=/nologo /O2 /MT /W3 /EHsc /std:c++17 /DWIN32_LEAN_AND_MEAN /DNOMINMAX /DUNICODE /D_UNICODE /Fobuild\obj\

cl %CFLAGS% /LD /Ithird_party\minhook\include /Isrc ^
  src\core\dllmain.cpp src\core\log.cpp src\core\config.cpp src\core\hooks.cpp ^
  src\core\rules_crit.cpp src\core\rules_crit_confirm.cpp src\core\rules_attack.cpp ^
  src\core\rules_extra_attack.cpp src\core\rules_attack_trace.cpp src\core\rules_retarget.cpp ^
  src\core\rules_sheet.cpp src\core\creature.cpp ^
  src\rules\core.cpp src\rules\combat.cpp src\rules\monster.cpp src\rules\nwn_conversion.cpp ^
  third_party\minhook\src\buffer.c third_party\minhook\src\hook.c ^
  third_party\minhook\src\trampoline.c third_party\minhook\src\hde\hde32.c ^
  /Fe:build\nwn5e.dll /link /DLL user32.lib || exit /b 1

rem nwmain.exe's manifest requires administrator, and a non-elevated process
rem cannot create it (error 740), so the loader asks for the same rights.
cl %CFLAGS% src\loader\main.cpp /Fe:build\nwn5e.exe ^
  /link /SUBSYSTEM:WINDOWS /MANIFEST:EMBED /MANIFESTUAC:"level='requireAdministrator' uiAccess='false'" ^
  user32.lib advapi32.lib shell32.lib || exit /b 1

cl %CFLAGS% tests\inject_test.cpp /Fe:build\inject_test.exe /link /SUBSYSTEM:CONSOLE || exit /b 1

rem build\nwn5e.ini holds the player's settings: only create it, never overwrite it.
if not exist build\nwn5e.ini copy nwn5e.ini build\nwn5e.ini >nul
echo built build\nwn5e.exe build\nwn5e.dll
exit /b 0

:no_vcvars
echo vcvars32.bat not found: "%VCVARS%"
exit /b 1
