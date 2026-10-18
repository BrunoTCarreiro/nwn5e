#include "rules/saves.h"

namespace rules {

bool HasSaveProficiency(CharacterClass first_class, Ability ability) {
  using A = Ability;
  switch (first_class) {
    case CharacterClass::kBarbarian:
    case CharacterClass::kFighter:
      return ability == A::kStrength || ability == A::kConstitution;
    case CharacterClass::kBard:
      return ability == A::kDexterity || ability == A::kCharisma;
    case CharacterClass::kCleric:
    case CharacterClass::kPaladin:
    case CharacterClass::kWarlock:
      return ability == A::kWisdom || ability == A::kCharisma;
    case CharacterClass::kDruid:
    case CharacterClass::kWizard:
      return ability == A::kIntelligence || ability == A::kWisdom;
    case CharacterClass::kMonk:
    case CharacterClass::kRanger:
      return ability == A::kStrength || ability == A::kDexterity;
    case CharacterClass::kRogue:
      return ability == A::kDexterity || ability == A::kIntelligence;
    case CharacterClass::kSorcerer:
      return ability == A::kConstitution || ability == A::kCharisma;
  }
  return false;
}

int SavingThrowBonus(int ability_modifier, bool proficient, int proficiency_bonus, int other_bonus) {
  return ability_modifier + (proficient ? proficiency_bonus : 0) + other_bonus;
}

Ability AbilityForNwnSave(NwnSave save) {
  switch (save) {
    case NwnSave::kFortitude:
      return Ability::kConstitution;
    case NwnSave::kReflex:
      return Ability::kDexterity;
    case NwnSave::kWill:
      return Ability::kWisdom;
  }
  return Ability::kConstitution;
}

}  // namespace rules
