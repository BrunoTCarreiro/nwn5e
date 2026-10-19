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
// - CN1–CN3 (`conditions`): stunned (both modes), paralysed and sleeping
//   (faithful) creatures fail Strength and Dexterity saves — the d20 comes up
//   1 and the result is a failure, told to the player; a restrained creature
//   (faithful) rolls Dexterity saves with disadvantage.

#include <stdint.h>
#include <string.h>

#include <optional>
#include <string>
#include <utility>

#include "condition_effects.h"
#include "config.h"
#include "creature.h"
#include "engine_queries.h"
#include "hooks.h"
#include "log.h"
#include "log_once.h"
#include "nwmain_1_69.h"
#include "rules/core.h"
#include "rules/d20.h"
#include "rules/nwn_conversion.h"
#include "rules/saves.h"

void TellPlayer(uint32_t object_id, const std::string& line);  // rules_chat.cpp

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
  int spell = -1;  // the spell whose script asked for this save, or -1
  std::optional<rules::Ability> ability;  // C1: the 2024 ability that spell uses
  ConditionSave condition;  // CN1–CN3
};

// The spell of the script running the current FortitudeSave / ReflexSave /
// WillSave / GetReflexAdjustedDamage command (-1 outside one).
int g_script_spell = -1;

std::string SpellNote(int spell);

using VmCommandFn = int(__fastcall*)(void* commands, void* edx, int command, int parameters);
using GetObjectArrayFn = void*(__fastcall*)(void* app, void* edx);
using GetGameObjectFn = uint8_t(__fastcall*)(void* array, void* edx, uint32_t id, void** object);
using VirtualCastFn = void*(__fastcall*)(void* object, void* edx);
VmCommandFn g_vm_save_command = nullptr;
VmCommandFn g_vm_reflex_damage_command = nullptr;
const auto g_get_object_array = reinterpret_cast<GetObjectArrayFn>(nwmain::kGetObjectArray);
const auto g_get_game_object = reinterpret_cast<GetGameObjectFn>(nwmain::kGetGameObject);

void* VirtualCast(void* object, uintptr_t slot) {
  void** vtable = *reinterpret_cast<void***>(object);
  return reinterpret_cast<VirtualCastFn>(vtable[slot / sizeof(void*)])(object, nullptr);
}

// The spell id of an object, as GetSpellId reads it (an area of effect keeps
// its own); -1 when unknown.
int SpellOfObject(uint32_t id) {
  void* app = engine::ServerExoApp();
  void* array = app ? g_get_object_array(app, nullptr) : nullptr;
  void* object = nullptr;
  if (!array || g_get_game_object(array, nullptr, id, &object) != 0 || !object) return -1;
  if (void* aoe = VirtualCast(object, nwmain::kVtableAsAreaOfEffect)) {
    return Field<int32_t>(aoe, nwmain::kAreaOfEffectSpellId);
  }
  void* nws_object = VirtualCast(object, nwmain::kVtableAsNwsObject);
  return nws_object ? Field<int32_t>(nws_object, nwmain::kObjectLastSpellId) : -1;
}

// Runs a script command with g_script_spell set to the running script's spell.
int WithScriptSpell(VmCommandFn original, void* commands, void* edx, int command, int parameters) {
  const int outer = g_script_spell;
  g_script_spell = SpellOfObject(Field<uint32_t>(commands, nwmain::kVmCommandsObjectSelf));
  const int result = original(commands, edx, command, parameters);
  g_script_spell = outer;
  return result;
}

int __fastcall VmSaveCommand(void* commands, void* edx, int command, int parameters) {
  return WithScriptSpell(g_vm_save_command, commands, edx, command, parameters);
}

