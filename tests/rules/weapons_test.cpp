#include "check.h"
#include "rules/weapons.h"

using namespace rules;

// Phase 1d D4 (proposed). NWN base item ids from baseitems.2da. Breaks
// caught: a 2024 weapon with the wrong property; an NWN-only weapon left
// without one; gloves, creature weapons or unknown ids given a property.
TEST(NwnWeaponsGetTheir2024Mastery) {
  CHECK_EQ(MasteryForNwnBaseItem(22), Mastery::kNick);     // dagger
  CHECK_EQ(MasteryForNwnBaseItem(1), Mastery::kSap);       // longsword
  CHECK_EQ(MasteryForNwnBaseItem(13), Mastery::kGraze);    // greatsword
  CHECK_EQ(MasteryForNwnBaseItem(18), Mastery::kCleave);   // greataxe
  CHECK_EQ(MasteryForNwnBaseItem(51), Mastery::kVex);      // rapier
  CHECK_EQ(MasteryForNwnBaseItem(11), Mastery::kVex);      // shortbow
  CHECK_EQ(MasteryForNwnBaseItem(8), Mastery::kSlow);      // longbow
  CHECK_EQ(MasteryForNwnBaseItem(5), Mastery::kPush);      // warhammer
  CHECK_EQ(MasteryForNwnBaseItem(50), Mastery::kTopple);   // quarterstaff
  CHECK_EQ(MasteryForNwnBaseItem(6), Mastery::kPush);      // heavy crossbow
  CHECK_EQ(MasteryForNwnBaseItem(7), Mastery::kSlow);      // light crossbow
  CHECK_EQ(MasteryForNwnBaseItem(41), Mastery::kSap);      // katana (NWN only)
  CHECK_EQ(MasteryForNwnBaseItem(42), Mastery::kNick);     // kukri (NWN only)
  CHECK_EQ(MasteryForNwnBaseItem(55), Mastery::kGraze);    // scythe (NWN only)
  CHECK_EQ(MasteryForNwnBaseItem(32), Mastery::kCleave);   // dire mace (NWN only)
  CHECK_EQ(MasteryForNwnBaseItem(108), Mastery::kTopple);  // dwarven waraxe (NWN only)
  CHECK_EQ(MasteryForNwnBaseItem(36), Mastery::kNone);     // gloves (unarmed)
  CHECK_EQ(MasteryForNwnBaseItem(69), Mastery::kNone);     // creature slashing weapon
  CHECK_EQ(MasteryForNwnBaseItem(-1), Mastery::kNone);
  CHECK_EQ(MasteryForNwnBaseItem(999), Mastery::kNone);
}

// Phase 1d D3 (proposed): the 2024 classes with Weapon Mastery. Breaks
// caught: Monk (no mastery in 2024) or a full caster given it; a martial
// class left out.
TEST(WeaponMasteryClassesAre2024) {
  CHECK_EQ(HasWeaponMastery(CharacterClass::kBarbarian), true);
  CHECK_EQ(HasWeaponMastery(CharacterClass::kFighter), true);
  CHECK_EQ(HasWeaponMastery(CharacterClass::kPaladin), true);
  CHECK_EQ(HasWeaponMastery(CharacterClass::kRanger), true);
  CHECK_EQ(HasWeaponMastery(CharacterClass::kRogue), true);
  CHECK_EQ(HasWeaponMastery(CharacterClass::kMonk), false);
  CHECK_EQ(HasWeaponMastery(CharacterClass::kWizard), false);
  CHECK_EQ(HasWeaponMastery(CharacterClass::kCleric), false);
  CHECK_EQ(HasWeaponMastery(CharacterClass::kBard), false);
}

// Phase 1d D1 (proposed): the extra (off-hand) attack adds no ability
// modifier to damage unless it is negative or the attacker has the
// Two-Weapon Fighting style (NWN's Two-Weapon Fighting feat). Breaks caught:
// NWN's half Strength kept; a negative modifier dropped; the style ignored.
TEST(OffHandDamageModifier) {
  CHECK_EQ(OffHandDamageAbilityModifier(3, false), 0);
  CHECK_EQ(OffHandDamageAbilityModifier(3, true), 3);
  CHECK_EQ(OffHandDamageAbilityModifier(-1, false), -1);
  CHECK_EQ(OffHandDamageAbilityModifier(-1, true), -1);
  CHECK_EQ(OffHandDamageAbilityModifier(0, false), 0);
}
