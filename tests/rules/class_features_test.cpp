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