int __fastcall VmReflexDamageCommand(void* commands, void* edx, int command, int parameters) {
  return WithScriptSpell(g_vm_reflex_damage_command, commands, edx, command, parameters);
}
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
  // A save asked for by a spell's script uses that spell's DC (GetSpellSaveDC,
  // already 2024 — persistent areas of effect pass themselves as the
  // opponent, so the caster check alone misses them).
  const bool spell_dc = g_script_spell >= 0 || IsSpellDc(versus, engine_dc);
  const int new_dc = spell_dc ? engine_dc : rules::ConvertNwnFixedDc(engine_dc);
  const SaveContext outer = g_save;
  g_save = SaveContext();
  g_save.active = true;
  g_save.creature = creature;
  g_save.save = static_cast<int>(save & 0xFF);
  g_save.type = static_cast<int>(type & 0xFF);
  g_save.spell = g_script_spell;
  if (g_config.saves == rules::RuleVariant::kFaithful && g_script_spell >= 0) {
    g_save.ability = rules::SaveAbilityForNwnSpell(g_script_spell);
  }
  if (g_save.save >= 1 && g_save.save <= 3) {
    const rules::Ability ability =
        g_save.ability.value_or(rules::AbilityForNwnSave(static_cast<rules::NwnSave>(g_save.save - 1)));
    g_save.condition = ConditionsForSave(creature, ability);
  }
  const ConditionSave condition = g_save.condition;
  uint32_t result =
      g_saving_throw_roll(creature, edx, save, (dc & 0xFFFF0000u) | static_cast<uint32_t>(new_dc), type, versus, a5,
                          a6, a7);
  g_save = outer;
  const uint32_t creature_id = Field<uint32_t>(creature, nwmain::kCreatureObjectId);
  if (condition.auto_fail && (result & 0xFF) != 2) {
    result &= ~0xFFu;  // a failure (0); the engine's own line may still read "success" when its total beat the DC
    TellPlayer(creature_id, "nwn5e: that save fails automatically (" + condition.note + ")");
  }
  if (g_config.log_attacks && g_save_log_lines < 300) {
    ++g_save_log_lines;
    Log("save roll: object 0x%08X save %d type %d vs 0x%08X: DC %d%s -> %d%s, %s%s%s", creature_id, save & 0xFF,
        type & 0xFF, versus, engine_dc, spell_dc ? " (spell)" : "", new_dc, SpellNote(g_script_spell).c_str(),
        (result & 0xFF) == 1 ? "success" : ((result & 0xFF) == 2 ? "immune" : "failure"),
        condition.auto_fail ? ", automatic failure: " : "", condition.auto_fail ? condition.note.c_str() : "");
  }
  return result;
}

