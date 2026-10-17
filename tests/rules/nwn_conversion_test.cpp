#include <limits>

#include "check.h"
#include "rules/nwn_conversion.h"

using namespace rules;

// Breaks caught: player characters using their CR (NWN gives PCs a CR too), or
// creatures using their level.
TEST(PlayerCharactersUseLevelCreaturesUseChallengeRating) {
  CHECK_EQ(ProficiencyBonusForCreature(true, 9, 1), 4);
  CHECK_EQ(ProficiencyBonusForCreature(false, 9, 1), 2);
  CHECK_EQ(ProficiencyBonusForCreature(false, 1, 21), 7);
}

// Breaks caught: NWN epic player levels (21-40) raising the bonus past +6.
TEST(EpicPlayerLevelsStayAtPlusSix) {
  CHECK_EQ(ProficiencyBonusForCreature(true, 30, 30), 6);
}

// Breaks caught: adding the BAB instead of removing it, or dropping the rest
// of the engine's modifier.
TEST(AttackModifierSwapsBaseAttackBonusForProficiency) {
  // Level 10 fighter: BAB +10, Str +3, +1 weapon, Weapon Focus +1 -> engine +15.
  CHECK_EQ(ConvertAttackModifier(15, 10, 4), 9);
  // Level 1 wizard: BAB +0, Dex +2 -> engine +2.
  CHECK_EQ(ConvertAttackModifier(2, 0, 2), 4);
}

// Breaks caught: garbage from a wrong memory offset (NaN, huge, negative)
// accepted as a challenge rating; real CRs (0, fractions, 30+) rejected.
TEST(PlausibleChallengeRatingsOnly) {
  CHECK_EQ(IsPlausibleChallengeRating(0.0f), true);
  CHECK_EQ(IsPlausibleChallengeRating(0.25f), true);
  CHECK_EQ(IsPlausibleChallengeRating(13.5f), true);
  CHECK_EQ(IsPlausibleChallengeRating(40.0f), true);
  CHECK_EQ(IsPlausibleChallengeRating(-0.5f), false);
  CHECK_EQ(IsPlausibleChallengeRating(61.0f), false);
  CHECK_EQ(IsPlausibleChallengeRating(3.0e9f), false);
  CHECK_EQ(IsPlausibleChallengeRating(std::numeric_limits<float>::quiet_NaN()), false);
  CHECK_EQ(IsPlausibleChallengeRating(std::numeric_limits<float>::infinity()), false);
}

// Breaks caught: player AC replaced by the CR table, or creatures keeping 3e AC.
TEST(OnlyNonPlayerCreaturesGetTheChallengeRatingArmorClass) {
  CHECK_EQ(ConvertArmorClass(true, 32, 20), 32);
  CHECK_EQ(ConvertArmorClass(false, 32, 20), 19);
  CHECK_EQ(ConvertArmorClass(false, 12, 1), 13);
}
