// Phase 1c (PLAN.md C3–C6), with `saves` on:
// - C5 spell DC: 8 + proficiency bonus + casting modifier (class casters),
//   the 2024 scroll DCs for spells cast from items, 8 + proficiency bonus +
//   Charisma for class-less abilities; NWN mode adds Spell Focus as +1/+2/+3.
//   Each DC handed out is remembered with its caster.
// - C6: any other DC reaching the save roll (traps, poisons, item powers,
//   scripted monster abilities) is converted to the 2024 scale.
// - C4 (faithful): natural 1s and 20s on saves are not automatic.
// - C7 (faithful): the save roll's own feat and racial bonuses ("+2 vs X")
//   are dropped; item and spell effects stay.
// - C3 species: advantage for dwarves vs poison, halflings vs fear, elves vs
//   mind spells (charm), gnomes on Will (Wisdom) saves; Halfling Luck rerolls
//   a natural 1. Both modes.

#include <stdint.h>
#include <string.h>

#include <utility>

#include "config.h"
#include "creature.h"
#include "hooks.h"
#include "log.h"
#include "log_once.h"
#include "nwmain_1_69.h"
#include "rules/core.h"
#include "rules/nwn_conversion.h"
#include "rules/saves.h"

namespace {

using SpellDcFn = int(__fastcall*)(void* creature, void* edx, int spell_id);
using SaveRollFn = uint32_t(__fastcall*)(void* creature, void* edx, uint32_t save, uint32_t dc, uint32_t type,
                                         uint32_t versus, uint32_t a5, uint32_t a6, uint32_t a7);
using RollDiceFn = int(__fastcall*)(void* rules_object, void* edx, int count, int sides);
using GetSpellFn = void*(__fastcall*)(void* spells, void* edx, int id);
using StatsByteFn = int(__fastcall*)(void* stats, void* edx, int which);
using HasFeatFn = int(__fastcall*)(void* stats, void* edx, int feat);

SpellDcFn g_calculate_spell_save_dc = nullptr;
SaveRollFn g_saving_throw_roll = nullptr;
const auto g_roll_dice = reinterpret_cast<RollDiceFn>(nwmain::kRollDice);
const auto g_get_spell = reinterpret_cast<GetSpellFn>(nwmain::kGetSpell);
const auto g_get_casting_modifier = reinterpret_cast<StatsByteFn>(nwmain::kGetCastingAbilityModifier);
const auto g_has_feat = reinterpret_cast<HasFeatFn>(nwmain::kHasFeat);

LogOnce g_dc_logged(2000);
int g_save_log_lines = 0;

// DCs handed out by CalculateSpellSaveDC, so the save roll can tell them from
// fixed DCs (C6 converts only the latter).
struct SpellDc {
  uint32_t caster;
  int dc;
};
SpellDc g_spell_dcs[32] = {};
int g_next_spell_dc = 0;

void RememberSpellDc(uint32_t caster, int dc) {
  g_spell_dcs[g_next_spell_dc] = {caster, dc};
  g_next_spell_dc = (g_next_spell_dc + 1) % 32;
}

bool IsSpellDc(uint32_t caster, int dc) {
  for (const SpellDc& entry : g_spell_dcs) {
    if (entry.caster == caster && entry.dc == dc && caster != 0) return true;
  }
  return false;
}

// The save being rolled, for the d20 replacement.
struct SaveContext {
  bool active = false;
  void* creature = nullptr;
  int save = 0;  // 1 Fortitude, 2 Reflex, 3 Will
  int type = 0;  // NWN save type
};
SaveContext g_save;

bool HasFeat(void* stats, int feat) { return g_has_feat(stats, nullptr, feat) != 0; }

bool SummonLike(int associate_type) { return associate_type >= 2 && associate_type <= 5; }

bool CountsAsCharacter(void* stats, const CreatureFacts& facts) {
  if (facts.character) return true;
  const std::vector<rules::NwnClassSlot> slots = ReadClassSlots(stats);
  return !slots.empty() && rules::ClassFromNwnId(slots[0].nwn_class_id) && !SummonLike(facts.associate_type);
}

int SpellFocusBonus(void* stats, int school) {
  if (school < 1 || school > 8) return 0;
  if (HasFeat(stats, 0x262 + school - 1)) return 6;
  if (HasFeat(stats, 0x189 + school - 1)) return 4;
  if (HasFeat(stats, school == 1 ? 0x23 : 0xA6 + school - 2)) return 2;
  return 0;
}

int __fastcall CalculateSpellSaveDc(void* creature, void* edx, int spell_id) {
  const int engine_dc = g_calculate_spell_save_dc(creature, edx, spell_id);
  if (g_config.saves == rules::RuleVariant::kOff) return engine_dc;
  void* stats = Field<void*>(creature, nwmain::kCreatureStats);
  void* rules_object = *reinterpret_cast<void**>(nwmain::kRulesObject);
  void* spells = rules_object ? Field<void*>(rules_object, nwmain::kRulesSpells) : nullptr;
  const void* spell = spells ? g_get_spell(spells, nullptr, spell_id) : nullptr;
  if (!stats || !spell) return engine_dc;
  const CreatureFacts facts = ReadCreatureFacts(stats);
  if (!facts.plausible) return engine_dc;
  const bool character = CountsAsCharacter(stats, facts);
  const int pb = rules::ProficiencyBonusForCreature(character, facts.level, facts.challenge_rating);
  const int charisma = Field<int8_t>(stats, nwmain::kStatsChaModifier);
  const int slot = Field<uint8_t>(creature, nwmain::kCreatureCastingSlot);
  const char* source = "class";
  int dc = 0;
  if (Field<int32_t>(creature, nwmain::kCreatureCastingFromItem) != 0) {
    dc = rules::SpellScrollDc(Field<uint8_t>(spell, nwmain::kSpellInnateLevel));
    source = "item";
  } else if (slot == 0xFF || slot == 0xFE) {
    dc = rules::SaveDifficultyClass(charisma, pb);  // class-less or innate ability: 8 + PB + Cha
    source = "ability";
  } else {
    const int modifier = static_cast<int8_t>(g_get_casting_modifier(stats, nullptr, slot) & 0xFF);
    dc = rules::SaveDifficultyClass(modifier, pb);
    if (g_config.saves == rules::RuleVariant::kNwn) {
      dc += rules::NwnModeSpellFocusDcBonus(SpellFocusBonus(stats, Field<uint8_t>(spell, nwmain::kSpellSchool)));
    }
  }
  RememberSpellDc(Field<uint32_t>(creature, nwmain::kCreatureObjectId), dc);
  if (g_config.log_attacks && g_dc_logged.First(LogOnce::Key(ObjectIdOf(stats), engine_dc, dc))) {
    Log("spell DC: object 0x%08X %s spell %d (%s): 3e %d -> 5e %d (PB %d)", ObjectIdOf(stats), KindOf(facts),
        spell_id, source, engine_dc, dc, pb);
  }
  return dc;
}

uint32_t __fastcall SavingThrowRoll(void* creature, void* edx, uint32_t save, uint32_t dc, uint32_t type,
                                    uint32_t versus, uint32_t a5, uint32_t a6, uint32_t a7) {
  if (g_config.saves == rules::RuleVariant::kOff) {
    return g_saving_throw_roll(creature, edx, save, dc, type, versus, a5, a6, a7);
  }
  const int engine_dc = dc & 0xFFFF;
  const bool spell_dc = IsSpellDc(versus, engine_dc);
  const int new_dc = spell_dc ? engine_dc : rules::ConvertNwnFixedDc(engine_dc);
  const SaveContext outer = g_save;
  g_save = {true, creature, static_cast<int>(save & 0xFF), static_cast<int>(type & 0xFF)};
  const uint32_t result =
      g_saving_throw_roll(creature, edx, save, (dc & 0xFFFF0000u) | static_cast<uint32_t>(new_dc), type, versus, a5,
                          a6, a7);
  g_save = outer;
  if (g_config.log_attacks && g_save_log_lines < 300) {
    ++g_save_log_lines;
    Log("save roll: object 0x%08X save %d type %d vs 0x%08X: DC %d%s -> %d, %s",
        Field<uint32_t>(creature, nwmain::kCreatureObjectId), save & 0xFF, type & 0xFF, versus, engine_dc,
        spell_dc ? " (spell)" : "", new_dc, result == 1 ? "success" : (result == 2 ? "immune" : "failure"));
  }
  return result;
}

bool SpeciesAdvantage(void* stats, int save, int type) {
  const int race = Field<uint16_t>(stats, nwmain::kStatsRace);
  return (race == nwmain::kRaceDwarf && type == nwmain::kSaveTypePoison) ||
         (race == nwmain::kRaceHalfling && type == nwmain::kSaveTypeFear) ||
         (race == nwmain::kRaceElf && type == nwmain::kSaveTypeMindSpells) || (race == nwmain::kRaceGnome && save == 3);
}

// Replaces the save roll's RollDice(1, 20).
int __fastcall RollSaveD20(void* rules_object, void* edx, int count, int sides) {
  if (!g_save.active) return g_roll_dice(rules_object, edx, count, sides);
  void* stats = Field<void*>(g_save.creature, nwmain::kCreatureStats);
  int kept = g_roll_dice(rules_object, edx, count, sides) & 0xFFFF;
  const bool advantage = stats && SpeciesAdvantage(stats, g_save.save, g_save.type);
  int second = 0;
  if (advantage) {
    second = g_roll_dice(rules_object, edx, count, sides) & 0xFFFF;
    if (second > kept) std::swap(kept, second);
  }
  bool lucky = false;
  if (kept == 1 && stats && Field<uint16_t>(stats, nwmain::kStatsRace) == nwmain::kRaceHalfling) {
    kept = g_roll_dice(rules_object, edx, count, sides) & 0xFFFF;  // Halfling Luck: reroll a natural 1
    lucky = true;
  }
  if ((advantage || lucky) && g_config.log_attacks && g_save_log_lines < 300) {
    ++g_save_log_lines;
    Log("save d20: object 0x%08X save %d type %d:%s%s -> %d", Field<uint32_t>(g_save.creature, nwmain::kCreatureObjectId),
        g_save.save, g_save.type, advantage ? " species advantage" : "", lucky ? " Halfling Luck reroll" : "", kept);
  }
  return kept;
}

// C7: stands in for HasFeat at the save roll's feat-bonus checks (faithful).
int __fastcall NoFeatBonus(void* stats, void* edx, int feat) {
  (void)stats;
  (void)edx;
  (void)feat;
  return 0;
}

bool PatchCallSite(const char* name, uintptr_t site, uintptr_t expected_target, const void* new_target) {
  const auto* code = reinterpret_cast<const uint8_t*>(site);
  int32_t rel = 0;
  memcpy(&rel, code + 1, sizeof(rel));
  if (code[0] != 0xe8 || site + nwmain::kCallLength + rel != expected_target) {
    Log("SKIP %s: 0x%08X is not the mapped call", name, site);
    return false;
  }
  uint8_t expected[5];
  memcpy(expected, code, sizeof(expected));
  const int32_t new_rel =
      static_cast<int32_t>(reinterpret_cast<uintptr_t>(new_target) - (site + nwmain::kCallLength));
  uint8_t call[5] = {0xe8};
  memcpy(call + 1, &new_rel, sizeof(new_rel));
  return PatchBytes(name, site, expected, call, sizeof(call));
}

}  // namespace

