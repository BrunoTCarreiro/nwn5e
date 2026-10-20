// Phase 1b (PLAN.md, decided 2026-10-18): 2024 advantage/disadvantage
// replace NWN's situational attack modifiers.
//
// ResolveAttackRoll rolls its d20 before it asks GetAttackModifierVersus for
// the modifier, so its hook works out the situation first: which of the
// engine's situational terms apply (the engine's own checks, nwmain_1_69.h)
// and the 2024 close-combat check (B5: any enemy within 5 ft that sees the
// attacker and is not incapacitated; Point Blank Shot cancels it, B3). The d20
// call is redirected to RollAttackD20, which rolls one or two dice with the
// engine's RollDice and returns the kept one; the GetAttackModifierVersus hook
// (rules_attack.cpp) takes the 3e total back out (PendingSituationalTotal).
//
// The ResolveAttackRoll hook also carries the [debug] attack_trace line.

#include <stdint.h>
#include <string.h>

#include <string>

#include "condition_effects.h"
#include "config.h"
#include "creature.h"
#include "engine_queries.h"
#include "hooks.h"
#include "log.h"
#include "mastery.h"
#include "nwmain_1_69.h"
#include "rules/nwn_situational.h"

void TraceAttackRoll(const void* creature);  // rules_attack_trace.cpp
void NotePotionResumeTarget(void* attacker, uint32_t target_id);  // rules_potion.cpp
bool ShoveInsteadOfAttackRoll(void* creature);                      // rules_shove.cpp
void KeepCombatBreakdownOn();                 // rules_attack_trace.cpp

