// CNWSCreature::AIUpdate 0x6475B0 (once per creature per AI master pass,
// nwmain_1_69.h) can be hooked only once; the rules that act on a creature's
// update hang off this one hook, after the engine's own update.

#include "config.h"
#include "hooks.h"
#include "nwmain_1_69.h"

void MasteryCreatureUpdate(void* creature);  // rules_mastery.cpp: Push slide, fall, watch
void PotionCreatureUpdate(void* creature);   // rules_potion.cpp: back to attacking after a potion

namespace {

using AIUpdateFn = void(__fastcall*)(void* creature, void* edx);
AIUpdateFn g_ai_update = nullptr;
bool g_push = false;
bool g_potion = false;

void __fastcall AIUpdate(void* creature, void* edx) {
  g_ai_update(creature, edx);
  if (!creature) return;
  if (g_push) MasteryCreatureUpdate(creature);
  if (g_potion) PotionCreatureUpdate(creature);
}

}  // namespace

// After InstallMasteryRules and InstallPotionRules (they decide what is on).
void InstallCreatureUpdate() {
  g_push = g_config.weapon_mastery == rules::RuleVariant::kFaithful;
  g_potion = g_config.resume_attack_after_potion;
  if (!g_push && !g_potion) return;
  InstallHook({"CNWSCreature::AIUpdate (Push slide and fall, back to attacking after a potion)",
               nwmain::kCreatureAIUpdate, nwmain::kCreatureAIUpdatePrologue,
               sizeof(nwmain::kCreatureAIUpdatePrologue), reinterpret_cast<void*>(&AIUpdate),
               reinterpret_cast<void**>(&g_ai_update)});
}