void InstallSaveRollRules() {
  if (g_config.saves == rules::RuleVariant::kOff) return;
  InstallHook({"CNWSCreature::CalculateSpellSaveDC (2024 spell DC)", nwmain::kCalculateSpellSaveDc,
               nwmain::kCalculateSpellSaveDcPrologue, sizeof(nwmain::kCalculateSpellSaveDcPrologue),
               reinterpret_cast<void*>(&CalculateSpellSaveDc), reinterpret_cast<void**>(&g_calculate_spell_save_dc)});
  InstallHook({"CNWSCreature saving throw roll (DCs, species advantage)", nwmain::kSavingThrowRoll,
               nwmain::kSavingThrowRollPrologue, sizeof(nwmain::kSavingThrowRollPrologue),
               reinterpret_cast<void*>(&SavingThrowRoll), reinterpret_cast<void**>(&g_saving_throw_roll)});
  PatchCallSite("save roll d20 (species advantage, Halfling Luck)", nwmain::kSaveD20Call, nwmain::kRollDice,
                reinterpret_cast<void*>(&RollSaveD20));
  if (g_config.saves == rules::RuleVariant::kFaithful) {
    static const uint8_t kNops[2] = {0x90, 0x90};
    PatchBytes("save roll: natural 1 not automatic (faithful)", nwmain::kSaveNatural1Check,
               nwmain::kSaveNatural1CheckBytes, nwmain::kSaveNatural1CheckSkip, sizeof(nwmain::kSaveNatural1CheckSkip));
    PatchBytes("save roll: natural 20 not automatic (faithful)", nwmain::kSaveNatural20Check,
               nwmain::kSaveNatural20CheckBytes, kNops, sizeof(kNops));
    int dropped = 0;
    for (uintptr_t site : nwmain::kSaveRollFeatBonusCalls) {
      dropped += PatchCallSite("save roll feat bonus (C7)", site, nwmain::kHasFeat,
                               reinterpret_cast<void*>(&NoFeatBonus))
                     ? 1
                     : 0;
    }
    Log("save roll: %d of %u feat/racial bonus checks dropped (faithful)", dropped,
        static_cast<unsigned>(sizeof(nwmain::kSaveRollFeatBonusCalls) / sizeof(nwmain::kSaveRollFeatBonusCalls[0])));
  }
}
