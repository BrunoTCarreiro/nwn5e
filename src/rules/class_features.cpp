#include "rules/class_features.h"

namespace rules {

namespace {

// SRD 5.2 Fighter and Champion (PLAN.md "Phase 3 — Fighter"); statuses as
// of the brief, updated as its decisions are built.
const std::vector<ClassFeature>& Fighter() {
  static const std::vector<ClassFeature> kFeatures = {
      {1, "Fighting Style", false, "built: Archery, Two-Weapon Fighting (Defense, Great Weapon Fighting: being built)"},
      {1, "Second Wind", false, "built: radial menu (an action until Phase 6), 2 / 3 / 4 uses per rest"},
      {1, "Weapon Mastery", false, "built (!mastery)"},
      {2, "Action Surge", false, "Phase 6"},
      {2, "Tactical Mind", false, "Phase 7 (a choice)"},
      {3, "Champion", false, "built: granted at 3"},
      {3, "Improved Critical", true, "built: crits on 19-20"},
      {3, "Remarkable Athlete", true, "built: advantage on initiative (the move after a crit: Phase 6)"},
      {4, "Ability Score Improvement", false, "built (!asi)"},
      {5, "Extra Attack", false, "built"},
      {5, "Tactical Shift", false, "built: no opportunity attacks the round you use Second Wind"},
      {6, "Ability Score Improvement", false, "built (!asi)"},
      {7, "Additional Fighting Style", true, "built: a second style"},
      {8, "Ability Score Improvement", false, "built (!asi)"},
      {9, "Indomitable", false, "brief F5"},
      {9, "Tactical Master", false, "later (a per-attack choice)"},
      {10, "Heroic Warrior", true, "with Heroic Inspiration (CM6)"},
      {11, "Two Extra Attacks", false, "built"},
      {12, "Ability Score Improvement", false, "built (!asi)"},
      {13, "Indomitable (2 uses)", false, "brief F5"},
      {13, "Studied Attacks", false, "built"},
      {14, "Ability Score Improvement", false, "built (!asi)"},
      {15, "Superior Critical", true, "built: crits on 18-20"},
      {16, "Ability Score Improvement", false, "built (!asi)"},
      {17, "Action Surge (2 uses)", false, "Phase 6"},
      {17, "Indomitable (3 uses)", false, "brief F5"},
      {18, "Survivor", true, "brief C5"},
      {19, "Epic Boon", false, "CM8"},
      {20, "Three Extra Attacks", false, "built"},
  };
  return kFeatures;
}

}  // namespace

const char* Subclass2024(CharacterClass character_class) {
  switch (character_class) {
    case CharacterClass::kBarbarian: return "Path of the Berserker";
    case CharacterClass::kBard: return "College of Lore";
    case CharacterClass::kCleric: return "Life Domain";
    case CharacterClass::kDruid: return "Circle of the Land";
    case CharacterClass::kFighter: return "Champion";
    case CharacterClass::kMonk: return "Warrior of the Open Hand";
    case CharacterClass::kPaladin: return "Oath of Devotion";
    case CharacterClass::kRanger: return "Hunter";
    case CharacterClass::kRogue: return "Thief";
    case CharacterClass::kSorcerer: return "Draconic Sorcery";
    case CharacterClass::kWarlock: return "Fiend Patron";
    case CharacterClass::kWizard: return "Evoker";
  }
  return "";
}

bool HasFeatureList(CharacterClass character_class) { return character_class == CharacterClass::kFighter; }

int ChampionCriticalFloor(int fighter_level) { return fighter_level >= 15 ? 18 : fighter_level >= 3 ? 19 : 20; }

bool HasRemarkableAthlete(int fighter_level) { return fighter_level >= 3; }

bool HasStudiedAttacks(int fighter_level) { return fighter_level >= 13; }

std::vector<int> GrantedFeats2024(const std::vector<ClassLevels>& classes) {
  std::vector<int> feats;
  for (const ClassLevels& c : classes) {
    if (c.character_class != CharacterClass::kFighter || c.levels < 1) continue;
    feats.push_back(c.levels >= 10 ? kFeatSecondWind4 : c.levels >= 4 ? kFeatSecondWind3 : kFeatSecondWind2);
  }
  return feats;
}

const std::vector<int>& GrantableFeats2024() {
  static const std::vector<int> kFeats = {kFeatSecondWind2, kFeatSecondWind3, kFeatSecondWind4};
  return kFeats;
}

bool IsFightingStyleLevel2024(CharacterClass character_class, int class_level) {
  return character_class == CharacterClass::kFighter && (class_level == 1 || class_level == 7);
}

std::vector<ClassFeature> ClassFeatures2024(CharacterClass character_class, int level) {
  std::vector<ClassFeature> features;
  if (character_class != CharacterClass::kFighter) return features;
  for (const ClassFeature& feature : Fighter()) {
    if (feature.level <= level) features.push_back(feature);
  }
  return features;
}

}  // namespace rules
