#include "check.h"
#include "rules/class_features.h"

#include <string>

using namespace rules;

// Breaks caught: features past the character's level listed, the subclass
// missing at 3, a class without a brief showing a list, the SRD subclass
// names drifting (they are SRD 5.2's one per class).
TEST(ClassFeaturesByLevel) {
  CHECK_EQ(ClassFeatures2024(CharacterClass::kFighter, 1).size(), 3u);
  bool champion = false, extra_attack = false;
  for (const ClassFeature& f : ClassFeatures2024(CharacterClass::kFighter, 5)) {
    champion = champion || std::string(f.name) == "Champion";
    extra_attack = extra_attack || std::string(f.name) == "Extra Attack";
  }
  CHECK_EQ(champion, true);
  CHECK_EQ(extra_attack, true);
  for (const ClassFeature& f : ClassFeatures2024(CharacterClass::kFighter, 4)) CHECK_EQ(f.level <= 4, true);
  CHECK_EQ(ClassFeatures2024(CharacterClass::kFighter, 20).size(), 29u);
  CHECK_EQ(ClassFeatures2024(CharacterClass::kWizard, 5).empty(), true);
  CHECK_EQ(HasFeatureList(CharacterClass::kFighter), true);
  CHECK_EQ(std::string(Subclass2024(CharacterClass::kFighter)) == "Champion", true);
  CHECK_EQ(std::string(Subclass2024(CharacterClass::kRogue)) == "Thief", true);
  CHECK_EQ(std::string(Subclass2024(CharacterClass::kWizard)) == "Evoker", true);
}

// Breaks caught: Champion crits before 3, 18 before 15, the gates off by one.
TEST(FighterFeatureLevels) {
  CHECK_EQ(ChampionCriticalFloor(0), 20);
  CHECK_EQ(ChampionCriticalFloor(2), 20);
  CHECK_EQ(ChampionCriticalFloor(3), 19);
  CHECK_EQ(ChampionCriticalFloor(14), 19);
  CHECK_EQ(ChampionCriticalFloor(15), 18);
  CHECK_EQ(HasRemarkableAthlete(2), false);
  CHECK_EQ(HasRemarkableAthlete(3), true);
  CHECK_EQ(HasStudiedAttacks(12), false);
  CHECK_EQ(HasStudiedAttacks(13), true);
  CHECK_EQ(IsFightingStyleLevel2024(CharacterClass::kFighter, 1), true);
  CHECK_EQ(IsFightingStyleLevel2024(CharacterClass::kFighter, 7), true);
  CHECK_EQ(IsFightingStyleLevel2024(CharacterClass::kFighter, 4), false);
  CHECK_EQ(IsFightingStyleLevel2024(CharacterClass::kPaladin, 2), false);  // with the Paladin's brief
}

// Breaks caught: Second Wind's uses not growing at 4 / 10, two tiers at once,
// a non-Fighter getting it.
TEST(GrantedFeatsFollowClassLevels) {
  CHECK_EQ(GrantedFeats2024({{CharacterClass::kFighter, 1}})[0], kFeatSecondWind2);
  CHECK_EQ(GrantedFeats2024({{CharacterClass::kFighter, 4}})[0], kFeatSecondWind3);
  CHECK_EQ(GrantedFeats2024({{CharacterClass::kFighter, 10}})[0], kFeatSecondWind4);
  CHECK_EQ(GrantedFeats2024({{CharacterClass::kFighter, 9}}).size(), 1u);
  CHECK_EQ(GrantedFeats2024({{CharacterClass::kWizard, 5}}).empty(), true);
  CHECK_EQ(GrantedFeats2024({{CharacterClass::kWizard, 3}, {CharacterClass::kFighter, 2}})[0], kFeatSecondWind2);
  CHECK_EQ(GrantableFeats2024().size(), 3u);
}
