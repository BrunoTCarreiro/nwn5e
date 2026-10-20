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

rem The private layer (docs/PRIVATE-LAYER.md): personal-use rules kept out of
rem this public repo, compiled in when private\ exists. "build.bat public"
rem leaves it out - build any DLL that leaves this machine that way.
set PRIVATE_SRC=
set PRIVATE_DEF=
if /i not "%~1"=="public" if exist private\src\*.cpp set PRIVATE_SRC=private\src\*.cpp
if /i not "%~1"=="public" if exist private\rules\*.cpp set PRIVATE_SRC=%PRIVATE_SRC% private\rules\*.cpp
if defined PRIVATE_SRC set PRIVATE_DEF=/DNWN5E_PRIVATE /Iprivate

cl %CFLAGS% /LD /Ithird_party\minhook\include /Isrc %PRIVATE_DEF% ^
  src\core\dllmain.cpp src\core\log.cpp src\core\config.cpp src\core\hooks.cpp ^
  src\core\rules_crit.cpp src\core\rules_crit_confirm.cpp src\core\rules_attack.cpp ^
  src\core\rules_extra_attack.cpp src\core\rules_attack_trace.cpp src\core\rules_retarget.cpp ^
  src\core\rules_sheet.cpp src\core\creature.cpp src\core\engine_queries.cpp src\core\rules_advantage.cpp src\core\rules_racial.cpp src\core\rules_saves.cpp src\core\rules_save_roll.cpp src\core\rules_chat.cpp src\core\rules_opportunity.cpp src\core\rules_two_weapon.cpp src\core\rules_mastery.cpp src\core\engine_effects.cpp src\core\rules_labels.cpp src\core\rules_conditions.cpp src\core\rules_potion.cpp src\core\creature_update.cpp src\core\rules_skills.cpp src\core\rules_perception.cpp src\core\rules_skill_dcs.cpp src\core\rules_spell_attack.cpp src\core\rules_casting.cpp src\core\rules_shove.cpp src\core\rules_initiative.cpp src\core\rules_nwn_skills.cpp src\core\rules_progression.cpp src\core\rules_loaded.cpp src\core\rules_alignment.cpp src\core\rules_hit_points.cpp src\core\data_serving.cpp src\data\twoda.cpp src\data\game_resources.cpp ^
  src\rules\core.cpp src\rules\combat.cpp src\rules\monster.cpp src\rules\nwn_conversion.cpp ^
  src\rules\d20.cpp src\rules\nwn_situational.cpp src\rules\modes.cpp src\rules\saves.cpp src\rules\conditions.cpp src\rules\skills.cpp src\rules\text.cpp src\rules\weapons.cpp src\rules\progression.cpp src\rules\hit_points.cpp ^
  %PRIVATE_SRC% third_party\minhook\src\buffer.c third_party\minhook\src\hook.c ^
  third_party\minhook\src\trampoline.c third_party\minhook\src\hde\hde32.c ^
  /Fe:build\nwn5e.dll /link /DLL user32.lib || exit /b 1

rem nwmain.exe's manifest requires administrator, and a non-elevated process
rem cannot create it (error 740), so the loader asks for the same rights.
cl %CFLAGS% src\loader\main.cpp /Fe:build\nwn5e.exe ^
  /link /SUBSYSTEM:WINDOWS /MANIFEST:EMBED /MANIFESTUAC:"level='requireAdministrator' uiAccess='false'" ^
  user32.lib advapi32.lib shell32.lib || exit /b 1

cl %CFLAGS% tests\inject_test.cpp /Fe:build\inject_test.exe /link /SUBSYSTEM:CONSOLE || exit /b 1

rem nwn5e's data (data\README.md), fresh next to the DLL; the private layer's
rem data only in a private build.
if exist build\data rmdir /s /q build\data
xcopy /e /i /q /y data build\data >nul || exit /b 1
if /i not "%~1"=="public" if exist private\data xcopy /e /i /q /y private\data build\data\private >nul

rem build\nwn5e.ini holds the player's settings: only create it, never overwrite it.
if not exist build\nwn5e.ini copy nwn5e.ini build\nwn5e.ini >nul
if defined PRIVATE_SRC echo built build\nwn5e.exe build\nwn5e.dll WITH the private layer (personal use only)
if not defined PRIVATE_SRC echo built build\nwn5e.exe build\nwn5e.dll
exit /b 0

:no_vcvars
echo vcvars32.bat not found: "%VCVARS%"
exit /b 1
