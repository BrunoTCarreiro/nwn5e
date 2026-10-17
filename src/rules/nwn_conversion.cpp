#include "rules/nwn_conversion.h"

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

int ConvertCriticalThreatFloor(int engine_threat_floor) { return engine_threat_floor > 20 ? engine_threat_floor : 20; }

int ConvertCriticalMultiplier(int engine_multiplier) { return engine_multiplier == 0 ? 0 : 2; }

bool IsPlausibleChallengeRating(float challenge_rating) {
  // Written so NaN fails both comparisons.
  return challenge_rating >= 0.0f && challenge_rating <= 60.0f;
}

}  // namespace rules
