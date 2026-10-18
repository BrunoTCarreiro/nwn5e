#pragma once

// Saving throws (SRD 5.2): six saves, one per ability; d20 + ability modifier
// + proficiency bonus when proficient. A character is proficient in the two
// saves of its first class (multiclassing grants no save proficiencies).
// Phase 1c — the NWN mapping below is the proposal in PLAN.md.

#include "rules/combat.h"

namespace rules {

enum class Ability { kStrength, kDexterity, kConstitution, kIntelligence, kWisdom, kCharisma };

bool HasSaveProficiency(CharacterClass first_class, Ability ability);

// Ability modifier + proficiency bonus (if proficient) + other bonuses
// (effects, items).
int SavingThrowBonus(int ability_modifier, bool proficient, int proficiency_bonus, int other_bonus);

// NWN asks for one of three 3e saves; each is read as one 2024 save
// (proposed: Fortitude -> Constitution, Reflex -> Dexterity, Will -> Wisdom).
enum class NwnSave { kFortitude, kReflex, kWill };
Ability AbilityForNwnSave(NwnSave save);

}  // namespace rules
