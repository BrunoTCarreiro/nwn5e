#pragma once

// Saving throws (SRD 5.2): six saves, one per ability; d20 + ability modifier
// + proficiency bonus when proficient. A character is proficient in the two
// saves of its first class (multiclassing grants no save proficiencies).
// Phase 1c — the NWN mapping below is the proposal in PLAN.md.

#include <optional>

#include "rules/combat.h"
#include "rules/modes.h"

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

// What a 2024 save value is built from (C1–C3).
struct SaveFacts {
  NwnSave save = NwnSave::kFortitude;
  int ability_modifier = 0;   // of AbilityForNwnSave(save)
  int proficiency_bonus = 2;
  bool character = false;     // PC, henchman, or an NPC with a real first class
  std::optional<CharacterClass> first_class;
  bool save_feat = false;     // Great Fortitude / Lightning Reflexes / Iron Will
  bool good_nwn_table = false;  // creature with a "good" NWN table for this save
  int effects = 0;            // magic effects (items, spells)
  int aura = 0;               // Aura of Protection
  int nwn_extras = 0;         // NWN's flat feat/racial/blueprint bonuses
};

// Faithful: ability modifier + proficiency bonus when proficient (characters:
// first class, or the save feat) + effects + aura. NWN mode: proficiency for
// characters from the first class, for creatures from a good NWN table; the
// save feats give NWN's flat bonus instead (in nwn_extras); no aura.
int SaveValue(const SaveFacts& facts, RuleVariant variant);

// C4: whether a save succeeds. A natural 20 succeeds automatically only with
// `natural_20_succeeds`, a natural 1 fails automatically only with
// `natural_1_fails` (faithful: neither).
bool SaveSucceeds(int d20, int bonus, int dc, bool natural_20_succeeds, bool natural_1_fails);

}  // namespace rules
