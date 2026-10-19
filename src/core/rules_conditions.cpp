// Conditions (PLAN.md "Conditions — decided by the owner", 2026-10-19). Each
// item has a faithful (2024) and an NWN-mode version, switched by the
// `conditions` key; the decisions are rules/conditions.h, the engine facts
// RE-NOTES "Condition effects". The conditions are read from the creature's
// applied effects (any it holds — NWN keeps only the highest state number in
// one byte).
//
// - CN8 Deafened (faithful): NWN's deafness also adds 20% spell failure, which
//   only ever hit Bard / Sorcerer / Wizard spells; it is applied at 0%.
// - CN1 Restrained (faithful): NWN's entangle -2 attack and -4 Dexterity are
//   applied at 0 (it still can't move); being restrained gives advantage to
//   attacks against it and disadvantage on its attacks and Dexterity saves.
// - CN2/CN3: stunned, paralysed and sleeping targets give advantage (both
//   modes) and fail Strength and Dexterity saves (stunned: both modes;
//   paralysed, asleep: faithful); a hit on a paralysed or sleeping creature
//   from within 5 ft is a critical hit (faithful).
// - CN4 Poisoned (faithful): disadvantage on its attack rolls while an NWN
//   poison effect is on it.
// - CN5 Frightened (faithful): NWN's fear state never takes hold — it makes
//   the creature flee, takes a player's control away and stops NWN's monster
//   AI from fighting at all. Instead the creature gets the fear icon for the
//   effect's duration, credited to the source, and attacks with disadvantage
//   while it can see that source (2024). Immunities and their feedback stay
//   NWN's. Remove Fear and the like don't end it early.
// - Coup de grace (faithful): gone (2024 has none).
// - CN6 Concealment (faithful, with advantage_disadvantage): NWN's
//   concealment / miss-chance roll after a hit is skipped; a target with a
//   concealment effect (Blur, Displacement, ...) or Epic Self-Concealment gives
//   disadvantage instead (invisible and unseen targets already do, B1).

#include <stdint.h>
#include <stdio.h>

#include "condition_effects.h"
#include "config.h"
#include "creature.h"
#include "engine_queries.h"
#include "hooks.h"
#include "log.h"
#include "engine_effects.h"
#include "nwmain_1_69.h"

void TellPlayer(uint32_t object_id, const std::string& line);  // rules_chat.cpp

