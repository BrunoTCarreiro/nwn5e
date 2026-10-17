#include "hooks.h"

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
