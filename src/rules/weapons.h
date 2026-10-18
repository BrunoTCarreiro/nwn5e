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

// D1: the 2024 Light weapons among NWN base items (club, dagger, handaxe,
// light hammer, sickle, scimitar, shortsword, plus kukri and kama).
bool IsLightNwnBaseItem(int base_item);

// D1: off-hand attacks per round. `nwn_attacks` is the engine's count (0 when
// it sees no second weapon; 2 with Improved Two-Weapon Fighting). 2024: one
// extra attack at most — with two Light weapons, a double weapon, or Dual
// Wielder (Improved Two-Weapon Fighting) with any weapons NWN allows.
int OffHandAttacks(int nwn_attacks, bool double_weapon, bool dual_wielder, bool main_hand_light,
                   bool off_hand_light);

// NWN's off-hand Strength damage: half the modifier (truncated) when it is
// positive, all of it when not (0x697D10).
int NwnOffHandStrengthDamage(int strength_modifier);

}  // namespace rules
