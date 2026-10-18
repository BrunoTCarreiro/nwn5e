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

bool FeatGivesSaveProficiency(NwnSave save, bool great_fortitude, bool lightning_reflexes, bool iron_will) {
  switch (save) {
    case NwnSave::kFortitude:
      return great_fortitude;
    case NwnSave::kReflex:
      return lightning_reflexes;
    case NwnSave::kWill:
      return iron_will;
  }
  return false;
}

int AuraOfProtectionBonus(int paladin_level, int charisma_modifier, bool conscious, float distance_m) {
  if (paladin_level < 6 || !conscious || !(distance_m <= 3.0f)) return 0;
  return charisma_modifier > 1 ? charisma_modifier : 1;
}

bool IsGoodNwnSaveTable(int bonus_at_level_1) { return bonus_at_level_1 >= 2; }

int ConvertNwnFixedDc(int dc) {
  const int half = dc >= 10 ? (dc - 10 + 1) / 2 : -((10 - dc) / 2);
  const int converted = 10 + half;
  return converted < 10 ? 10 : (converted > 30 ? 30 : converted);
}

int SpellScrollDc(int spell_level) {
  if (spell_level <= 2) return 13;
  if (spell_level <= 4) return 15;
  if (spell_level <= 6) return 17;
  if (spell_level <= 8) return 18;
  return 19;
}

int NwnModeSpellFocusDcBonus(int nwn_focus_bonus) { return nwn_focus_bonus / 2; }

bool SaveSucceeds(int d20, int bonus, int dc, bool natural_20_succeeds, bool natural_1_fails) {
  if (d20 == 20 && natural_20_succeeds) return true;
  if (d20 == 1 && natural_1_fails) return false;
  return d20 + bonus >= dc;
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
