// 2024 critical hits (SRD 5.2): a critical hit happens on a natural 20 only,
// with no confirmation roll, and the attack's damage dice are rolled twice.
//
// Phase 0 covers the threat range and the multiplier. NWN multiplies all
// weapon damage, not only the dice; doubling dice alone needs the damage-roll
// hooks planned for Phase 1. The confirmation roll lives inside the attack
// resolution and is handled in rules_crit_confirm.cpp once mapped.

#include "config.h"
#include "hooks.h"
#include "log.h"
#include "nwmain_1_69.h"

namespace {

using GetCritFn = int(__fastcall*)(void* stats, void* edx, int offhand);
GetCritFn g_get_critical_hit_roll = nullptr;
GetCritFn g_get_critical_hit_multiplier = nullptr;

int __fastcall GetCriticalHitRoll(void* stats, void* edx, int offhand) {
  const int vanilla = g_get_critical_hit_roll(stats, edx, offhand);
  if (!g_config.crit_nat20_only) return vanilla;
  if (g_config.log_attacks && vanilla != 20) Log("crit threat: 3e %d-20 -> 5e 20", vanilla);
  return 20;
}

int __fastcall GetCriticalHitMultiplier(void* stats, void* edx, int offhand) {
  const int vanilla = g_get_critical_hit_multiplier(stats, edx, offhand);
  if (!g_config.crit_double_damage) return vanilla;
  if (g_config.log_attacks && vanilla != 2) Log("crit multiplier: 3e x%d -> 5e x2", vanilla);
  return 2;
}

}  // namespace

void InstallCritRules() {
  InstallHook({"CNWSCreatureStats::GetCriticalHitRoll", nwmain::kGetCriticalHitRoll, nwmain::kGetCriticalHitRollPrologue,
               sizeof(nwmain::kGetCriticalHitRollPrologue), reinterpret_cast<void*>(&GetCriticalHitRoll),
               reinterpret_cast<void**>(&g_get_critical_hit_roll)});
  InstallHook({"CNWSCreatureStats::GetCriticalHitMultiplier", nwmain::kGetCriticalHitMultiplier,
               nwmain::kGetCriticalHitMultiplierPrologue, sizeof(nwmain::kGetCriticalHitMultiplierPrologue),
               reinterpret_cast<void*>(&GetCriticalHitMultiplier),
               reinterpret_cast<void**>(&g_get_critical_hit_multiplier)});
}
