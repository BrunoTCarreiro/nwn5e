// 2024 critical hits (SRD 5.2): a critical hit happens on a natural 20 only,
// with no confirmation roll, and the attack's damage dice are rolled twice.
//
// Phase 0 covers the threat range and the multiplier. NWN multiplies all
// weapon damage, not only the dice; doubling dice alone needs the damage-roll
// hooks planned for Phase 1. The confirmation roll is removed in
// rules_crit_confirm.cpp.
//
// The engine answers threat 21 / multiplier x0 for a hand with nothing to
// attack with (the empty off-hand, when the character sheet's combat
// information is computed); rules::ConvertCritical* leave those unchanged.

#include "config.h"
#include "hooks.h"
#include "log.h"
#include "log_once.h"
#include "nwmain_1_69.h"
#include "rules/nwn_conversion.h"

namespace {

using GetCritFn = int(__fastcall*)(void* stats, void* edx, int offhand);
GetCritFn g_get_critical_hit_roll = nullptr;
GetCritFn g_get_critical_hit_multiplier = nullptr;

// Crit values depend on the weapon, not the creature: log each distinct one once.
LogOnce g_threat_logged(64);
LogOnce g_multiplier_logged(64);

int __fastcall GetCriticalHitRoll(void* stats, void* edx, int offhand) {
  const int vanilla = g_get_critical_hit_roll(stats, edx, offhand);
  if (g_config.debug_every_hit_crits) {
    // Every d20 roll threatens; with the confirmation removed every hit crits.
    if (g_config.log_attacks && g_threat_logged.First(LogOnce::Key(0, vanilla, 1))) {
      Log("debug every_hit_crits: threat %d-20 -> 1-20", vanilla);
    }
    return 1;
  }
  if (!g_config.crit_nat20_only) return vanilla;
  const int converted = rules::ConvertCriticalThreatFloor(vanilla);
  if (g_config.log_attacks && converted != vanilla && g_threat_logged.First(LogOnce::Key(0, vanilla, converted))) {
    Log("crit threat: 3e %d-20 -> 5e %d-20 (logged once per value)", vanilla, converted);
  }
  return converted;
}

int __fastcall GetCriticalHitMultiplier(void* stats, void* edx, int offhand) {
  const int vanilla = g_get_critical_hit_multiplier(stats, edx, offhand);
  if (!g_config.crit_double_damage) return vanilla;
  const int converted = rules::ConvertCriticalMultiplier(vanilla);
  if (g_config.log_attacks && converted != vanilla &&
      g_multiplier_logged.First(LogOnce::Key(0, vanilla, converted))) {
    Log("crit multiplier: 3e x%d -> 5e x%d (logged once per value)", vanilla, converted);
  }
  return converted;
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