namespace {

using ResolveAttackRollFn = void(__fastcall*)(void* creature, void* edx, void* target);
using RollDiceFn = int(__fastcall*)(void* rules_object, void* edx, int count, int sides);
using GetAttackFn = void*(__fastcall*)(void* round, void* edx, int index);
using HasFeatFn = int(__fastcall*)(void* stats, void* edx, int feat);
using CreatureCheckFn = int(__fastcall*)(void* creature, void* edx);
using GetInvisibleFn = int(__fastcall*)(void* creature, void* edx, void* object, int flag);
using IsFlankedByFn = int(__fastcall*)(void* attacker, void* edx, void* target);
using AsObjectFn = void*(__fastcall*)(void* object, void* edx);

ResolveAttackRollFn g_resolve_attack_roll = nullptr;
const auto g_roll_dice = reinterpret_cast<RollDiceFn>(nwmain::kRollDice);
const auto g_get_attack = reinterpret_cast<GetAttackFn>(nwmain::kGetAttack);
const auto g_has_feat = reinterpret_cast<HasFeatFn>(nwmain::kHasFeat);
const auto g_get_blind = reinterpret_cast<CreatureCheckFn>(nwmain::kGetBlind);
const auto g_get_invisible = reinterpret_cast<GetInvisibleFn>(nwmain::kGetInvisible);
const auto g_is_flanked_by = reinterpret_cast<IsFlankedByFn>(nwmain::kIsFlankedBy);
const auto g_wields_ranged_weapon = reinterpret_cast<CreatureCheckFn>(nwmain::kWieldsRangedWeapon);

constexpr float kFiveFeet = 1.524f;  // metres, edge to edge
constexpr int kLogLimit = 400;
int g_log_lines = 0;

// The attack ResolveAttackRoll is resolving right now.
struct PendingAttack {
  bool active = false;
  const void* attacker_stats = nullptr;
  const void* target = nullptr;  // target creature
  rules::NwnSituationalTerms terms;
  int nwn_total = 0;
  int threats = 0;
  bool point_blank_shot = false;
  rules::RollMode mode = rules::RollMode::kNormal;
  uint32_t attacker_id = 0;
  uint32_t target_id = 0;
  std::string mastery;  // D4 Vex / Sap note for the log
  std::string conditions;  // CN1–CN6 note for the log
};
PendingAttack g_pending;

bool HasFeat(const void* creature, int feat) {
  void* stats = Field<void*>(creature, nwmain::kCreatureStats);
  return stats && g_has_feat(stats, nullptr, feat) != 0;
}

bool IsProne(int animation) {
  return animation == 0x05 || animation == 0x07 || animation == 0x20 || animation == 0x21 || animation == 0x24;
}

bool IsMoving(int animation) { return animation == 0x04 || animation == 0x56 || animation == 0x57; }

// The situational terms GetAttackModifierVersus will add for this attack,
// with the engine's own checks in the engine's order (nwmain_1_69.h).
rules::NwnSituationalTerms ReadNwnTerms(void* attacker, void* target, bool ranged) {
  rules::NwnSituationalTerms terms;
  terms.ranged = ranged;
  const bool blind_fight_applies = !ranged && HasFeat(target, nwmain::kFeatBlindFight);
  terms.target_blind = g_get_blind(target, nullptr) != 0 && !blind_fight_applies;
  void* attacker_object = (*reinterpret_cast<AsObjectFn* const*>(attacker))[nwmain::kVtableAsObject / 4](attacker,
                                                                                                          nullptr);
  terms.attacker_invisible = g_get_invisible(attacker, nullptr, attacker_object, 0) != 0 && !blind_fight_applies;
  terms.cant_see_target = !engine::Sees(attacker, Field<uint32_t>(target, nwmain::kCreatureObjectId));
  const int animation = Field<int32_t>(target, nwmain::kCreatureAnimation);
  terms.target_moving = ranged && IsMoving(animation);
  terms.target_prone = IsProne(animation);
  terms.target_stunned = Field<uint8_t>(target, nwmain::kCreatureActionState) == 6;
  const float distance_squared = engine::CentreDistanceSquared(attacker, target);
  terms.target_flanked = g_is_flanked_by(attacker, nullptr, target) == 1 && (!ranged || distance_squared <= 100.0f);
  if (ranged && !HasFeat(attacker, nwmain::kFeatPointBlankShot)) {
    const float reach = engine::MaxAttackRange(target, Field<uint32_t>(attacker, nwmain::kCreatureObjectId), 0);
    terms.ranged_in_melee = distance_squared <= reach * reach && g_wields_ranged_weapon(target, nullptr) == 0;
  }
  return terms;
}

bool IsIncapacitated(void* creature) {
  return engine::IsDeadOrDying(creature) || IncapacitatedByCondition(creature);
}

// B5: enemies within 5 ft of the attacker that see it and can act.
int CountCloseCombatThreats(void* attacker) {
  const uint32_t self = Field<uint32_t>(attacker, nwmain::kCreatureObjectId);
  int threats = 0;
  engine::ForEachOtherCreatureInArea(attacker, [&](uint32_t id, void* creature) {
    if (!engine::IsEnemy(attacker, id) || IsIncapacitated(creature)) return;
    if (engine::EdgeDistance(attacker, creature) > kFiveFeet || !engine::Sees(creature, self)) return;
    ++threats;
  });
  return threats;
}

const char* ModeName(rules::RollMode mode) {
  switch (mode) {
    case rules::RollMode::kAdvantage:
      return "advantage";
    case rules::RollMode::kDisadvantage:
      return "disadvantage";
    default:
      return "normal";
  }
}

std::string TermNames(const rules::NwnSituationalTerms& t) {
  std::string names;
  const auto add = [&](bool on, const char* name) {
    if (!on) return;
    if (!names.empty()) names += ", ";
    names += name;
  };
  add(t.target_blind, "target blind");
  add(t.attacker_invisible, "attacker invisible");
  add(t.cant_see_target, "target unseen");
  add(t.target_moving, "target moving");
  add(t.target_prone, "target prone");
  add(t.target_stunned, "target stunned");
  add(t.target_flanked, "target flanked");
  add(t.ranged_in_melee, "3e ranged in reach");
  return names.empty() ? "none" : names;
}

bool LogSlotLeft() { return g_config.log_attacks && g_log_lines < kLogLimit && ++g_log_lines; }

void ReadPendingAttack(void* creature, void* target_object) {
  void* round = Field<void*>(creature, nwmain::kCreatureCombatRound);
  void* stats = Field<void*>(creature, nwmain::kCreatureStats);
  if (!round || !stats || !target_object) return;
  void* target = engine::GetCreature(Field<uint32_t>(target_object, nwmain::kCreatureObjectId));
  if (!target) return;  // doors and placeables: GetAttackModifierVersus adds no situational terms
  const void* attack = g_get_attack(round, nullptr, Field<uint8_t>(round, nwmain::kRoundCurrentAttack));
  if (!attack) return;
  const bool ranged = Field<int32_t>(attack, nwmain::kAttackDataRanged) != 0;
  g_pending.terms = ReadNwnTerms(creature, target, ranged);
  g_pending.nwn_total = rules::NwnSituationalTotal(g_pending.terms);
  g_pending.point_blank_shot = HasFeat(creature, nwmain::kFeatPointBlankShot);
  g_pending.threats = ranged ? CountCloseCombatThreats(creature) : 0;
  MasteryRollSources mastery = TakeMasteryRollSources(creature, Field<uint32_t>(target, nwmain::kCreatureObjectId));
  const MasteryRollSources studied = TakeStudiedAttacks(creature, Field<uint32_t>(target, nwmain::kCreatureObjectId));
  mastery.advantage += studied.advantage;  // Fighter F6
  if (!studied.note.empty()) mastery.note += mastery.note.empty() ? studied.note : ", " + studied.note;
  g_pending.mastery = mastery.note;
  const ConditionRollSources conditions = ConditionAttackRollSources(creature, target, ranged);
  g_pending.conditions = conditions.note;
  g_pending.mode = rules::SituationalRollMode(
      g_pending.terms, rules::RangedCloseCombat(ranged, g_pending.point_blank_shot, g_pending.threats),
      mastery.advantage + conditions.advantage, mastery.disadvantage + conditions.disadvantage);
  g_pending.attacker_stats = stats;
  g_pending.target = target;
  g_pending.attacker_id = Field<uint32_t>(creature, nwmain::kCreatureObjectId);
  g_pending.target_id = Field<uint32_t>(target, nwmain::kCreatureObjectId);
  g_pending.active = true;
}

// [debug] creature_hp_multiplier / creature_min_hp: the first time a character
// attacks a creature that is not a character, its current hit points are
// multiplied and raised to the minimum, once per creature (marked with a local
// variable, so a reloaded creature is boosted again), so weak monsters survive
// several hits in mastery tests.
void BoostTestTarget(void* attacker, void* target_object) {
  const int multiplier = g_config.debug_creature_hp_multiplier > 1 ? g_config.debug_creature_hp_multiplier : 1;
  const int minimum = g_config.debug_creature_min_hp;
  if ((multiplier <= 1 && minimum <= 0) || !attacker || !target_object) return;
  const uint32_t target_id = Field<uint32_t>(target_object, nwmain::kCreatureObjectId);
  void* target = engine::GetCreature(target_id);
  void* attacker_stats = Field<void*>(attacker, nwmain::kCreatureStats);
  void* target_stats = target ? Field<void*>(target, nwmain::kCreatureStats) : nullptr;
  if (!attacker_stats || !target_stats) return;
  if (!ReadCreatureFacts(attacker_stats).character || ReadCreatureFacts(target_stats).character) return;
  if (engine::GetLocalInt(target, "nwn5e_test_boosted")) return;
  engine::SetLocalInt(target, "nwn5e_test_boosted", 1);
  const int hit_points = Field<int16_t>(target, nwmain::kObjectCurrentHitPoints);
  if (hit_points <= 0) return;
  int new_hit_points = hit_points * multiplier;
  if (new_hit_points < minimum) new_hit_points = minimum;
  if (new_hit_points > 30000) new_hit_points = 30000;
  if (new_hit_points == hit_points) return;
  SetField<int16_t>(target, nwmain::kObjectCurrentHitPoints, static_cast<int16_t>(new_hit_points));
  Log("debug: creature 0x%08X hit points %d -> %d (creature_hp_multiplier=%d, creature_min_hp=%d)", target_id,
      hit_points, new_hit_points, multiplier, minimum);
}

void __fastcall ResolveAttackRoll(void* creature, void* edx, void* target) {
  if (ShoveInsteadOfAttackRoll(creature)) return;  // SK6: a Shove makes no attack roll
  if (g_config.debug_attack_trace) TraceAttackRoll(creature);
  KeepCombatBreakdownOn();
  BoostTestTarget(creature, target);
  NoteWeaponUsed(creature);
  if (target) NotePotionResumeTarget(creature, Field<uint32_t>(target, nwmain::kCreatureObjectId));
  const int weapon = MasteryAttackWeapon(creature);
  const PendingAttack outer = g_pending;
  g_pending = PendingAttack();
  if (g_config.advantage_disadvantage) ReadPendingAttack(creature, target);
  if (g_pending.active && g_pending.mode == rules::RollMode::kNormal &&
      (g_pending.nwn_total != 0 || !g_pending.mastery.empty() || !g_pending.conditions.empty()) && LogSlotLeft()) {
    Log("advantage: 0x%08X vs 0x%08X (%s): 3e situational %+d removed (%s)%s%s%s%s; normal roll",
        g_pending.attacker_id, g_pending.target_id, g_pending.terms.ranged ? "ranged" : "melee", g_pending.nwn_total,
        TermNames(g_pending.terms).c_str(), g_pending.mastery.empty() ? "" : "; mastery: ", g_pending.mastery.c_str(),
        g_pending.conditions.empty() ? "" : "; conditions: ", g_pending.conditions.c_str());
  }
  g_resolve_attack_roll(creature, edx, target);
  ConditionsAfterAttackRoll(creature, target);
  MasteryAfterAttackRoll(creature, target, weapon);
  StudiedAttacksAfterAttackRoll(creature, target);  // Fighter F6
  g_pending = outer;
}

// Replaces ResolveAttackRoll's RollDice(1, 20) call for the attack roll.
// C3 Halfling Luck (2024): a natural 1 on a d20 test is rerolled once.
int HalflingLuck(void* rules_object, void* edx, int count, int sides, int kept) {
  if (kept != 1 || g_config.saves == rules::RuleVariant::kOff || !g_pending.attacker_stats) return kept;
  if (Field<uint16_t>(g_pending.attacker_stats, nwmain::kStatsRace) != nwmain::kRaceHalfling) return kept;
  const int reroll = g_roll_dice(rules_object, edx, count, sides) & 0xFFFF;
  if (LogSlotLeft()) Log("advantage: 0x%08X Halfling Luck: natural 1 rerolled -> %d", g_pending.attacker_id, reroll);
  return reroll;
}

int __fastcall RollAttackD20(void* rules_object, void* edx, int count, int sides) {
  if (!g_pending.active) return g_roll_dice(rules_object, edx, count, sides);
  if (g_pending.mode == rules::RollMode::kNormal) {
    return HalflingLuck(rules_object, edx, count, sides, g_roll_dice(rules_object, edx, count, sides) & 0xFFFF);
  }
  const int first = g_roll_dice(rules_object, edx, count, sides) & 0xFFFF;
  const int second = g_roll_dice(rules_object, edx, count, sides) & 0xFFFF;
  const bool keep_higher = g_pending.mode == rules::RollMode::kAdvantage;
  const int kept = keep_higher ? (first > second ? first : second) : (first < second ? first : second);
  if (LogSlotLeft()) {
    Log("advantage: 0x%08X vs 0x%08X (%s): %s; 3e situational %+d removed (%s); close-combat enemies %d%s%s%s%s%s; "
        "d20 %d / %d -> %d",
        g_pending.attacker_id, g_pending.target_id, g_pending.terms.ranged ? "ranged" : "melee",
        ModeName(g_pending.mode), g_pending.nwn_total, TermNames(g_pending.terms).c_str(), g_pending.threats,
        g_pending.point_blank_shot ? " (Point Blank Shot)" : "", g_pending.mastery.empty() ? "" : "; mastery: ",
        g_pending.mastery.c_str(), g_pending.conditions.empty() ? "" : "; conditions: ",
        g_pending.conditions.c_str(), first, second, kept);
  }
  return HalflingLuck(rules_object, edx, count, sides, kept);
}

bool PatchAttackD20Call() {
  const auto* code = reinterpret_cast<const uint8_t*>(nwmain::kAttackD20Call);
  int32_t rel = 0;
  memcpy(&rel, code + 1, sizeof(rel));
  if (code[0] != 0xe8 || nwmain::kAttackD20Call + nwmain::kCallLength + rel != nwmain::kRollDice) {
    Log("SKIP advantage_disadvantage: the attack d20 call at 0x%08X is not the mapped call", nwmain::kAttackD20Call);
    return false;
  }
  uint8_t expected[5];
  memcpy(expected, code, sizeof(expected));
  const int32_t new_rel = static_cast<int32_t>(reinterpret_cast<uintptr_t>(&RollAttackD20) -
                                               (nwmain::kAttackD20Call + nwmain::kCallLength));
  uint8_t call[5] = {0xe8};
  memcpy(call + 1, &new_rel, sizeof(new_rel));
  return PatchBytes("ResolveAttackRoll attack d20 (advantage/disadvantage)", nwmain::kAttackD20Call, expected, call,
                    sizeof(call));
}

}  // namespace

