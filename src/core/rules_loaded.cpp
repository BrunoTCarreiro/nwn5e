// The rules object's "loaded" moment (rules_loaded.h): the last loader call
// of the CNWRules constructor (0x47A6F8) and of ReloadAll (0x47AD4C) — both
// call 0x47E280 with ecx = the rules object and no stack arguments — go
// through here, and the registered handlers run after it.

#include "rules_loaded.h"

#include <stdint.h>
#include <string.h>

#include <vector>

#include "hooks.h"
#include "log.h"
#include "nwmain_1_69.h"

namespace {

using RulesLoaderFn = void(__fastcall*)(void* rules_object, void* edx);

const auto g_rules_last_loader = reinterpret_cast<RulesLoaderFn>(nwmain::kRulesLastLoader);
std::vector<RulesLoadedHandler> g_handlers;
int g_installed = 0;  // 0 not yet, 1 installed, -1 failed

void Run(void* rules_object, void* edx, bool reload) {
  g_rules_last_loader(rules_object, edx);
  for (RulesLoadedHandler handler : g_handlers) handler(rules_object, reload);
}

void __fastcall AfterConstructor(void* rules_object, void* edx) { Run(rules_object, edx, false); }
void __fastcall AfterReload(void* rules_object, void* edx) { Run(rules_object, edx, true); }

bool RedirectCall(const char* name, uintptr_t site, uintptr_t expected_target, const void* new_target) {
  const auto* code = reinterpret_cast<const uint8_t*>(site);
  int32_t rel = 0;
  memcpy(&rel, code + 1, sizeof(rel));
  if (code[0] != 0xe8 || site + nwmain::kCallLength + rel != expected_target) {
    Log("SKIP %s: 0x%08X is not the mapped call", name, site);
    return false;
  }
  uint8_t expected[5];
  memcpy(expected, code, sizeof(expected));
  uint8_t call[5] = {0xe8};
  const int32_t new_rel = static_cast<int32_t>(reinterpret_cast<uintptr_t>(new_target) - (site + nwmain::kCallLength));
  memcpy(call + 1, &new_rel, sizeof(new_rel));
  return PatchBytes(name, site, expected, call, sizeof(call));
}

}  // namespace

bool OnRulesLoaded(RulesLoadedHandler handler) {
  if (g_installed == 0) {
    const bool loaded = RedirectCall("rules loaded: after the rules constructor's loaders", nwmain::kRulesLoadedCall,
                                     nwmain::kRulesLastLoader, reinterpret_cast<void*>(&AfterConstructor));
    const bool reloaded = RedirectCall("rules loaded: after ReloadAll's loaders", nwmain::kRulesReloadedCall,
                                       nwmain::kRulesLastLoader, reinterpret_cast<void*>(&AfterReload));
    g_installed = loaded && reloaded ? 1 : -1;
  }
  if (g_installed < 0) return false;
  g_handlers.push_back(handler);
  return true;
}
