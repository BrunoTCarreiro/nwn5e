// 2024 attack numbers on top of the engine's 3e ones:
//
// - Attack rolls: the engine's attack modifier minus the 3e base attack bonus
//   plus the proficiency bonus (by level for player characters and henchmen,
//   by challenge rating for every other creature). The ability modifier,
//   weapon enhancement, effects and feats the engine adds are kept. The 3e
//   iterative penalty is removed by rules_extra_attack.cpp.
// - Armor Class: player characters and henchmen keep the engine's (gear) AC;
//   every other creature — monsters, summons, familiars, animal companions —
//   uses the expected AC for its challenge rating (decision 11).
// - Situational modifiers (prone, blind, flanked…) are taken out of the
//   attack modifier when rules_advantage.cpp turns them into advantage or
//   disadvantage for the attack being resolved.
//
// If the challenge rating read from memory is not plausible, the engine's
// numbers are returned unchanged and the read is logged.

#include <stdint.h>

#include "config.h"
#include "creature.h"
#include "hooks.h"
#include "log.h"
#include "log_once.h"
#include "nwmain_1_69.h"
#include "rules/nwn_conversion.h"

int PendingSituationalTotal(const void* stats, const void* target);  // rules_advantage.cpp
int RacialAttackBonusToRemove(void* stats, void* target);            // rules_racial.cpp
int RacialArmorBonusToRemove(void* defender_stats, void* attacker);  // rules_racial.cpp

int FightingStyleAttackBonus(void* stats);  // rules_fighter.cpp (Fighter F1)
int DefenseArmorClassBonus(void* creature);  // rules_fighter.cpp (Fighter F1)

namespace {

using AttackModifierFn = int(__fastcall*)(void* stats, void* edx, void* target);
using ArmorClassFn = int(__fastcall*)(void* stats, void* edx, void* attacker, int touch);

AttackModifierFn g_get_attack_modifier_versus = nullptr;
ArmorClassFn g_get_armor_class_versus = nullptr;

// Each creature's conversion is logged once, and again when its numbers
// change (another weapon, a buff, a level), so every combatant shows up.
LogOnce g_attack_logged(2000);
LogOnce g_armor_logged(2000);

int __fastcall GetAttackModifierVersus(void* stats, void* edx, void* target) {
  const int engine_modifier = g_get_attack_modifier_versus(stats, edx, target) -
                              PendingSituationalTotal(stats, target) - RacialAttackBonusToRemove(stats, target);
  if (!g_config.attack_proficiency) return engine_modifier;
  const CreatureFacts facts = ReadCreatureFacts(stats);
  if (!facts.plausible) return engine_modifier;
  int base_attack_bonus = 0;
  int proficiency = 0;
  const int converted = ConvertEngineAttackBonus(stats, engine_modifier, facts, &base_attack_bonus, &proficiency) +
                        FightingStyleAttackBonus(stats);  // Fighter F1 Archery
  if (g_config.log_attacks && g_attack_logged.First(LogOnce::Key(ObjectIdOf(stats), engine_modifier, converted))) {
    Log("attack: object 0x%08X %s level %d CR %.2f: 3e %+d (BAB %d) -> 5e %+d (PB %d)", ObjectIdOf(stats),
        KindOf(facts), facts.level, facts.challenge_rating, engine_modifier, base_attack_bonus, converted,
        proficiency);
  }
  return converted;
}

int __fastcall GetArmorClassVersus(void* stats, void* edx, void* attacker, int touch) {
  const int engine_armor_class =
      g_get_armor_class_versus(stats, edx, attacker, touch) - RacialArmorBonusToRemove(stats, attacker);
  if (!g_config.creature_ac_by_cr && g_config.debug_creature_ac <= 0) return engine_armor_class;
  const CreatureFacts facts = ReadCreatureFacts(stats);
  if (facts.character) {  // players and henchmen keep their gear AC (+ Fighter F1 Defense)
    return engine_armor_class + DefenseArmorClassBonus(Field<void*>(stats, nwmain::kStatsCreature));
  }
  // ResolveAttackRoll reads the result as a 16-bit value.
  const int engine_value = static_cast<int16_t>(engine_armor_class & 0xFFFF);
  if (g_config.debug_creature_ac > 0) {
    // Test switch: attacks against creatures miss (except natural 20s), so a
    // round's attacks are all rolled instead of ending with the first kill.
    if (g_config.log_attacks &&
        g_armor_logged.First(LogOnce::Key(ObjectIdOf(stats), engine_value, g_config.debug_creature_ac))) {
      Log("AC: object 0x%08X creature: 3e %d -> debug %d", ObjectIdOf(stats), engine_value,
          g_config.debug_creature_ac);
    }
    return g_config.debug_creature_ac;
  }
  if (!g_config.creature_ac_by_cr || !facts.plausible) return engine_armor_class;
  const int converted = rules::ConvertArmorClass(false, engine_value, facts.challenge_rating);
  if (g_config.log_attacks && g_armor_logged.First(LogOnce::Key(ObjectIdOf(stats), engine_value, converted))) {
    Log("AC: object 0x%08X creature CR %.2f%s: 3e %d -> 5e %d", ObjectIdOf(stats), facts.challenge_rating,
        touch ? " (touch)" : "", engine_value, converted);
  }
  return converted;
}

}  // namespace

void InstallAttackRules() {
  InstallHook({"CNWSCreatureStats::GetAttackModifierVersus", nwmain::kGetAttackModifierVersus,
               nwmain::kGetAttackModifierVersusPrologue, sizeof(nwmain::kGetAttackModifierVersusPrologue),
               reinterpret_cast<void*>(&GetAttackModifierVersus),
               reinterpret_cast<void**>(&g_get_attack_modifier_versus)});
  InstallHook({"CNWSCreatureStats::GetArmorClassVersus", nwmain::kGetArmorClassVersus,
               nwmain::kGetArmorClassVersusPrologue, sizeof(nwmain::kGetArmorClassVersusPrologue),
               reinterpret_cast<void*>(&GetArmorClassVersus),
               reinterpret_cast<void**>(&g_get_armor_class_versus)});
}
