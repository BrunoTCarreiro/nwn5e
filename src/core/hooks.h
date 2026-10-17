#pragma once

#include <stddef.h>
#include <stdint.h>

// One engine function we replace. `expected` is the function's first bytes in
// the mapped nwmain.exe build (see re/maps/); a mismatch means a different
// build, and the hook is skipped rather than patched into the wrong code.
struct HookSpec {
  const char* name;
  uintptr_t address;
  const uint8_t* expected;
  size_t expected_size;
  void* detour;
  void** original;
};

bool HooksBegin();
bool InstallHook(const HookSpec& spec);
bool HooksCommit();