namespace {

using GetAttackFn = void*(__fastcall*)(void* round, void* edx, int index);
using HasFeatFn = int(__fastcall*)(void* stats, void* edx, int feat);
using GetIsImmuneFn = int(__fastcall*)(void* stats, void* edx, int type, void* versus);

const auto g_get_attack = reinterpret_cast<GetAttackFn>(nwmain::kGetAttack);
const auto g_has_feat = reinterpret_cast<HasFeatFn>(nwmain::kHasFeat);
const auto g_get_is_immune = reinterpret_cast<GetIsImmuneFn>(nwmain::kGetIsImmune);

bool LogSlot();

using OnApplySetStateFn = int(__fastcall*)(void* handler, void* edx, void* object, void* effect, int loading);
OnApplySetStateFn g_on_apply_set_state = nullptr;
constexpr float kPermanentFearSeconds = 60.0f;

// CN5: the source of the fear nwn5e put on `creature` (its fear icon), or
// kObjectInvalid.
uint32_t FearSource(const void* creature) {
  uint32_t source = nwmain::kObjectInvalid;
  engine::ForEachEffectOfType(creature, nwmain::kEffectTypeIcon, [&](const void* effect) {
    if (engine::EffectInteger(effect, 0) == nwmain::kIconFrightened) source = Field<uint32_t>(effect, nwmain::kEffectCreator);
  });
  return source;
}

// NWN's own refusals of a fear effect (the handler sends their feedback).
bool NwnRefusesFear(void* creature, const void* effect) {
  if (Field<int32_t>(creature, nwmain::kCreatureDeadFlag) != 0 || engine::IsDeadOrDying(creature) ||
      Field<int32_t>(creature, nwmain::kCreatureRefusesStates) != 0) {
    return true;
  }
  void* stats = Field<void*>(creature, nwmain::kCreatureStats);
  if (!stats) return true;
  void* versus = engine::GetCreature(Field<uint32_t>(effect, nwmain::kEffectCreator));
  if (g_get_is_immune(stats, nullptr, nwmain::kImmunityFear, versus) == 1 ||
      g_has_feat(stats, nullptr, nwmain::kFeatFearImmunity) == 1) {
    return true;
  }
  const bool mind_immune = g_get_is_immune(stats, nullptr, nwmain::kImmunityMindSpells, versus) == 1 ||
                           g_has_feat(stats, nullptr, nwmain::kFeatMindImmunity) == 1;
  const bool bypass = (Field<uint16_t>(effect, nwmain::kEffectFlags) & 0x18) == 0x10 && engine::EffectInteger(effect, 1) == 1;
  return mind_immune && !bypass;
}

int __fastcall OnApplySetState(void* handler, void* edx, void* object, void* effect, int loading) {
  if (g_config.conditions != rules::RuleVariant::kFaithful || !object || !effect || loading ||
      engine::EffectInteger(effect, 0) != nwmain::kStateFrightened) {
    return g_on_apply_set_state(handler, edx, object, effect, loading);
  }
  const uint32_t id = Field<uint32_t>(object, nwmain::kCreatureObjectId);
  void* creature = engine::GetCreature(id);
  if (!creature || NwnRefusesFear(creature, effect)) return g_on_apply_set_state(handler, edx, object, effect, loading);
  const uint32_t source = Field<uint32_t>(effect, nwmain::kEffectCreator);
  const bool temporary = (Field<uint16_t>(effect, nwmain::kEffectFlags) & 0x7) == nwmain::kEffectDurationTemporary;
  const float seconds = temporary ? Field<float>(effect, nwmain::kEffectDuration) : kPermanentFearSeconds;
  engine::ApplyIcon(creature, source, nwmain::kIconFrightened, seconds, 0);
  char line[160];
  sprintf_s(line, "nwn5e: frightened for %.0f s - disadvantage on your attacks while you can see its source; you keep control",
            seconds);
  TellPlayer(id, line);
  if (LogSlot()) Log("conditions: 0x%08X frightened by 0x%08X for %.0f s (2024 fear; NWN's flee state not applied)", id, source, seconds);
  return 1;  // NWN's frightened state is not kept
}

constexpr float kFiveFeet = 1.524f;  // metres, edge to edge
bool g_concealment_roll_removed = false;
int g_log_lines = 0;

bool LogSlot() { return g_config.log_attacks && g_log_lines < 300 && ++g_log_lines; }

bool HasFeat(void* creature, int feat) {
  void* stats = Field<void*>(creature, nwmain::kCreatureStats);
  return stats && g_has_feat(stats, nullptr, feat) == 1;
}

// CN6: a concealment effect on `target` that applies to this attack (its
// melee / ranged filter and race filter; alignment-limited ones are left
// out), or Epic Self-Concealment.
bool TargetConcealed(void* attacker, void* target, bool ranged) {
  void* attacker_stats = Field<void*>(attacker, nwmain::kCreatureStats);
  const int race = attacker_stats ? Field<uint16_t>(attacker_stats, nwmain::kStatsRace) : -1;
  bool concealed = false;
  engine::ForEachEffectOfType(target, nwmain::kEffectTypeConcealment, [&](const void* effect) {
    if (engine::EffectInteger(effect, 0) <= 0) return;
    const int effect_race = engine::EffectInteger(effect, 1);
    if (effect_race != nwmain::kRacialTypeAll && effect_race != race) return;
    if (engine::EffectInteger(effect, 2) != 0 || engine::EffectInteger(effect, 3) != 0) return;
    const int kind = engine::EffectInteger(effect, 4);  // 0 all, 2 ranged, 3 melee
    if ((kind == 2 && !ranged) || (kind == 3 && ranged)) return;
    concealed = true;
  });
  for (int feat = nwmain::kFeatEpicSelfConcealmentFirst; !concealed && feat <= nwmain::kFeatEpicSelfConcealmentLast;
       ++feat) {
    concealed = HasFeat(target, feat);
  }
  return concealed;
}

void AddName(std::string* names, bool on, const char* name) {
  if (!on) return;
  if (!names->empty()) *names += ", ";
  *names += name;
}

}  // namespace

