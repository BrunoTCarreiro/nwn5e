#include "rules/nwn_conversion.h"

#include <iterator>

#include "rules/core.h"
#include "rules/monster.h"

namespace rules {

int ProficiencyBonusForCreature(bool is_player_character, int total_level, double challenge_rating) {
  return is_player_character ? ProficiencyBonusForLevel(total_level)
                             : ProficiencyBonusForChallengeRating(challenge_rating);
}

int ConvertAttackModifier(int engine_attack_modifier, int base_attack_bonus, int proficiency_bonus) {
  return engine_attack_modifier - base_attack_bonus + proficiency_bonus;
}

int ConvertArmorClass(bool is_player_character, int engine_armor_class, double challenge_rating) {
  return is_player_character ? engine_armor_class : ExpectedMonsterArmorClass(challenge_rating);
}

std::optional<CharacterClass> ClassFromNwnId(int nwn_class_id) {
  static constexpr CharacterClass kBaseClasses[] = {
      CharacterClass::kBarbarian, CharacterClass::kBard,    CharacterClass::kCleric,  CharacterClass::kDruid,
      CharacterClass::kFighter,   CharacterClass::kMonk,    CharacterClass::kPaladin, CharacterClass::kRanger,
      CharacterClass::kRogue,     CharacterClass::kSorcerer, CharacterClass::kWizard,
  };
  if (nwn_class_id < 0 || nwn_class_id >= static_cast<int>(std::size(kBaseClasses))) return std::nullopt;
  return kBaseClasses[nwn_class_id];
}

int MainHandAttacksPerRound(bool is_player_character, const std::vector<NwnClassSlot>& classes, int engine_attacks) {
  if (!is_player_character) return engine_attacks;
  std::vector<ClassLevels> base_classes;
  for (const NwnClassSlot& slot : classes) {
    if (const auto character_class = ClassFromNwnId(slot.nwn_class_id)) {
      base_classes.push_back({*character_class, slot.levels});
    }
  }
  return AttacksPerAttackAction(base_classes);
}

int ConvertCriticalThreatFloor(int engine_threat_floor) { return engine_threat_floor > 20 ? engine_threat_floor : 20; }

int ConvertCriticalMultiplier(int engine_multiplier) { return engine_multiplier == 0 ? 0 : 2; }

bool IsPlausibleChallengeRating(float challenge_rating) {
  // Written so NaN fails both comparisons.
  return challenge_rating >= 0.0f && challenge_rating <= 60.0f;
}

}  // namespace rules
