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

// C3: Great Fortitude / Lightning Reflexes / Iron Will -> proficiency in the
// matching save (2024 Resilient stand-in).
bool FeatGivesSaveProficiency(NwnSave save, bool great_fortitude, bool lightning_reflexes, bool iron_will);

// C3: Paladin Aura of Protection — the paladin's Charisma modifier (at least
// +1) for the paladin and allies within 10 ft (3 m), from paladin level 6,
// while the paladin is conscious.
int AuraOfProtectionBonus(int paladin_level, int charisma_modifier, bool conscious, float distance_m);

// C2 NWN mode: a class save table is "good" when it gives +2 at level 1.
bool IsGoodNwnSaveTable(int bonus_at_level_1);

// C6: an NWN fixed DC on the 2024 scale: 10 + (DC - 10) / 2 rounded half up,
// within 10..30.
int ConvertNwnFixedDc(int dc);

// C5: the 2024 spell scroll DC for a spell level (13 / 15 / 17 / 18 / 19).
int SpellScrollDc(int spell_level);

// C5 NWN mode: NWN's Spell Focus DC bonus (+2 / +4 / +6) as +1 / +2 / +3.
int NwnModeSpellFocusDcBonus(int nwn_focus_bonus);

// C4: whether a save succeeds. A natural 20 succeeds automatically only with
// `natural_20_succeeds`, a natural 1 fails automatically only with
// `natural_1_fails` (faithful: neither).
bool SaveSucceeds(int d20, int bonus, int dc, bool natural_20_succeeds, bool natural_1_fails);

}  // namespace rules