rules::Conditions ReadConditions(void* creature) {
  rules::Conditions conditions;
  if (!creature) return conditions;
  conditions.stunned = engine::HasState(creature, nwmain::kStateStunned);
  conditions.paralyzed = engine::HasState(creature, nwmain::kStateParalyzed);
  conditions.asleep = engine::HasState(creature, nwmain::kStateAsleep);
  conditions.restrained = engine::HasEffectType(creature, nwmain::kEffectTypeEntangle);
  conditions.poisoned = engine::HasEffectType(creature, nwmain::kEffectTypePoison);
  return conditions;
}

bool IncapacitatedByCondition(void* creature) {
  return engine::HasState(creature, nwmain::kStateDazed) || engine::HasState(creature, nwmain::kStateStunned) ||
         engine::HasState(creature, nwmain::kStateParalyzed) || engine::HasState(creature, nwmain::kStateAsleep) ||
         engine::HasState(creature, nwmain::kStatePetrified);
}

ConditionRollSources ConditionAttackRollSources(void* attacker, void* target, bool ranged) {
  ConditionRollSources out;
  if (g_config.conditions == rules::RuleVariant::kOff || !attacker || !target) return out;
  const rules::Conditions attacker_conditions = ReadConditions(attacker);
  rules::Conditions attacker_frightened = attacker_conditions;
  const uint32_t fear_source = FearSource(attacker);
  attacker_frightened.frightened_source_in_sight =
      fear_source != nwmain::kObjectInvalid && engine::Sees(attacker, fear_source);
  rules::Conditions target_conditions = ReadConditions(target);
  target_conditions.concealed = g_concealment_roll_removed && TargetConcealed(attacker, target, ranged);
  const rules::RollSources sources =
      rules::ConditionAttackSources(attacker_frightened, target_conditions, g_config.conditions);
  out.advantage = sources.advantage;
  out.disadvantage = sources.disadvantage;
  const bool faithful = g_config.conditions == rules::RuleVariant::kFaithful;
  AddName(&out.note, target_conditions.stunned, "target stunned");
  AddName(&out.note, target_conditions.paralyzed, "target paralysed");
  AddName(&out.note, target_conditions.asleep, "target asleep");
  AddName(&out.note, faithful && target_conditions.restrained, "target restrained");
  AddName(&out.note, faithful && target_conditions.concealed, "target concealed");
  AddName(&out.note, faithful && attacker_conditions.restrained, "attacker restrained");
  AddName(&out.note, faithful && attacker_conditions.poisoned, "attacker poisoned");
  AddName(&out.note, faithful && attacker_frightened.frightened_source_in_sight, "attacker frightened, source in sight");
  return out;
}

ConditionSave ConditionsForSave(void* creature, rules::Ability ability) {
  ConditionSave out;
  if (g_config.conditions == rules::RuleVariant::kOff || !creature) return out;
  const rules::Conditions conditions = ReadConditions(creature);
  out.auto_fail = rules::ConditionSaveOutcome(conditions, ability, g_config.conditions) == rules::SaveOutcome::kAutoFail;
  out.disadvantage = rules::ConditionSaveDisadvantage(conditions, ability, g_config.conditions);
  if (out.auto_fail) {
    out.note = conditions.stunned ? "stunned" : (conditions.paralyzed ? "paralysed" : "asleep");
  } else if (out.disadvantage > 0) {
    out.note = "restrained";
  }
  return out;
}

