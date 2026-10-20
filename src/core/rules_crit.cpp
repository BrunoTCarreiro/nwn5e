// 2024 critical hits (SRD 5.2): a critical hit happens on a natural 20 only,
// with no confirmation roll, and the attack's damage dice are rolled twice.
//
// NWN's GetDamageRoll asks for the multiplier and repeats each part of the
// damage that many times: weapon dice are re-rolled (2024: twice) and the flat
// bonuses (Strength etc.) are added again (2024: once). With crit_dice_only the
// multiplier is 1 when asked from the flat-bonus loop (nwmain_1_69.h) and 2
// everywhere else. Effect damage bonuses (GetTotalEffectBonus) ask from one
// loop for dice and constants alike; there the hook reads the current entry's
// dice count from the caller's stack and answers 1 for a constant.
// The confirmation roll is removed in rules_crit_confirm.cpp.
//
// The engine answers threat 21 / multiplier x0 for a hand with nothing to
// attack with (the empty off-hand, when the character sheet's combat
// information is computed); rules::ConvertCritical* leave those unchanged.

#include <intrin.h>
#include <string.h>

#include <algorithm>

#include "config.h"
#include "hooks.h"
#include "log.h"
#include "log_once.h"
#include "nwmain_1_69.h"
#include "rules/nwn_conversion.h"

int ChampionCriticalFloorFor(void* stats);  // rules_fighter.cpp (Fighter C1)

namespace {

using GetCritFn = int(__fastcall*)(void* stats, void* edx, int offhand);
GetCritFn g_get_critical_hit_roll = nullptr;
GetCritFn g_get_critical_hit_multiplier = nullptr;

// Crit values depend on the weapon, not the creature: log each distinct one once.
LogOnce g_threat_logged(64);
LogOnce g_multiplier_logged(64);

// Set at install when both flat-bonus call sites hold the mapped call bytes.
bool g_flat_bonus_sites_verified = false;

bool IsFlatBonusLoopCaller(uintptr_t return_address) {
  return g_flat_bonus_sites_verified &&
         (return_address == nwmain::kFlatBonusLoopCall1 + nwmain::kCallLength ||
          return_address == nwmain::kFlatBonusLoopCall2 + nwmain::kCallLength);
}

// Set at install when all GetTotalEffectBonus sites call GetCriticalHitMultiplier.
bool g_effect_bonus_sites_verified = false;

bool IsEffectBonusCaller(uintptr_t return_address) {
  if (!g_effect_bonus_sites_verified) return false;
  for (uintptr_t site : nwmain::kEffectBonusMultiplierCalls) {
    if (return_address == site + nwmain::kCallLength) return true;
  }
  return false;
}

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
  int converted = rules::ConvertCriticalThreatFloor(vanilla);
  if (converted <= 20) converted = std::min(converted, ChampionCriticalFloorFor(stats));  // Fighter C1
  if (g_config.log_attacks && converted != vanilla && g_threat_logged.First(LogOnce::Key(0, vanilla, converted))) {
    Log("crit threat: 3e %d-20 -> 5e %d-20 (logged once per value)", vanilla, converted);
  }
  return converted;
}

