#pragma once

#include <windows.h>

#include <string>

// Starts `exe` suspended and queues LoadLibraryW(dll) on its main thread, so
// the DLL loads during process initialisation — after the static imports,
// before the executable's entry point — then resumes it. Returns false (and
// terminates the child) if the DLL could not be queued.
inline bool StartWithDll(const std::wstring& exe, std::wstring command, const std::wstring& directory,
                         const std::wstring& dll, PROCESS_INFORMATION* out) {
  STARTUPINFOW startup = {sizeof(startup)};
  PROCESS_INFORMATION process = {};
  if (!CreateProcessW(exe.c_str(), &command[0], nullptr, nullptr, FALSE, CREATE_SUSPENDED, nullptr,
                      directory.empty() ? nullptr : directory.c_str(), &startup, &process)) {
    return false;
  }
  const SIZE_T bytes = (dll.size() + 1) * sizeof(wchar_t);
  void* remote = VirtualAllocEx(process.hProcess, nullptr, bytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
  auto load_library = reinterpret_cast<PAPCFUNC>(GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "LoadLibraryW"));
  if (!remote || !WriteProcessMemory(process.hProcess, remote, dll.c_str(), bytes, nullptr) ||
      !QueueUserAPC(load_library, process.hThread, reinterpret_cast<ULONG_PTR>(remote))) {
    TerminateProcess(process.hProcess, 1);
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return false;
  }
  ResumeThread(process.hThread);
  *out = process;
  return true;
}