void ConditionsAfterAttackRoll(void* attacker, void* target_object) {
  if (g_config.conditions != rules::RuleVariant::kFaithful || !attacker || !target_object) return;
  void* round = Field<void*>(attacker, nwmain::kCreatureCombatRound);
  void* attack = round ? g_get_attack(round, nullptr, Field<uint8_t>(round, nwmain::kRoundCurrentAttack)) : nullptr;
  if (!attack || Field<uint8_t>(attack, nwmain::kAttackDataResult) != nwmain::kAttackResultHit) return;
  const uint32_t target_id = Field<uint32_t>(target_object, nwmain::kCreatureObjectId);
  void* target = engine::GetCreature(target_id);
  void* target_stats = target ? Field<void*>(target, nwmain::kCreatureStats) : nullptr;
  if (!target_stats) return;
  const bool within_five_feet = engine::EdgeDistance(attacker, target) <= kFiveFeet;
  if (!rules::ConditionForcesCritical(ReadConditions(target), within_five_feet, g_config.conditions)) return;
  // The engine's own critical-hit checks (0x74477B): immunity, the feat it
  // tests with it, and — unless crits_against_players removed it — the
  // difficulty rule against player characters.
  const uint32_t attacker_id = Field<uint32_t>(attacker, nwmain::kCreatureObjectId);
  const char* refused = nullptr;
  if (g_get_is_immune(target_stats, nullptr, nwmain::kImmunityCriticalHit, attacker) != 0 ||
      g_has_feat(target_stats, nullptr, nwmain::kFeatCriticalHitImmunity) == 1) {
    refused = "immune to critical hits";
  } else if (!g_config.crits_against_players && ReadCreatureFacts(target_stats).character) {
    refused = "no critical hits on characters (crits_against_players=0)";
  }
  if (refused) {
    if (LogSlot()) Log("conditions: 0x%08X hit helpless 0x%08X: %s, stays a hit", attacker_id, target_id, refused);
    return;
  }
  SetField<uint8_t>(attack, nwmain::kAttackDataResult, nwmain::kAttackResultCritical);
  if (LogSlot()) {
    Log("conditions: 0x%08X hit 0x%08X (paralysed or asleep) from within 5 ft: critical hit", attacker_id, target_id);
  }
}

void InstallConditionRules() {
  if (g_config.conditions != rules::RuleVariant::kFaithful) return;
  int applied = 0;
  for (const nwmain::SmallPatch& patch : nwmain::kFaithfulConditionPatches) {
    char name[96];
    sprintf_s(name, "conditions: %s (faithful)", patch.what);
    applied += PatchBytes(name, patch.address, patch.original, patch.patched, patch.size) ? 1 : 0;
  }
  Log("conditions: %d of %u faithful patches applied", applied,
      static_cast<unsigned>(sizeof(nwmain::kFaithfulConditionPatches) /
                            sizeof(nwmain::kFaithfulConditionPatches[0])));
  InstallHook({"CNWSEffectListHandler::OnApplySetState (CN5 2024 fear)", nwmain::kOnApplySetState,
               nwmain::kOnApplySetStatePrologue, sizeof(nwmain::kOnApplySetStatePrologue),
               reinterpret_cast<void*>(&OnApplySetState), reinterpret_cast<void**>(&g_on_apply_set_state)});
  // CN6 replaces the miss chance with disadvantage, so it needs the
  // advantage / disadvantage hook.
  if (g_config.advantage_disadvantage) {
    const nwmain::SmallPatch& gate = nwmain::kFaithfulConcealmentGate;
    g_concealment_roll_removed =
        PatchBytes("conditions: no concealment / miss chance roll (faithful)", gate.address, gate.original,
                   gate.patched, gate.size);
  } else {
    Log("conditions: concealment keeps NWN's miss chance (advantage_disadvantage=0)");
  }
}
