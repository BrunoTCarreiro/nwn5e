// Phase 1c (PLAN.md C1–C3): the three NWN save values become 2024 saves.
// Fortitude, Reflex and Will are read as the Constitution, Dexterity and
// Wisdom saves: ability modifier + proficiency bonus when proficient + magic
// effects (+ Aura of Protection in faithful mode, + NWN's flat feat/racial and
// blueprint bonuses in NWN mode) — rules::SaveValue. The character sheet and
// the save roll read these functions, so both follow. `saves=off` keeps NWN's
// 3e values.

#include <stdint.h>

#include <algorithm>
#include <optional>
#include <stdio.h>
#include <string>

#include "config.h"
#include "creature.h"
#include "engine_queries.h"
#include "hooks.h"
#include "log.h"
#include "log_once.h"
#include "nwmain_1_69.h"
#include "rules/nwn_conversion.h"
#include "rules/saves.h"

namespace {

using SaveFn = int(__fastcall*)(void* stats, void* edx, int base_only);
using HasFeatFn = int(__fastcall*)(void* stats, void* edx, int feat);
using DexModFn = int(__fastcall*)(void* stats, void* edx, int armor_dex_cap);
using ClassSaveBonusFn = int(__fastcall*)(void* character_class, void* edx, int level);

SaveFn g_get_fort = nullptr;
SaveFn g_get_reflex = nullptr;
SaveFn g_get_will = nullptr;
const auto g_has_feat = reinterpret_cast<HasFeatFn>(nwmain::kHasFeat);
const auto g_get_dex_mod = reinterpret_cast<DexModFn>(nwmain::kGetDexMod);

LogOnce g_save_logged(2000);

bool HasFeat(void* stats, int feat) { return g_has_feat(stats, nullptr, feat) != 0; }

int Int8(const void* stats, uintptr_t offset) { return Field<int8_t>(stats, offset); }

int AbilityModifier(void* stats, rules::NwnSave save) {
  switch (save) {
    case rules::NwnSave::kFortitude:
      return Int8(stats, nwmain::kStatsConModifier);
    case rules::NwnSave::kReflex:
      return static_cast<int8_t>(g_get_dex_mod(stats, nullptr, 0) & 0xFF);
    case rules::NwnSave::kWill:
      return Int8(stats, nwmain::kStatsWisModifier);
  }
  return 0;
}

int SaveFeat(rules::NwnSave save) {
  switch (save) {
    case rules::NwnSave::kFortitude:
      return nwmain::kFeatGreatFortitude;
    case rules::NwnSave::kReflex:
      return nwmain::kFeatLightningReflexes;
    default:
      return nwmain::kFeatIronWill;
  }
}

// NWN mode: NWN's flat feat, racial and blueprint bonuses for this save, as
// the engine's own save functions add them (nwmain_1_69.h).
int NwnExtras(void* stats, rules::NwnSave save) {
  const int charisma = Int8(stats, nwmain::kStatsChaModifier);
  int extras = 0;
  if (HasFeat(stats, nwmain::kFeatDivineGrace) && charisma > 0) extras += charisma;
  if (HasFeat(stats, SaveFeat(save))) extras += 2;
  if (HasFeat(stats, nwmain::kFeatLucky)) extras += 1;
  if (HasFeat(stats, nwmain::kFeatLuckOfHeroes)) extras += 1;
  if (HasFeat(stats, nwmain::kFeatDarkBlessing)) extras += charisma;
  switch (save) {
    case rules::NwnSave::kFortitude:
      if (HasFeat(stats, 388)) extras += 1;
      if (HasFeat(stats, 583)) extras += 4;
      extras += Int8(stats, nwmain::kStatsFortBlueprintBonus);
      break;
    case rules::NwnSave::kReflex:
      if (HasFeat(stats, 386)) extras += 1;
      if (HasFeat(stats, 585)) extras += 4;
      extras += Int8(stats, nwmain::kStatsReflexBlueprintBonus);
      break;
    case rules::NwnSave::kWill:
      if (HasFeat(stats, 380)) extras += 1;
      if (HasFeat(stats, 388)) extras += 1;
      if (HasFeat(stats, 695)) extras += 4;
      extras += Int8(stats, nwmain::kStatsWillBlueprintBonus);
      break;
  }
  return extras;
}

// C2 NWN mode: any of the creature's classes has a "good" table for this save.
bool HasGoodNwnTable(const std::vector<rules::NwnClassSlot>& slots, rules::NwnSave save) {
  void* rules_object = *reinterpret_cast<void**>(nwmain::kRulesObject);
  if (!rules_object) return false;
  uint8_t* classes = Field<uint8_t*>(rules_object, nwmain::kRulesClasses);
  if (!classes) return false;
  const uintptr_t function = save == rules::NwnSave::kFortitude ? nwmain::kGetFortSaveBonus
                             : save == rules::NwnSave::kReflex  ? nwmain::kGetReflexSaveBonus
                                                                : nwmain::kGetWillSaveBonus;
  const auto save_bonus = reinterpret_cast<ClassSaveBonusFn>(function);
  for (const rules::NwnClassSlot& slot : slots) {
    if (slot.nwn_class_id < 0 || slot.nwn_class_id > 255) continue;
    void* character_class = classes + slot.nwn_class_id * nwmain::kClassStride;
    if (rules::IsGoodNwnSaveTable(static_cast<int8_t>(save_bonus(character_class, nullptr, 1) & 0xFF))) return true;
  }
  return false;
}

int PaladinLevels(void* stats) {
  int levels = 0;
  for (const rules::NwnClassSlot& slot : ReadClassSlots(stats)) {
    if (rules::ClassFromNwnId(slot.nwn_class_id) == rules::CharacterClass::kPaladin) levels += slot.levels;
  }
  return levels;
}

bool Conscious(void* creature) {
  const uint8_t state = Field<uint8_t>(creature, nwmain::kCreatureActionState);
  return !engine::IsDeadOrDying(creature) && state != 5 && state != 6;
}

int AuraFrom(void* paladin, float distance_m) {
  void* stats = Field<void*>(paladin, nwmain::kCreatureStats);
  if (!stats) return 0;
  return rules::AuraOfProtectionBonus(PaladinLevels(stats), Int8(stats, nwmain::kStatsChaModifier), Conscious(paladin),
                                      distance_m);
}

// C3 faithful: the best Aura of Protection covering this creature (its own,
// or an ally paladin's within 3 m). Auras don't stack.
int AuraOfProtection(void* stats) {
  void* creature = Field<void*>(stats, nwmain::kStatsCreature);
  if (!creature) return 0;
  int best = AuraFrom(creature, 0.0f);
  const int faction = engine::FactionOf(creature);
  engine::ForEachOtherCreatureInArea(creature, [&](uint32_t, void* other) {
    if (faction < 0 || engine::FactionOf(other) != faction) return;
    best = std::max(best, AuraFrom(other, std::max(0.0f, engine::EdgeDistance(creature, other))));
  });
  return best;
}

bool SummonLike(int associate_type) { return associate_type >= 2 && associate_type <= 5; }

const char* SaveName(rules::NwnSave save) {
  return save == rules::NwnSave::kFortitude ? "Fortitude (Con)"
         : save == rules::NwnSave::kReflex  ? "Reflex (Dex)"
                                            : "Will (Wis)";
}

int ConvertSave(rules::NwnSave save, SaveFn original, void* stats, void* edx, int base_only) {
  const int engine_value = original(stats, edx, base_only);
  const rules::RuleVariant variant = g_config.saves;
  if (variant == rules::RuleVariant::kOff) return engine_value;
  const CreatureFacts facts = ReadCreatureFacts(stats);
  if (!facts.plausible) return engine_value;
  const std::vector<rules::NwnClassSlot> slots = ReadClassSlots(stats);
  rules::SaveFacts save_facts;
  save_facts.save = save;
  save_facts.ability_modifier = AbilityModifier(stats, save);
  save_facts.first_class = slots.empty() ? std::nullopt : rules::ClassFromNwnId(slots[0].nwn_class_id);
  save_facts.character = facts.character || (save_facts.first_class && !SummonLike(facts.associate_type));
  save_facts.proficiency_bonus =
      rules::ProficiencyBonusForCreature(save_facts.character, facts.level, facts.challenge_rating);
  save_facts.save_feat = HasFeat(stats, SaveFeat(save));
  save_facts.good_nwn_table = !save_facts.character && HasGoodNwnTable(slots, save);
  save_facts.effects = base_only ? 0 : engine_value - original(stats, edx, 1);
  if (variant == rules::RuleVariant::kFaithful && !base_only) save_facts.aura = AuraOfProtection(stats);
  if (variant == rules::RuleVariant::kNwn) save_facts.nwn_extras = NwnExtras(stats, save);
  const int value = rules::SaveValue(save_facts, variant);
  if (g_config.log_attacks &&
      g_save_logged.First(LogOnce::Key(ObjectIdOf(stats) ^ (static_cast<uint32_t>(save) << 28), engine_value, value))) {
    Log("save: object 0x%08X %s %s%s: 3e %+d -> 5e %+d (ability %+d, %s, effects %+d%s)", ObjectIdOf(stats),
        KindOf(facts), SaveName(save), base_only ? " base" : "", engine_value, value, save_facts.ability_modifier,
        save_facts.character ? "character" : "creature", save_facts.effects,
        save_facts.aura ? ", aura" : (save_facts.nwn_extras ? ", NWN extras" : ""));
  }
  return value;
}

int __fastcall GetFortSavingThrow(void* stats, void* edx, int base_only) {
  return ConvertSave(rules::NwnSave::kFortitude, g_get_fort, stats, edx, base_only);
}
int __fastcall GetReflexSavingThrow(void* stats, void* edx, int base_only) {
  return ConvertSave(rules::NwnSave::kReflex, g_get_reflex, stats, edx, base_only);
}
int __fastcall GetWillSavingThrow(void* stats, void* edx, int base_only) {
  return ConvertSave(rules::NwnSave::kWill, g_get_will, stats, edx, base_only);
}

}  // namespace

