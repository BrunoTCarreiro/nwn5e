#include "hooks.h"

#include <windows.h>
#include <string.h>

#include "MinHook.h"
#include "log.h"

bool HooksBegin() {
  const MH_STATUS status = MH_Initialize();
  if (status != MH_OK) Log("MinHook initialise failed: %s", MH_StatusToString(status));
  return status == MH_OK;
}

bool InstallHook(const HookSpec& spec) {
  const void* target = reinterpret_cast<const void*>(spec.address);
  if (memcmp(target, spec.expected, spec.expected_size) != 0) {
    Log("SKIP %s at 0x%08X: prologue does not match the mapped build", spec.name, spec.address);
    return false;
  }
  MH_STATUS status = MH_CreateHook(const_cast<void*>(target), spec.detour, spec.original);
  if (status == MH_OK) status = MH_QueueEnableHook(const_cast<void*>(target));
  if (status != MH_OK) {
    Log("FAIL %s at 0x%08X: %s", spec.name, spec.address, MH_StatusToString(status));
    return false;
  }
  Log("hook %s at 0x%08X", spec.name, spec.address);
  return true;
}

bool HooksCommit() {
  const MH_STATUS status = MH_ApplyQueued();
  if (status != MH_OK) Log("MinHook apply failed: %s", MH_StatusToString(status));
  return status == MH_OK;
}

bool PatchBytes(const char* name, uintptr_t address, const uint8_t* expected, const uint8_t* replacement, size_t size) {
  void* target = reinterpret_cast<void*>(address);
  if (memcmp(target, expected, size) != 0) {
    Log("SKIP patch %s at 0x%08X: bytes do not match the mapped build", name, address);
    return false;
  }
  DWORD old_protect = 0;
  if (!VirtualProtect(target, size, PAGE_EXECUTE_READWRITE, &old_protect)) {
    Log("FAIL patch %s at 0x%08X: VirtualProtect error %lu", name, address, GetLastError());
    return false;
  }
  memcpy(target, replacement, size);
  VirtualProtect(target, size, old_protect, &old_protect);
  FlushInstructionCache(GetCurrentProcess(), target, size);
  Log("patch %s at 0x%08X (%u bytes)", name, address, static_cast<unsigned>(size));
  return true;
}
