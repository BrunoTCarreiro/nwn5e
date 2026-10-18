#pragma once

// Phase 1d (proposed — PLAN.md "Phase 1d proposal"): 2024 weapon mastery
// properties on NWN weapons, the classes that use them, and two-weapon
// fighting damage.

#include "rules/combat.h"

namespace rules {

enum class Mastery { kNone, kCleave, kGraze, kNick, kPush, kSap, kSlow, kTopple, kVex };

// The mastery property of an NWN base item (baseitems.2da row); NWN-only
// weapons take the closest 2024 weapon's. kNone for anything that is not a
// manufactured weapon.
Mastery MasteryForNwnBaseItem(int base_item);

// D3: Barbarian, Fighter, Paladin, Ranger and Rogue have Weapon Mastery.
bool HasWeaponMastery(CharacterClass character_class);

// D1: the extra (off-hand) attack adds the ability modifier to damage only
// when it is negative or with the Two-Weapon Fighting style.
int OffHandDamageAbilityModifier(int ability_modifier, bool two_weapon_fighting_style);

}  // namespace rules