int __fastcall GetCriticalHitMultiplier(void* stats, void* edx, int offhand) {
  const int vanilla = g_get_critical_hit_multiplier(stats, edx, offhand);
  if (!g_config.crit_double_damage) return vanilla;
  // The detour is reached by a jmp from the hooked entry, so this is the
  // engine call site's return address.
  const uintptr_t caller = reinterpret_cast<uintptr_t>(_ReturnAddress());
  if (g_config.crit_dice_only && IsFlatBonusLoopCaller(caller)) {
    const int converted = rules::CriticalMultiplierFor(rules::CritDamagePart::kFlatBonus, vanilla);
    if (g_config.log_attacks && g_multiplier_logged.First(LogOnce::Key(1, vanilla, converted))) {
      Log("crit damage: flat bonuses (Strength etc.) 3e x%d -> 5e x%d (logged once per value)", vanilla, converted);
    }
    return converted;
  }
  if (g_config.crit_dice_only && IsEffectBonusCaller(caller)) {
    const auto* return_slot = static_cast<const uint8_t*>(_AddressOfReturnAddress());
    const int dice_count =
        *reinterpret_cast<const int32_t*>(return_slot + nwmain::kEffectBonusDiceCountFromReturnSlot);
    const rules::CritDamagePart part = rules::EffectDamagePart(dice_count);
    const int converted = rules::CriticalMultiplierFor(part, vanilla);
    if (g_config.log_attacks && part == rules::CritDamagePart::kFlatBonus &&
        g_multiplier_logged.First(LogOnce::Key(2, vanilla, converted))) {
      Log("crit damage: flat effect damage bonuses 3e x%d -> 5e x%d (logged once per value)", vanilla, converted);
    }
    return converted;
  }
  const int converted = rules::CriticalMultiplierFor(rules::CritDamagePart::kDice, vanilla);
  if (g_config.log_attacks && converted != vanilla &&
      g_multiplier_logged.First(LogOnce::Key(0, vanilla, converted))) {
    Log("crit multiplier: 3e x%d -> 5e x%d (logged once per value)", vanilla, converted);
  }
  return converted;
}

bool VerifyFlatBonusSites() {
  const bool first = memcmp(reinterpret_cast<const void*>(nwmain::kFlatBonusLoopCall1), nwmain::kFlatBonusLoopCall1Bytes,
                            sizeof(nwmain::kFlatBonusLoopCall1Bytes)) == 0;
  const bool second = memcmp(reinterpret_cast<const void*>(nwmain::kFlatBonusLoopCall2),
                             nwmain::kFlatBonusLoopCall2Bytes, sizeof(nwmain::kFlatBonusLoopCall2Bytes)) == 0;
  if (!first || !second) {
    Log("SKIP crit_dice_only: GetDamageRoll flat-bonus call sites do not match the mapped build");
    return false;
  }
  Log("crit_dice_only: GetDamageRoll flat-bonus call sites verified at 0x%08X / 0x%08X",
      nwmain::kFlatBonusLoopCall1, nwmain::kFlatBonusLoopCall2);
  return true;
}

// Each site must be E8 rel32 landing on GetCriticalHitMultiplier (checked
// before the hook patches that function's entry; call sites are not patched).
bool VerifyEffectBonusSites() {
  for (uintptr_t site : nwmain::kEffectBonusMultiplierCalls) {
    const auto* code = reinterpret_cast<const uint8_t*>(site);
    int32_t rel = 0;
    memcpy(&rel, code + 1, sizeof(rel));
    if (code[0] != 0xe8 || site + nwmain::kCallLength + rel != nwmain::kGetCriticalHitMultiplier) {
      Log("SKIP crit_dice_only for effect bonuses: GetTotalEffectBonus site 0x%08X is not the mapped call", site);
      return false;
    }
  }
  Log("crit_dice_only: GetTotalEffectBonus multiplier call sites verified (%u)",
      static_cast<unsigned>(sizeof(nwmain::kEffectBonusMultiplierCalls) / sizeof(nwmain::kEffectBonusMultiplierCalls[0])));
  return true;
}

}  // namespace

void InstallCritRules() {
  if (g_config.crit_dice_only) {
    g_flat_bonus_sites_verified = VerifyFlatBonusSites();
    g_effect_bonus_sites_verified = VerifyEffectBonusSites();
  }
  InstallHook({"CNWSCreatureStats::GetCriticalHitRoll", nwmain::kGetCriticalHitRoll, nwmain::kGetCriticalHitRollPrologue,
               sizeof(nwmain::kGetCriticalHitRollPrologue), reinterpret_cast<void*>(&GetCriticalHitRoll),
               reinterpret_cast<void**>(&g_get_critical_hit_roll)});
  InstallHook({"CNWSCreatureStats::GetCriticalHitMultiplier", nwmain::kGetCriticalHitMultiplier,
               nwmain::kGetCriticalHitMultiplierPrologue, sizeof(nwmain::kGetCriticalHitMultiplierPrologue),
               reinterpret_cast<void*>(&GetCriticalHitMultiplier),
               reinterpret_cast<void**>(&g_get_critical_hit_multiplier)});
}
