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

#include "config.h"
#include "creature.h"
#include "engine_queries.h"
#include "hooks.h"
#include "log.h"
#include "nwmain_1_69.h"
#include "rules/nwn_situational.h"

void TraceAttackRoll(const void* creature);  // rules_attack_trace.cpp
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
  const uint8_t state = Field<uint8_t>(creature, nwmain::kCreatureActionState);
  return engine::IsDeadOrDying(creature) || state == 5 || state == 6;
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
  g_pending.mode = rules::SituationalRollMode(
      g_pending.terms, rules::RangedCloseCombat(ranged, g_pending.point_blank_shot, g_pending.threats));
  g_pending.attacker_stats = stats;
  g_pending.target = target;
  g_pending.attacker_id = Field<uint32_t>(creature, nwmain::kCreatureObjectId);
  g_pending.target_id = Field<uint32_t>(target, nwmain::kCreatureObjectId);
  g_pending.active = true;
}

void __fastcall ResolveAttackRoll(void* creature, void* edx, void* target) {
  if (g_config.debug_attack_trace) TraceAttackRoll(creature);
  KeepCombatBreakdownOn();
  const PendingAttack outer = g_pending;
  g_pending = PendingAttack();
  if (g_config.advantage_disadvantage) ReadPendingAttack(creature, target);
  if (g_pending.active && g_pending.mode == rules::RollMode::kNormal && g_pending.nwn_total != 0 && LogSlotLeft()) {
    Log("advantage: 0x%08X vs 0x%08X (%s): 3e situational %+d removed (%s); normal roll", g_pending.attacker_id,
        g_pending.target_id, g_pending.terms.ranged ? "ranged" : "melee", g_pending.nwn_total,
        TermNames(g_pending.terms).c_str());
  }
  g_resolve_attack_roll(creature, edx, target);
  g_pending = outer;
}

// Replaces ResolveAttackRoll's RollDice(1, 20) call for the attack roll.
int __fastcall RollAttackD20(void* rules_object, void* edx, int count, int sides) {
  if (!g_pending.active || g_pending.mode == rules::RollMode::kNormal) {
    return g_roll_dice(rules_object, edx, count, sides);
  }
  const int first = g_roll_dice(rules_object, edx, count, sides) & 0xFFFF;
  const int second = g_roll_dice(rules_object, edx, count, sides) & 0xFFFF;
  const bool keep_higher = g_pending.mode == rules::RollMode::kAdvantage;
  const int kept = keep_higher ? (first > second ? first : second) : (first < second ? first : second);
  if (LogSlotLeft()) {
    Log("advantage: 0x%08X vs 0x%08X (%s): %s; 3e situational %+d removed (%s); close-combat enemies %d%s; d20 %d / "
        "%d -> %d",
        g_pending.attacker_id, g_pending.target_id, g_pending.terms.ranged ? "ranged" : "melee",
        ModeName(g_pending.mode), g_pending.nwn_total, TermNames(g_pending.terms).c_str(), g_pending.threats,
        g_pending.point_blank_shot ? " (Point Blank Shot)" : "", first, second, kept);
  }
  return kept;
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
  if (!g_config.advantage_disadvantage && !g_config.debug_attack_trace && !g_config.debug_combat_breakdown) return;
  InstallHook({g_config.advantage_disadvantage ? "CNWSCreature::ResolveAttackRoll (advantage/disadvantage)"
                                               : "CNWSCreature::ResolveAttackRoll (trace)",
               nwmain::kResolveAttackRoll, nwmain::kResolveAttackRollPrologue,
               sizeof(nwmain::kResolveAttackRollPrologue), reinterpret_cast<void*>(&ResolveAttackRoll),
               reinterpret_cast<void**>(&g_resolve_attack_roll)});
}
