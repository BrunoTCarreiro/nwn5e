// CNWSCreature::AIUpdate 0x6475B0 (once per creature per AI master pass,
// nwmain_1_69.h) can be hooked only once; the rules that act on a creature's
// update hang off this one hook, after the engine's own update.

#include <windows.h>

#include <stdint.h>

#include <unordered_map>

#include "config.h"
#include "creature.h"
#include "engine_queries.h"
#include "hooks.h"
#include "log.h"
#include "nwmain_1_69.h"

bool IsPlayerControlled(uint32_t object_id);  // rules_chat.cpp

void MasteryCreatureUpdate(void* creature);  // rules_mastery.cpp: Push slide, fall, watch
void PotionCreatureUpdate(void* creature);   // rules_potion.cpp: back to attacking after a potion

namespace {

using AIUpdateFn = void(__fastcall*)(void* creature, void* edx);
AIUpdateFn g_ai_update = nullptr;
bool g_push = false;
bool g_potion = false;
bool g_idle_trace = false;

// Test log: a player's character left without orders in a fight ("lost its
// target", owner 2026-10-19). Logged once per idle spell, after 1.5 s, with
// what the engine still holds about its targets.
struct Idle {
  DWORD since = 0;
  bool logged = false;
};
std::unordered_map<uint32_t, Idle> g_idle;
int g_idle_lines = 0;

void IdleTrace(void* creature) {
  const uint32_t id = Field<uint32_t>(creature, nwmain::kCreatureObjectId);
  if (Field<int32_t>(creature, nwmain::kCreatureInCombat) == 0 || !engine::HasNoOrders(creature) ||
      engine::IsDeadOrDying(creature)) {
    if (!g_idle.empty()) g_idle.erase(id);
    return;
  }
  const DWORD now = GetTickCount();
  Idle& idle = g_idle[id];
  if (idle.since == 0) idle.since = now;
  if (idle.logged || now - idle.since < 1500) return;
  idle.logged = true;
  if (g_idle_lines >= 100 || !IsPlayerControlled(id)) return;
  ++g_idle_lines;
  const uint32_t enemy = engine::NearestSeenEnemy(creature, 5.0f);
  void* enemy_object = enemy == nwmain::kObjectInvalid ? nullptr : engine::GetCreature(enemy);
  void* round = Field<void*>(creature, nwmain::kCreatureCombatRound);
  Log("combat: 0x%08X idle in a fight for %.1f s: nearest seen enemy 0x%08X%s (%.1f m); stealth mode %d, detect mode %d; "
      "attack target 0x%08X, attempted 0x%08X, round target 0x%08X",
      id, (now - idle.since) / 1000.0f, enemy, enemy_object ? "" : " (none within 5 m)",
      enemy_object ? engine::EdgeDistance(creature, enemy_object) : 0.0f,
      Field<uint8_t>(creature, nwmain::kCreatureStealthMode), Field<uint8_t>(creature, nwmain::kCreatureDetectMode),
      Field<uint32_t>(creature, nwmain::kCreatureAttackTarget),
      Field<uint32_t>(creature, nwmain::kCreatureAttemptedAttackTarget),
      round ? Field<uint32_t>(round, nwmain::kRoundTarget) : nwmain::kObjectInvalid);
}

void __fastcall AIUpdate(void* creature, void* edx) {
  g_ai_update(creature, edx);
  if (!creature) return;
  if (g_push) MasteryCreatureUpdate(creature);
  if (g_potion) PotionCreatureUpdate(creature);
  if (g_idle_trace) IdleTrace(creature);
}

}  // namespace

// After InstallMasteryRules and InstallPotionRules (they decide what is on).
void InstallCreatureUpdate() {
  g_push = g_config.weapon_mastery == rules::RuleVariant::kFaithful;
  g_potion = g_config.resume_attack_after_potion;
  g_idle_trace = g_config.log_attacks;
  if (!g_push && !g_potion && !g_idle_trace) return;
  InstallHook({"CNWSCreature::AIUpdate (Push slide and fall, back to attacking after a potion)",
               nwmain::kCreatureAIUpdate, nwmain::kCreatureAIUpdatePrologue,
               sizeof(nwmain::kCreatureAIUpdatePrologue), reinterpret_cast<void*>(&AIUpdate),
               reinterpret_cast<void**>(&g_ai_update)});
}
