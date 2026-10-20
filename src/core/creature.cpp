#include "creature.h"

#include <algorithm>

#include "log.h"
#include "log_once.h"
#include "nwmain_1_69.h"

namespace {

using BaseAttackBonusFn = int(__fastcall*)(void* stats, void* edx, int pre_epic_only);
using LevelFn = int(__fastcall*)(void* stats, void* edx, int count_negative_levels);

const auto g_get_base_attack_bonus = reinterpret_cast<BaseAttackBonusFn>(nwmain::kGetBaseAttackBonus);
const auto g_get_level = reinterpret_cast<LevelFn>(nwmain::kGetLevel);

LogOnce g_implausible_logged(200);

}  // namespace

uint32_t ObjectIdOf(void* stats) {
  const void* creature = Field<void*>(stats, nwmain::kStatsCreature);
  return creature ? Field<uint32_t>(creature, nwmain::kCreatureObjectId) : 0;
}

CreatureFacts ReadCreatureFacts(void* stats) {
  CreatureFacts facts;
  facts.is_player = Field<uint32_t>(stats, nwmain::kStatsIsPlayer) != 0;
  const void* creature = Field<void*>(stats, nwmain::kStatsCreature);
  facts.associate_type = creature ? Field<uint16_t>(creature, nwmain::kCreatureAssociateType) : 0;
  facts.character = rules::UsesCharacterRules(facts.is_player, facts.associate_type);
  facts.level = g_get_level(stats, nullptr, 0) & 0xFF;
  facts.challenge_rating = Field<float>(stats, nwmain::kStatsChallengeRating);
  facts.plausible = facts.character || rules::IsPlausibleChallengeRating(facts.challenge_rating);
  if (!facts.plausible && g_implausible_logged.First(LogOnce::Key(ObjectIdOf(stats), 0, 0))) {
    const uint32_t raw = Field<uint32_t>(stats, nwmain::kStatsChallengeRating);
    Log("implausible CR read for object 0x%08X (raw 0x%08X); engine numbers kept", ObjectIdOf(stats), raw);
  }
  return facts;
}

const char* KindOf(const CreatureFacts& facts) {
  if (facts.is_player) return "PC";
  return facts.character ? "henchman" : "creature";
}

std::vector<rules::NwnClassSlot> ReadClassSlots(void* stats) {
  std::vector<rules::NwnClassSlot> slots;
  const int count = std::min<int>(Field<uint8_t>(stats, nwmain::kStatsClassCount), 3);
  for (int i = 0; i < count; ++i) {
    const uintptr_t slot = i * nwmain::kStatsClassSlotStride;
    slots.push_back({Field<uint8_t>(stats, slot + nwmain::kStatsClassSlotId),
                     Field<uint8_t>(stats, slot + nwmain::kStatsClassSlotLevels)});
  }
  return slots;
}

int ClassLevel2024(void* stats, rules::CharacterClass character_class) {
  int levels = 0;
  for (const rules::ClassLevels& c : rules::GroupClassLevels(ReadClassSlots(stats))) {
    if (c.character_class == character_class) levels += c.levels;
  }
  return levels;
}

int ConvertEngineAttackBonus(void* stats, int engine_value, const CreatureFacts& facts, int* base_attack_bonus,
                             int* proficiency_bonus) {
  if (!facts.plausible) return engine_value;
  const int bab = g_get_base_attack_bonus(stats, nullptr, 0);
  const int pb = rules::ProficiencyBonusForCreature(facts.character, facts.level, facts.challenge_rating);
  if (base_attack_bonus) *base_attack_bonus = bab;
  if (proficiency_bonus) *proficiency_bonus = pb;
  return rules::ConvertAttackModifier(engine_value, bab, pb);
}