// C8: all six 2024 saves for `!saves` (rules_chat.cpp). Con / Dex / Wis are
// the hooked Fortitude / Reflex / Will values; Str / Int / Cha (no NWN save
// asks for them yet) are ability modifier + proficiency bonus when proficient
// (+ Aura of Protection in faithful mode).
std::string DescribeSaves(void* stats) {
  if (g_config.saves == rules::RuleVariant::kOff) return "nwn5e: 2024 saves are off (saves=off in nwn5e.ini).";
  const CreatureFacts facts = ReadCreatureFacts(stats);
  const std::vector<rules::NwnClassSlot> slots = ReadClassSlots(stats);
  const std::optional<rules::CharacterClass> first =
      slots.empty() ? std::nullopt : rules::ClassFromNwnId(slots[0].nwn_class_id);
  const bool character = facts.character || (first && !SummonLike(facts.associate_type));
  const int pb = rules::ProficiencyBonusForCreature(character, facts.level, facts.challenge_rating);
  const bool faithful = g_config.saves == rules::RuleVariant::kFaithful;
  const int aura = faithful ? AuraOfProtection(stats) : 0;
  const auto class_proficient = [&](rules::Ability ability) {
    return character && first && rules::HasSaveProficiency(*first, ability);
  };
  struct Row {
    const char* name;
    int value;
    bool proficient;
  };
  const auto engine_save = [&](uintptr_t address, rules::NwnSave save, rules::Ability ability) {
    const int value = reinterpret_cast<SaveFn>(address)(stats, nullptr, 0);
    const bool feat = faithful && character && HasFeat(stats, SaveFeat(save));
    const bool good = !faithful && !character && HasGoodNwnTable(slots, save);
    return Row{"", value, class_proficient(ability) || feat || good};
  };
  const auto plain_save = [&](uintptr_t offset, rules::Ability ability) {
    const bool proficient = class_proficient(ability);
    return Row{"", rules::SavingThrowBonus(Int8(stats, offset), proficient, pb, aura), proficient};
  };
  Row rows[6] = {plain_save(nwmain::kStatsStrModifier, rules::Ability::kStrength),
                 engine_save(nwmain::kGetReflexSavingThrow, rules::NwnSave::kReflex, rules::Ability::kDexterity),
                 engine_save(nwmain::kGetFortSavingThrow, rules::NwnSave::kFortitude, rules::Ability::kConstitution),
                 plain_save(nwmain::kStatsIntModifier, rules::Ability::kIntelligence),
                 engine_save(nwmain::kGetWillSavingThrow, rules::NwnSave::kWill, rules::Ability::kWisdom),
                 plain_save(nwmain::kStatsChaModifier, rules::Ability::kCharisma)};
  static const char* const kNames[6] = {"Str", "Dex", "Con", "Int", "Wis", "Cha"};
  std::string text = faithful ? "Saves (2024): " : "Saves (2024, NWN mode): ";
  char part[32];
  for (int i = 0; i < 6; ++i) {
    snprintf(part, sizeof(part), "%s%s %+d%s", i ? ", " : "", kNames[i], rows[i].value, rows[i].proficient ? "*" : "");
    text += part;
  }
  text += "   (* proficient)";
  return text;
}

void InstallSaveRules() {
  if (g_config.saves == rules::RuleVariant::kOff) return;
  InstallHook({"CNWSCreatureStats::GetFortSavingThrow", nwmain::kGetFortSavingThrow,
               nwmain::kGetFortSavingThrowPrologue, sizeof(nwmain::kGetFortSavingThrowPrologue),
               reinterpret_cast<void*>(&GetFortSavingThrow), reinterpret_cast<void**>(&g_get_fort)});
  InstallHook({"CNWSCreatureStats::GetReflexSavingThrow", nwmain::kGetReflexSavingThrow,
               nwmain::kGetReflexSavingThrowPrologue, sizeof(nwmain::kGetReflexSavingThrowPrologue),
               reinterpret_cast<void*>(&GetReflexSavingThrow), reinterpret_cast<void**>(&g_get_reflex)});
  InstallHook({"CNWSCreatureStats::GetWillSavingThrow", nwmain::kGetWillSavingThrow,
               nwmain::kGetWillSavingThrowPrologue, sizeof(nwmain::kGetWillSavingThrowPrologue),
               reinterpret_cast<void*>(&GetWillSavingThrow), reinterpret_cast<void**>(&g_get_will)});
}
