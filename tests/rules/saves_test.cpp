#include "check.h"
#include "rules/saves.h"

using namespace rules;

// SRD 5.2 class tables. Breaks caught: a class with the wrong pair (e.g. the
// 3e good saves: Monk all three, Rogue Reflex only), or a class proficient in
// more or fewer than two saves.
TEST(ClassSaveProficienciesAre2024) {
  CHECK_EQ(HasSaveProficiency(CharacterClass::kFighter, Ability::kStrength), true);
  CHECK_EQ(HasSaveProficiency(CharacterClass::kFighter, Ability::kConstitution), true);
  CHECK_EQ(HasSaveProficiency(CharacterClass::kFighter, Ability::kDexterity), false);
  CHECK_EQ(HasSaveProficiency(CharacterClass::kRogue, Ability::kDexterity), true);
  CHECK_EQ(HasSaveProficiency(CharacterClass::kRogue, Ability::kIntelligence), true);
  CHECK_EQ(HasSaveProficiency(CharacterClass::kWizard, Ability::kIntelligence), true);
  CHECK_EQ(HasSaveProficiency(CharacterClass::kWizard, Ability::kWisdom), true);
  CHECK_EQ(HasSaveProficiency(CharacterClass::kWizard, Ability::kConstitution), false);
  CHECK_EQ(HasSaveProficiency(CharacterClass::kMonk, Ability::kStrength), true);
  CHECK_EQ(HasSaveProficiency(CharacterClass::kMonk, Ability::kDexterity), true);
  CHECK_EQ(HasSaveProficiency(CharacterClass::kMonk, Ability::kWisdom), false);
  CHECK_EQ(HasSaveProficiency(CharacterClass::kBarbarian, Ability::kStrength), true);
  CHECK_EQ(HasSaveProficiency(CharacterClass::kBarbarian, Ability::kConstitution), true);
  CHECK_EQ(HasSaveProficiency(CharacterClass::kBard, Ability::kDexterity), true);
  CHECK_EQ(HasSaveProficiency(CharacterClass::kBard, Ability::kCharisma), true);
  CHECK_EQ(HasSaveProficiency(CharacterClass::kCleric, Ability::kWisdom), true);
  CHECK_EQ(HasSaveProficiency(CharacterClass::kCleric, Ability::kCharisma), true);
  CHECK_EQ(HasSaveProficiency(CharacterClass::kDruid, Ability::kIntelligence), true);
  CHECK_EQ(HasSaveProficiency(CharacterClass::kDruid, Ability::kWisdom), true);
  CHECK_EQ(HasSaveProficiency(CharacterClass::kPaladin, Ability::kWisdom), true);
  CHECK_EQ(HasSaveProficiency(CharacterClass::kPaladin, Ability::kCharisma), true);
  CHECK_EQ(HasSaveProficiency(CharacterClass::kRanger, Ability::kStrength), true);
  CHECK_EQ(HasSaveProficiency(CharacterClass::kRanger, Ability::kDexterity), true);
  CHECK_EQ(HasSaveProficiency(CharacterClass::kSorcerer, Ability::kConstitution), true);
  CHECK_EQ(HasSaveProficiency(CharacterClass::kSorcerer, Ability::kCharisma), true);
  CHECK_EQ(HasSaveProficiency(CharacterClass::kWarlock, Ability::kWisdom), true);
  CHECK_EQ(HasSaveProficiency(CharacterClass::kWarlock, Ability::kCharisma), true);
  const CharacterClass all[] = {CharacterClass::kBarbarian, CharacterClass::kBard,    CharacterClass::kCleric,
                                CharacterClass::kDruid,     CharacterClass::kFighter, CharacterClass::kMonk,
                                CharacterClass::kPaladin,   CharacterClass::kRanger,  CharacterClass::kRogue,
                                CharacterClass::kSorcerer,  CharacterClass::kWarlock, CharacterClass::kWizard};
  const Ability abilities[] = {Ability::kStrength,     Ability::kDexterity, Ability::kConstitution,
                               Ability::kIntelligence, Ability::kWisdom,    Ability::kCharisma};
  for (CharacterClass c : all) {
    int count = 0;
    for (Ability a : abilities) count += HasSaveProficiency(c, a) ? 1 : 0;
    CHECK_EQ(count, 2);
  }
}

// Breaks caught: the proficiency bonus added without proficiency, or other
// bonuses (effects, items) dropped.
TEST(SavingThrowBonusIsModifierPlusProficiency) {
  CHECK_EQ(SavingThrowBonus(3, true, 2, 0), 5);
  CHECK_EQ(SavingThrowBonus(3, false, 2, 0), 3);
  CHECK_EQ(SavingThrowBonus(-1, true, 3, 0), 2);
  CHECK_EQ(SavingThrowBonus(2, false, 4, 1), 3);
}

// Phase 1c proposed default (PLAN.md). Breaks caught: a 3e save read with the
// wrong ability.
TEST(NwnSavesMapToAbilities) {
  CHECK_EQ(AbilityForNwnSave(NwnSave::kFortitude), Ability::kConstitution);
  CHECK_EQ(AbilityForNwnSave(NwnSave::kReflex), Ability::kDexterity);
  CHECK_EQ(AbilityForNwnSave(NwnSave::kWill), Ability::kWisdom);
}
