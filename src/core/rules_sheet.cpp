// Character sheet: show the numbers the 2024 rules actually use.
//
// - Attack bonus (main-hand, off-hand, ranged): the sheet's values come from
//   GetMeleeAttackBonus / GetRangedAttackBonus, not from the attack roll, so
//   they get the same conversion as attack rolls (3e BAB swapped for the
//   proficiency bonus).
// - Number of attacks: UpdateCombatInformation computes its own BAB / 5
//   count; for player characters and henchmen it is replaced by the Extra
//   Attack count (GetAttacksPerRound, hooked by rules_extra_attack.cpp).
// The "base attack bonus" line stays 3e (see nwmain_1_69.h).

#include <stdint.h>

#include <algorithm>

#include "config.h"
#include "creature.h"
#include "hooks.h"
#include "log.h"
#include "log_once.h"
#include "nwmain_1_69.h"

namespace {

using MeleeAttackBonusFn = int(__fastcall*)(void* stats, void* edx, int offhand, int a, int b);
using RangedAttackBonusFn = int(__fastcall*)(void* stats, void* edx, int a, int b);
using UpdateCombatInformationFn = void(__fastcall*)(void* stats, void* edx);
using AttacksPerRoundFn = int(__fastcall*)(void* stats, void* edx);

MeleeAttackBonusFn g_get_melee_attack_bonus = nullptr;
RangedAttackBonusFn g_get_ranged_attack_bonus = nullptr;
UpdateCombatInformationFn g_update_combat_information = nullptr;
// Called through its address so the Extra Attack hook (if installed) applies.
const auto g_get_attacks_per_round = reinterpret_cast<AttacksPerRoundFn>(nwmain::kGetAttacksPerRound);

LogOnce g_sheet_logged(500);

int ConvertSheetAttackBonus(void* stats, int engine_value, const char* which) {
  if (!g_config.attack_proficiency) return engine_value;
  const CreatureFacts facts = ReadCreatureFacts(stats);
  const int converted = ConvertEngineAttackBonus(stats, engine_value, facts, nullptr, nullptr);
  if (g_config.log_attacks && facts.character &&
      g_sheet_logged.First(LogOnce::Key(ObjectIdOf(stats), engine_value, converted))) {
    Log("sheet: object 0x%08X %s %s attack bonus 3e %+d -> 5e %+d", ObjectIdOf(stats), KindOf(facts), which,
        engine_value, converted);
  }
  return converted;
}

int __fastcall GetMeleeAttackBonus(void* stats, void* edx, int offhand, int a, int b) {
  const int engine_value = g_get_melee_attack_bonus(stats, edx, offhand, a, b);
  return ConvertSheetAttackBonus(stats, engine_value, offhand ? "off-hand" : "melee");
}

int __fastcall GetRangedAttackBonus(void* stats, void* edx, int a, int b) {
  const int engine_value = g_get_ranged_attack_bonus(stats, edx, a, b);
  return ConvertSheetAttackBonus(stats, engine_value, "ranged");
}

void __fastcall UpdateCombatInformation(void* stats, void* edx) {
  g_update_combat_information(stats, edx);
  if (!g_config.extra_attack && g_config.debug_attacks_per_round <= 0) return;
  const CreatureFacts facts = ReadCreatureFacts(stats);
  if (!facts.character) return;
  uint8_t* block = Field<uint8_t*>(stats, nwmain::kStatsCombatInformation);
  if (!block) return;
  const int engine_attacks = block[nwmain::kCombatInformationAttacks];
  const int attacks = std::clamp(g_get_attacks_per_round(stats, nullptr), 1, 6);
  block[nwmain::kCombatInformationAttacks] = static_cast<uint8_t>(attacks);
  if (g_config.log_attacks && engine_attacks != attacks &&
      g_sheet_logged.First(LogOnce::Key(ObjectIdOf(stats) ^ 0x80000000u, engine_attacks, attacks))) {
    Log("sheet: object 0x%08X %s attacks per round 3e %d -> 5e %d", ObjectIdOf(stats), KindOf(facts),
        engine_attacks, attacks);
  }
}

}  // namespace

void InstallSheetRules() {
  if (g_config.attack_proficiency) {
    InstallHook({"CNWSCreatureStats::GetMeleeAttackBonus (sheet)", nwmain::kGetMeleeAttackBonus,
                 nwmain::kGetMeleeAttackBonusPrologue, sizeof(nwmain::kGetMeleeAttackBonusPrologue),
                 reinterpret_cast<void*>(&GetMeleeAttackBonus), reinterpret_cast<void**>(&g_get_melee_attack_bonus)});
    InstallHook({"CNWSCreatureStats::GetRangedAttackBonus (sheet)", nwmain::kGetRangedAttackBonus,
                 nwmain::kGetRangedAttackBonusPrologue, sizeof(nwmain::kGetRangedAttackBonusPrologue),
                 reinterpret_cast<void*>(&GetRangedAttackBonus),
                 reinterpret_cast<void**>(&g_get_ranged_attack_bonus)});
  }
  if (g_config.extra_attack || g_config.debug_attacks_per_round > 0) {
    InstallHook({"CNWSCreatureStats::UpdateCombatInformation (sheet)", nwmain::kUpdateCombatInformation,
                 nwmain::kUpdateCombatInformationPrologue, sizeof(nwmain::kUpdateCombatInformationPrologue),
                 reinterpret_cast<void*>(&UpdateCombatInformation),
                 reinterpret_cast<void**>(&g_update_combat_information)});
  }
}