int PendingSituationalTotal(const void* stats, const void* target) {
  if (!g_pending.active || stats != g_pending.attacker_stats || target != g_pending.target) return 0;
  return g_pending.nwn_total;
}

void InstallAdvantageRules() {
  if (g_config.advantage_disadvantage && !PatchAttackD20Call()) g_config.advantage_disadvantage = false;
  // Weapon mastery (D3) fills empty slots from this hook too.
  const bool mastery = g_config.weapon_mastery == rules::RuleVariant::kFaithful;
  if (!g_config.advantage_disadvantage && !g_config.debug_attack_trace && !g_config.debug_combat_breakdown && !mastery &&
      !g_config.resume_attack_after_potion && g_config.debug_creature_hp_multiplier <= 1 &&
      g_config.debug_creature_min_hp <= 0 && g_config.knockdown != rules::RuleVariant::kFaithful) {
    return;
  }
  InstallHook({g_config.advantage_disadvantage ? "CNWSCreature::ResolveAttackRoll (advantage/disadvantage)"
                                               : "CNWSCreature::ResolveAttackRoll (trace, weapon mastery)",
               nwmain::kResolveAttackRoll, nwmain::kResolveAttackRollPrologue,
               sizeof(nwmain::kResolveAttackRollPrologue), reinterpret_cast<void*>(&ResolveAttackRoll),
               reinterpret_cast<void**>(&g_resolve_attack_roll)});
}

