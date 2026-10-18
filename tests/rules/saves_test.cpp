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

// C3: Great Fortitude / Lightning Reflexes / Iron Will give proficiency in
// the matching save (like 2024 Resilient). Breaks caught: a feat granting the
// wrong save, or none.
TEST(SaveFeatsGiveProficiency) {
  CHECK_EQ(FeatGivesSaveProficiency(NwnSave::kFortitude, true, false, false), true);
  CHECK_EQ(FeatGivesSaveProficiency(NwnSave::kReflex, true, false, false), false);
  CHECK_EQ(FeatGivesSaveProficiency(NwnSave::kReflex, false, true, false), true);
  CHECK_EQ(FeatGivesSaveProficiency(NwnSave::kWill, false, false, true), true);
  CHECK_EQ(FeatGivesSaveProficiency(NwnSave::kWill, true, true, false), false);
}

// C3: 2024 Paladin Aura of Protection. Breaks caught: before paladin level 6;
// a Charisma modifier of 0 or less not raised to +1; an unconscious paladin
// still projecting it; allies beyond 10 ft (3 m) included.
TEST(AuraOfProtection) {
  CHECK_EQ(AuraOfProtectionBonus(6, 3, true, 0.0f), 3);
  CHECK_EQ(AuraOfProtectionBonus(5, 3, true, 0.0f), 0);
  CHECK_EQ(AuraOfProtectionBonus(10, 0, true, 2.0f), 1);
  CHECK_EQ(AuraOfProtectionBonus(10, -1, true, 2.0f), 1);
  CHECK_EQ(AuraOfProtectionBonus(10, 4, false, 0.0f), 0);
  CHECK_EQ(AuraOfProtectionBonus(10, 4, true, 3.0f), 4);
  CHECK_EQ(AuraOfProtectionBonus(10, 4, true, 3.1f), 0);
}

// C2 NWN mode: a creature is proficient in a save its class tables make
// "good" (+2 at level 1). Breaks caught: poor tables (+0) counted.
TEST(GoodNwnSaveTables) {
  CHECK_EQ(IsGoodNwnSaveTable(2), true);
  CHECK_EQ(IsGoodNwnSaveTable(0), false);
  CHECK_EQ(IsGoodNwnSaveTable(1), false);
}

// C6: NWN's fixed DCs onto the 2024 scale, 10 + (DC - 10) / 2 rounded half
// up, within 10..30. Breaks caught: the PLAN.md examples drifting; low DCs
// dropping under 10; huge DCs unbounded.
TEST(NwnFixedDcsConvert) {
  CHECK_EQ(ConvertNwnFixedDc(14), 12);
  CHECK_EQ(ConvertNwnFixedDc(20), 15);
  CHECK_EQ(ConvertNwnFixedDc(26), 18);
  CHECK_EQ(ConvertNwnFixedDc(30), 20);
  CHECK_EQ(ConvertNwnFixedDc(40), 25);
  CHECK_EQ(ConvertNwnFixedDc(15), 13);
  CHECK_EQ(ConvertNwnFixedDc(10), 10);
  CHECK_EQ(ConvertNwnFixedDc(5), 10);
  CHECK_EQ(ConvertNwnFixedDc(60), 30);
}

// C5: 2024 spell scroll DCs by spell level. Breaks caught: any row of the
// table wrong; levels outside 0..9 not clamped.
TEST(SpellScrollDcs) {
  CHECK_EQ(SpellScrollDc(0), 13);
  CHECK_EQ(SpellScrollDc(1), 13);
  CHECK_EQ(SpellScrollDc(2), 13);
  CHECK_EQ(SpellScrollDc(3), 15);
  CHECK_EQ(SpellScrollDc(4), 15);
  CHECK_EQ(SpellScrollDc(5), 17);
  CHECK_EQ(SpellScrollDc(6), 17);
  CHECK_EQ(SpellScrollDc(7), 18);
  CHECK_EQ(SpellScrollDc(8), 18);
  CHECK_EQ(SpellScrollDc(9), 19);
  CHECK_EQ(SpellScrollDc(12), 19);
  CHECK_EQ(SpellScrollDc(-1), 13);
}

// C5 NWN mode: Spell Focus / Greater / Epic (+2 / +4 / +6 in NWN) count as
// +1 / +2 / +3.
TEST(SpellFocusInNwnMode) {
  CHECK_EQ(NwnModeSpellFocusDcBonus(0), 0);
  CHECK_EQ(NwnModeSpellFocusDcBonus(2), 1);
  CHECK_EQ(NwnModeSpellFocusDcBonus(4), 2);
  CHECK_EQ(NwnModeSpellFocusDcBonus(6), 3);
}

// C1-C3 put together. Breaks caught: a creature proficient in faithful mode;
// NWN mode losing creature good saves; a save feat counted twice in NWN mode
// (proficiency and +2); the aura in NWN mode or NWN's extras in faithful;
// magic effects dropped in either mode.
TEST(SaveValueByMode) {
  SaveFacts fighter;
  fighter.save = NwnSave::kFortitude;
  fighter.ability_modifier = 2;
  fighter.proficiency_bonus = 3;
  fighter.character = true;
  fighter.first_class = CharacterClass::kFighter;
  CHECK_EQ(SaveValue(fighter, RuleVariant::kFaithful), 5);
  CHECK_EQ(SaveValue(fighter, RuleVariant::kNwn), 5);
  SaveFacts fighter_will = fighter;
  fighter_will.save = NwnSave::kWill;
  fighter_will.ability_modifier = 1;
  CHECK_EQ(SaveValue(fighter_will, RuleVariant::kFaithful), 1);
  fighter_will.save_feat = true;  // Iron Will
  fighter_will.nwn_extras = 2;    // NWN's +2 for it
  CHECK_EQ(SaveValue(fighter_will, RuleVariant::kFaithful), 4);
  CHECK_EQ(SaveValue(fighter_will, RuleVariant::kNwn), 3);
  SaveFacts ogre;
  ogre.save = NwnSave::kFortitude;
  ogre.ability_modifier = 4;
  ogre.proficiency_bonus = 2;
  ogre.good_nwn_table = true;
  CHECK_EQ(SaveValue(ogre, RuleVariant::kFaithful), 4);
  CHECK_EQ(SaveValue(ogre, RuleVariant::kNwn), 6);
  SaveFacts buffed = fighter;
  buffed.effects = 1;
  buffed.aura = 3;
  buffed.nwn_extras = 2;
  CHECK_EQ(SaveValue(buffed, RuleVariant::kFaithful), 9);
  CHECK_EQ(SaveValue(buffed, RuleVariant::kNwn), 8);
}

// C4: faithful has no automatic success or failure; NWN mode: a natural 20
// always saves, a natural 1 always fails when the game option is on.
TEST(NaturalRollsOnSaves) {
  CHECK_EQ(SaveSucceeds(20, 0, 25, false, false), false);
  CHECK_EQ(SaveSucceeds(20, 0, 25, true, false), true);
  CHECK_EQ(SaveSucceeds(1, 15, 10, false, false), true);
  CHECK_EQ(SaveSucceeds(1, 15, 10, false, true), false);
  CHECK_EQ(SaveSucceeds(12, 3, 15, false, false), true);
  CHECK_EQ(SaveSucceeds(11, 3, 15, false, false), false);
}
