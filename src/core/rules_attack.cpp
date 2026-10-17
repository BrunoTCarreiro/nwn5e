// 2024 attack numbers on top of the engine's 3e ones:
//
// - Attack rolls: the engine's attack modifier minus the 3e base attack bonus
//   plus the proficiency bonus (by level for player characters, by challenge
//   rating for every other creature). The ability modifier, weapon
//   enhancement, effects and feats the engine adds are kept. The 3e iterative
//   penalty (-5 per extra attack) is still applied until Extra Attack replaces
//   the iterative attacks.
// - Armor Class: creatures that are not player characters use the expected AC
//   for their challenge rating. Henchmen and summons are not player
//   characters to the engine, so they are converted too (for now).
//
// If the challenge rating read from memory is not plausible, the engine's
// numbers are returned unchanged and the read is logged.

#include <stdint.h>

#include "config.h"
#include "hooks.h"
#include "log.h"
#include "nwmain_1_69.h"
#include "rules/nwn_conversion.h"

namespace {

using AttackModifierFn = int(__fastcall*)(void* stats, void* edx, void* target);
using ArmorClassFn = int(__fastcall*)(void* stats, void* edx, void* attacker, int touch);
using BaseAttackBonusFn = int(__fastcall*)(void* stats, void* edx, int pre_epic_only);
using LevelFn = int(__fastcall*)(void* stats, void* edx, int count_negative_levels);

AttackModifierFn g_get_attack_modifier_versus = nullptr;
ArmorClassFn g_get_armor_class_versus = nullptr;
const auto g_get_base_attack_bonus = reinterpret_cast<BaseAttackBonusFn>(nwmain::kGetBaseAttackBonus);
const auto g_get_level = reinterpret_cast<LevelFn>(nwmain::kGetLevel);

// Each kind of decision is logged a limited number of times, to show the
// conversion working without filling the log during long fights.
constexpr int kLogLimit = 40;
int g_attack_logs = 0;
int g_armor_logs = 0;
int g_implausible_logs = 0;

template <typename T>
T Field(const void* object, uintptr_t offset) {
  return *reinterpret_cast<const T*>(static_cast<const uint8_t*>(object) + offset);
}

uint32_t ObjectIdOf(void* stats) {
  const void* creature = Field<void*>(stats, nwmain::kStatsCreature);
  return creature ? Field<uint32_t>(creature, nwmain::kCreatureObjectId) : 0;
}

struct CreatureFacts {
  bool is_player;
  int level;
  float challenge_rating;
  bool plausible;
};

CreatureFacts ReadFacts(void* stats) {
  CreatureFacts facts;
  facts.is_player = Field<uint32_t>(stats, nwmain::kStatsIsPlayer) != 0;
  facts.level = g_get_level(stats, nullptr, 0) & 0xFF;
  facts.challenge_rating = Field<float>(stats, nwmain::kStatsChallengeRating);
  facts.plausible = facts.is_player || rules::IsPlausibleChallengeRating(facts.challenge_rating);
  if (!facts.plausible && g_implausible_logs < kLogLimit) {
    ++g_implausible_logs;
    const uint32_t raw = Field<uint32_t>(stats, nwmain::kStatsChallengeRating);
    Log("implausible CR read for object 0x%08X (raw 0x%08X); engine numbers kept", ObjectIdOf(stats), raw);
  }
  return facts;
}

int __fastcall GetAttackModifierVersus(void* stats, void* edx, void* target) {
  const int engine_modifier = g_get_attack_modifier_versus(stats, edx, target);
  if (!g_config.attack_proficiency) return engine_modifier;
  const CreatureFacts facts = ReadFacts(stats);
  if (!facts.plausible) return engine_modifier;
  const int base_attack_bonus = g_get_base_attack_bonus(stats, nullptr, 0);
  const int proficiency = rules::ProficiencyBonusForCreature(facts.is_player, facts.level, facts.challenge_rating);
  const int converted = rules::ConvertAttackModifier(engine_modifier, base_attack_bonus, proficiency);
  if (g_config.log_attacks && g_attack_logs < kLogLimit) {
    ++g_attack_logs;
    Log("attack: object 0x%08X %s level %d CR %.2f: 3e %+d (BAB %d) -> 5e %+d (PB %d)", ObjectIdOf(stats),
        facts.is_player ? "PC" : "creature", facts.level, facts.challenge_rating, engine_modifier,
        base_attack_bonus, converted, proficiency);
  }
  return converted;
}

int __fastcall GetArmorClassVersus(void* stats, void* edx, void* attacker, int touch) {
  const int engine_armor_class = g_get_armor_class_versus(stats, edx, attacker, touch);
  if (!g_config.creature_ac_by_cr) return engine_armor_class;
  const CreatureFacts facts = ReadFacts(stats);
  if (facts.is_player || !facts.plausible) return engine_armor_class;
  // ResolveAttackRoll reads the result as a 16-bit value.
  const int engine_value = static_cast<int16_t>(engine_armor_class & 0xFFFF);
  const int converted = rules::ConvertArmorClass(false, engine_value, facts.challenge_rating);
  if (g_config.log_attacks && g_armor_logs < kLogLimit) {
    ++g_armor_logs;
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