// SK10 (rules_spell_attack.cpp): the d20 of a touch attack, with the roll mode
// an attack would have (NWN's situational terms as advantage / disadvantage —
// the touch bonus never had them, so nothing is taken out —, the 2024
// close-combat check for ranged, conditions) and Halfling Luck.
int RollSpellAttackD20(void* caster, void* target, bool ranged) {
  void* rules_object = *reinterpret_cast<void**>(nwmain::kRulesObject);
  const PendingAttack outer = g_pending;
  g_pending = PendingAttack();
  void* stats = caster ? Field<void*>(caster, nwmain::kCreatureStats) : nullptr;
  if (caster && target && stats && g_config.advantage_disadvantage) {
    g_pending.terms = ReadNwnTerms(caster, target, ranged);
    g_pending.nwn_total = 0;
    g_pending.point_blank_shot = HasFeat(caster, nwmain::kFeatPointBlankShot);
    g_pending.threats = ranged ? CountCloseCombatThreats(caster) : 0;
    const ConditionRollSources conditions = ConditionAttackRollSources(caster, target, ranged);
    g_pending.conditions = conditions.note;
    g_pending.mode = rules::SituationalRollMode(
        g_pending.terms, rules::RangedCloseCombat(ranged, g_pending.point_blank_shot, g_pending.threats),
        conditions.advantage, conditions.disadvantage);
    g_pending.attacker_stats = stats;
    g_pending.target = target;
    g_pending.attacker_id = Field<uint32_t>(caster, nwmain::kCreatureObjectId);
    g_pending.target_id = Field<uint32_t>(target, nwmain::kCreatureObjectId);
    g_pending.active = true;
  }
  const int d20 = rules_object ? RollAttackD20(rules_object, nullptr, 1, 20) & 0xFFFF : 10;
  g_pending = outer;
  return d20;
}
