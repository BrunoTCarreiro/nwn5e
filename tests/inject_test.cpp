// Checks the loader's injection path without starting the game: starts a
// 32-bit system program (SysWOW64\whoami.exe) through StartWithDll and
// expects nwn5e.log to show that the DLL loaded during initialisation and
// refused to hook a host that is not the mapped nwmain.exe.
//
// The DLL writes its log next to itself, so the test runs a copy from
// %TEMP%\nwn5e-inject-test and never touches the game's build\nwn5e.log.
//
//   build\inject_test.exe   (exit 0 = pass)

#include <windows.h>
#include <shlwapi.h>
#include <stdio.h>

#include <string>

#include "../src/loader/inject.h"

#pragma comment(lib, "shlwapi.lib")

int wmain() {
  wchar_t here[MAX_PATH];
  GetModuleFileNameW(nullptr, here, MAX_PATH);
  PathRemoveFileSpecW(here);
  wchar_t temp[MAX_PATH];
  GetTempPathW(MAX_PATH, temp);
  const std::wstring dir = std::wstring(temp) + L"nwn5e-inject-test";
  CreateDirectoryW(dir.c_str(), nullptr);
  const std::wstring dll = dir + L"\\nwn5e.dll";
  const std::wstring log = dir + L"\\nwn5e.log";
  if (!CopyFileW((std::wstring(here) + L"\\nwn5e.dll").c_str(), dll.c_str(), FALSE)) {
    wprintf(L"FAIL: could not copy nwn5e.dll to %s (error %lu)\n", dir.c_str(), GetLastError());
    return 1;
  }
  DeleteFileW(log.c_str());

  wchar_t windows[MAX_PATH];
  GetWindowsDirectoryW(windows, MAX_PATH);
  const std::wstring exe = std::wstring(windows) + L"\\SysWOW64\\whoami.exe";

  PROCESS_INFORMATION process = {};
  DWORD error = 0;
  if (const wchar_t* step = StartWithDll(exe, L"\"" + exe + L"\"", L"", dll, &process, &error)) {
    wprintf(L"FAIL: could not start %s with the DLL: %s failed (error %lu)\n", exe.c_str(), step, error);
    return 1;
  }
  WaitForSingleObject(process.hProcess, 15000);
  DWORD code = STILL_ACTIVE;
  GetExitCodeProcess(process.hProcess, &code);
  CloseHandle(process.hThread);
  CloseHandle(process.hProcess);

  char text[4096] = {};
  FILE* file = nullptr;
  if (_wfopen_s(&file, log.c_str(), L"rb") != 0 || !file) {
    wprintf(L"FAIL: no nwn5e.log - the DLL never ran (whoami exit %lu)\n", code);
    return 1;
  }
  fread(text, 1, sizeof(text) - 1, file);
  fclose(file);
  printf("--- nwn5e.log ---\n%s-----------------\n", text);
  const bool loaded = strstr(text, "nwn5e 0.9.43 loaded") != nullptr;
  const bool refused = strstr(text, "host is not the mapped nwmain.exe 1.69 build") != nullptr;
  const bool hooked = strstr(text, "hook ") != nullptr || strstr(text, "patch ") != nullptr;
  printf("whoami exit code: %lu\n", code);
  if (loaded && refused && !hooked && code == 0) {
    printf("PASS: DLL loaded before the entry point, refused a foreign host, host ran normally\n");
    return 0;
  }
  printf("FAIL: loaded=%d refused=%d hooked=%d\n", loaded, refused, hooked);
  return 1;
}