std::string SpellNote(int spell) {
  if (spell < 0) return std::string();
  const std::optional<rules::Ability> ability =
      g_config.saves == rules::RuleVariant::kFaithful ? rules::SaveAbilityForNwnSpell(spell) : std::nullopt;
  static const char* const kNames[] = {"Strength", "Dexterity", "Constitution", "Intelligence", "Wisdom", "Charisma"};
  return ", spell " + std::to_string(spell) +
         (ability ? std::string(" uses ") + kNames[static_cast<int>(*ability)] : std::string());
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
  if (g_save.condition.auto_fail) return 1;  // CN2/CN3: the wrapper makes it a failure
  void* stats = Field<void*>(g_save.creature, nwmain::kCreatureStats);
  int kept = g_roll_dice(rules_object, edx, count, sides) & 0xFFFF;
  const bool species = stats && SpeciesAdvantage(stats, g_save.save, g_save.type);
  const rules::RollMode mode = rules::ResolveRollMode(species ? 1 : 0, g_save.condition.disadvantage);
  const bool advantage = mode == rules::RollMode::kAdvantage;
  const bool disadvantage = mode == rules::RollMode::kDisadvantage;
  int second = 0;
  if (advantage || disadvantage) {
    second = g_roll_dice(rules_object, edx, count, sides) & 0xFFFF;
    if (advantage ? second > kept : second < kept) std::swap(kept, second);
  }
  bool lucky = false;
  if (kept == 1 && stats && Field<uint16_t>(stats, nwmain::kStatsRace) == nwmain::kRaceHalfling) {
    kept = g_roll_dice(rules_object, edx, count, sides) & 0xFFFF;  // Halfling Luck: reroll a natural 1
    lucky = true;
  }
  if ((species || disadvantage || lucky) && g_config.log_attacks && g_save_log_lines < 300) {
    ++g_save_log_lines;
    Log("save d20: object 0x%08X save %d type %d:%s%s%s%s -> %d",
        Field<uint32_t>(g_save.creature, nwmain::kCreatureObjectId), g_save.save, g_save.type,
        species ? " species advantage" : "", g_save.condition.disadvantage > 0 ? " disadvantage (" : "",
        g_save.condition.disadvantage > 0 ? (g_save.condition.note + ")").c_str() : "",
        lucky ? " Halfling Luck reroll" : "", kept);
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

// C1 + C8: the save kind the save-roll feedback sends — NWN's 1/2/3, or the
// kind of the spell's own ability when it replaced the save's (faithful).
int __stdcall SentSaveKind(int kind) {
  return g_save.active && g_save.ability ? rules::FeedbackSaveKind(*g_save.ability) : kind;
}

// Called from 0x749BB8 in place of `mov edx,[esp+0x24]; push edx` (+4 on esp
// for the return address); leaves the kind pushed as that code did.
__declspec(naked) void FeedbackSaveKindStub() {
  __asm {
    mov edx, dword ptr [esp + 0x28]
    push edx
    call SentSaveKind
    pop edx
    push eax
    jmp edx
  }
}

// Jumped to from the client's save-kind switch (0x45E9ED): kinds 0x11-0x16
// push their private string id the way the engine's cases push 520/519/745 and
// go to its GetSimpleString call (0x45EA42), where the label hook names it;
// every other kind goes on to the engine's cases as before.
__declspec(naked) void ClientSaveKindStub() {
  __asm {
    mov al, byte ptr [esp + 0x4F]
    cmp al, 0x11
    jb engine_kinds
    cmp al, 0x16
    ja engine_kinds
    movzx eax, al
    add eax, 0x00FDFFEF
    xor ecx, ecx
    mov cl, byte ptr ds:[0x008B15AC]
    lea edx, [esp + 0x3E0]
    push ecx
    push edx
    push eax
    push 0x0045EA42
    ret
  engine_kinds:
    cmp al, 1
    jne other_kinds
    push 0x0045E9F5
    ret
  other_kinds:
    push 0x0045EA0C
    ret
  }
}
static_assert(rules::kFeedbackSaveKindFirst == 0x11 && rules::kFeedbackSaveKindFirst + 5 == 0x16 &&
                  rules::kSaveLabelStrrefFirst - rules::kFeedbackSaveKindFirst == 0x00FDFFEF,
              "ClientSaveKindStub's literals");

// The combat log names a save on a spell's own ability by that ability. The
// client switch is patched first, so the server never sends a kind the client
// cannot name.
void InstallSaveFeedbackLabel() {
  uint8_t jump[8] = {0xe9, 0, 0, 0, 0, 0x90, 0x90, 0x90};  // jmp rel32 (as long as a call) + 3 nops
  const int32_t jump_rel = static_cast<int32_t>(reinterpret_cast<uintptr_t>(&ClientSaveKindStub) -
                                                (nwmain::kClientSaveKindSwitch + nwmain::kCallLength));
  memcpy(jump + 1, &jump_rel, sizeof(jump_rel));
  if (!PatchBytes("combat log save label: client names the spell's ability (C1)", nwmain::kClientSaveKindSwitch,
                  nwmain::kClientSaveKindSwitchBytes, jump, sizeof(jump))) {
    return;
  }
  uint8_t call[5] = {0xe8};
  const int32_t call_rel = static_cast<int32_t>(reinterpret_cast<uintptr_t>(&FeedbackSaveKindStub) -
                                                (nwmain::kSaveFeedbackKindPush + nwmain::kCallLength));
  memcpy(call + 1, &call_rel, sizeof(call_rel));
  PatchBytes("combat log save label: server sends the spell's ability (C1)", nwmain::kSaveFeedbackKindPush,
             nwmain::kSaveFeedbackKindPushBytes, call, sizeof(call));
}

}  // namespace

// A DC that is already on the 2024 scale (D4 Topple), so the save roll does not
// convert it: remembered like a spell DC for `caster`.
void RememberSaveDc2024(uint32_t caster, int dc) { RememberSpellDc(caster, dc); }

// C1: the ability the save being rolled for `stats` uses, when its spell asks
// for another one than the NWN save's default.
std::optional<rules::Ability> SaveAbilityOverride(void* stats) {
  if (!g_save.active || !g_save.ability || !g_save.creature) return std::nullopt;
  return Field<void*>(g_save.creature, nwmain::kCreatureStats) == stats ? g_save.ability : std::nullopt;
}

void InstallSaveRollRules() {
  if (g_config.saves == rules::RuleVariant::kOff) return;
  InstallHook({"CNWSCreature::CalculateSpellSaveDC (2024 spell DC)", nwmain::kCalculateSpellSaveDc,
               nwmain::kCalculateSpellSaveDcPrologue, sizeof(nwmain::kCalculateSpellSaveDcPrologue),
               reinterpret_cast<void*>(&CalculateSpellSaveDc), reinterpret_cast<void**>(&g_calculate_spell_save_dc)});
  InstallHook({"script save commands (the spell behind a save)", nwmain::kVmSaveCommand,
               nwmain::kVmSaveCommandPrologue, sizeof(nwmain::kVmSaveCommandPrologue),
               reinterpret_cast<void*>(&VmSaveCommand), reinterpret_cast<void**>(&g_vm_save_command)});
  InstallHook({"GetReflexAdjustedDamage command (the spell behind its save)", nwmain::kVmReflexDamageCommand,
               nwmain::kVmReflexDamageCommandPrologue, sizeof(nwmain::kVmReflexDamageCommandPrologue),
               reinterpret_cast<void*>(&VmReflexDamageCommand), reinterpret_cast<void**>(&g_vm_reflex_damage_command)});
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
    InstallSaveFeedbackLabel();
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

// SK10 (rules_spell_attack.cpp): the casting part of a creature's 2024 spell
// attack bonus for the spell it is casting, read as CalculateSpellSaveDc reads
// it: PB + the casting modifier of the class slot it casts from (Charisma for
// innate abilities), or a scroll's attack bonus when it casts from an item.
bool SpellAttackCasting(void* creature, int* bonus, const char** source) {
  void* stats = creature ? Field<void*>(creature, nwmain::kCreatureStats) : nullptr;
  if (!stats) return false;
  const CreatureFacts facts = ReadCreatureFacts(stats);
  if (!facts.plausible) return false;
  const int pb = rules::ProficiencyBonusForCreature(CountsAsCharacter(stats, facts), facts.level,
                                                    facts.challenge_rating);
  const int slot = Field<uint8_t>(creature, nwmain::kCreatureCastingSlot);
  if (Field<int32_t>(creature, nwmain::kCreatureCastingFromItem) != 0) {
    void* rules_object = *reinterpret_cast<void**>(nwmain::kRulesObject);
    void* spells = rules_object ? Field<void*>(rules_object, nwmain::kRulesSpells) : nullptr;
    const int spell_id = SpellOfObject(Field<uint32_t>(creature, nwmain::kCreatureObjectId));
    const void* spell = spells && spell_id >= 0 ? g_get_spell(spells, nullptr, spell_id) : nullptr;
    if (!spell) return false;
    *bonus = rules::SpellScrollAttackBonus(Field<uint8_t>(spell, nwmain::kSpellInnateLevel));
    *source = "item";
  } else if (slot == 0xFF || slot == 0xFE) {
    *bonus = rules::SpellAttackBonus2024(pb, Field<int8_t>(stats, nwmain::kStatsChaModifier));
    *source = "ability: PB + Cha";
  } else {
    *bonus = rules::SpellAttackBonus2024(pb, static_cast<int8_t>(g_get_casting_modifier(stats, nullptr, slot) & 0xFF));
    *source = "class: PB + casting ability";
  }
  return true;
}
