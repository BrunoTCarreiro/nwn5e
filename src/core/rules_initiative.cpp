// Decision SK9 (PLAN.md), faithful: initiative is a 2024 Dexterity check.
// NWN (0x655AF0, RE-NOTES "Initiative"): d20 + Dex modifier + 8 with Epic
// Superior Initiative or else + 4 with Improved Initiative, + 2 each with
// Blooded and Thug. Faithful: d20 + Dex modifier + the proficiency bonus with
// Improved Initiative (2024's Alert); the other feat bonuses are gone (Epic
// Superior Initiative until Phase 3 sorts the epic feats; Blooded and Thug
// have no 2024 counterpart). The d20 has advantage while the creature is
// invisible or hidden (2024's Invisible condition) and Halfling Luck rerolls
// a natural 1. NWN mode: NWN's.

#include <stdint.h>
#include <string.h>

#include <string>

#include "condition_effects.h"
#include "config.h"
#include "creature.h"
#include "hooks.h"
#include "log.h"
#include "log_once.h"
#include "nwmain_1_69.h"
#include "rules/saves.h"
#include "rules/skills.h"

bool IsPlayerControlled(uint32_t object_id);  // rules_chat.cpp
bool HiddenByStealth(void* creature);         // rules_perception.cpp
bool HasAlertFromBackground(void* creature);  // rules_abilities.cpp (CM5)
bool HasRemarkableAthleteAdvantage(void* creature);  // rules_fighter.cpp (Fighter C2)

namespace {

using DexModFn = int(__fastcall*)(void* stats, void* edx, int armor_cap);
using HasFeatFn = int(__fastcall*)(void* stats, void* edx, int feat);
using RollDiceFn = int(__fastcall*)(void* rules_object, void* edx, int count, int sides);
using GetInvisibleFn = int(__fastcall*)(void* creature, void* edx, void* object, int flag);
using AsObjectFn = void*(__fastcall*)(void* creature, void* edx);

const auto g_get_dex_mod = reinterpret_cast<DexModFn>(nwmain::kGetDexMod);
const auto g_has_feat = reinterpret_cast<HasFeatFn>(nwmain::kHasFeat);
const auto g_roll_dice = reinterpret_cast<RollDiceFn>(nwmain::kRollDice);
const auto g_get_invisible = reinterpret_cast<GetInvisibleFn>(nwmain::kGetInvisible);
LogOnce g_logged(300);
int g_roll_lines = 0;

// In place of the initiative function's GetDexMod(0) (0x655B3E): the whole
// 2024 initiative modifier (the feat bonuses after it are skipped).
int __fastcall InitiativeModifier(void* stats, void* edx, int armor_cap) {
  const int dex = static_cast<int8_t>(g_get_dex_mod(stats, edx, armor_cap) & 0xFF);
  const CreatureFacts facts = ReadCreatureFacts(stats);
  if (!facts.plausible) return dex;
  const int pb = rules::ProficiencyBonusForCreature(facts.character, facts.level, facts.challenge_rating);
  const bool alert = g_has_feat(stats, nullptr, nwmain::kFeatImprovedInitiative) != 0 ||
                     HasAlertFromBackground(Field<void*>(stats, nwmain::kStatsCreature));  // CM5: Criminal
  const int modifier = rules::InitiativeModifier2024(dex, pb, alert);
  if (g_config.log_attacks && IsPlayerControlled(ObjectIdOf(stats)) &&
      g_logged.First(LogOnce::Key(ObjectIdOf(stats), dex, modifier))) {
    Log("initiative: object 0x%08X modifier 2024 %+d (Dex %+d%s)", ObjectIdOf(stats), modifier, dex,
        alert ? ", PB with Improved Initiative = Alert" : "");
  }
  return modifier;
}

bool Invisible(void* creature) {
  void* object = (*reinterpret_cast<AsObjectFn* const*>(creature))[nwmain::kVtableAsObject / 4](creature, nullptr);
  return g_get_invisible(creature, nullptr, object, 0) != 0;
}

int D20(void* rules_object) { return g_roll_dice(rules_object, nullptr, 1, 20) & 0xFFFF; }

// The initiative d20 (from InitiativeD20Stub).
int __stdcall InitiativeD20(void* creature) {
  void* rules_object = *reinterpret_cast<void**>(nwmain::kRulesObject);
  if (!rules_object) return 10;
  const bool invisible = creature && Invisible(creature);
  const bool hidden = creature && !invisible && HiddenByStealth(creature);
  const bool athlete = creature && !invisible && !hidden && HasRemarkableAthleteAdvantage(creature);  // Fighter C2
  const std::string worse = creature ? CheckDisadvantage(creature) : std::string();  // SK11
  const int advantage = invisible || hidden || athlete ? 1 : 0;
  const int disadvantage = worse.empty() ? 0 : 1;
  const int first = D20(rules_object);
  const int second = advantage || disadvantage ? D20(rules_object) : 0;
  int kept = rules::KeptCheckD20(first, second, advantage, disadvantage);
  void* stats = creature ? Field<void*>(creature, nwmain::kCreatureStats) : nullptr;
  int luck = 0;
  if (kept == 1 && stats && Field<uint16_t>(stats, nwmain::kStatsRace) == nwmain::kRaceHalfling) {
    luck = D20(rules_object);  // Halfling Luck: a natural 1 on a d20 test is rerolled once
    kept = luck;
  }
  const uint32_t id = creature ? Field<uint32_t>(creature, nwmain::kCreatureObjectId) : nwmain::kObjectInvalid;
  if (g_config.log_attacks && (second || luck) && g_roll_lines < 200 && IsPlayerControlled(id)) {
    ++g_roll_lines;
    std::string why = invisible ? "advantage: invisible"
                      : hidden  ? "advantage: hidden"
                      : athlete ? "advantage: Remarkable Athlete"
                                : "";
    if (disadvantage) why += (why.empty() ? "disadvantage: " : "; disadvantage: ") + worse;
    if (advantage && disadvantage) why += " - they cancel";
    if (luck) why += why.empty() ? "Halfling Luck rerolled a 1" : "; Halfling Luck rerolled a 1";
    Log("initiative: 0x%08X d20 %d%s%s -> %d (%s)", id, first, second ? " / " : "",
        second ? std::to_string(second).c_str() : "", kept, why.c_str());
  }
  return kept;
}

// In place of RollDice(1, 20) at 0x655B26 (ecx = rules object, two stack
// arguments, ret 8); edi = the creature rolling initiative.
__declspec(naked) void InitiativeD20Stub() {
  __asm {
    push edi
    call InitiativeD20
    ret 8
  }
}

bool RedirectCall(const char* name, uintptr_t site, uintptr_t expected_target, const void* new_target) {
  const auto* code = reinterpret_cast<const uint8_t*>(site);
  int32_t rel = 0;
  memcpy(&rel, code + 1, sizeof(rel));
  if (code[0] != 0xe8 || site + nwmain::kCallLength + rel != expected_target) {
    Log("SKIP %s: 0x%08X is not the mapped call", name, site);
    return false;
  }
  uint8_t expected[5];
  memcpy(expected, code, sizeof(expected));
  uint8_t call[5] = {0xe8};
  const int32_t new_rel = static_cast<int32_t>(reinterpret_cast<uintptr_t>(new_target) - (site + nwmain::kCallLength));
  memcpy(call + 1, &new_rel, sizeof(new_rel));
  return PatchBytes(name, site, expected, call, sizeof(call));
}

}  // namespace

void InstallInitiativeRules() {
  if (g_config.initiative != rules::RuleVariant::kFaithful) return;
  int applied = 0;
  // The feat bonuses go first: the 2024 modifier must not get NWN's +4 on top.
  if (PatchBytes("initiative: no NWN feat bonuses (faithful)", nwmain::kInitiativeFeatBonuses,
                 nwmain::kInitiativeFeatBonusesBytes, nwmain::kInitiativeFeatBonusesSkip,
                 sizeof(nwmain::kInitiativeFeatBonusesSkip))) {
    ++applied;
    applied += RedirectCall("initiative: Dex + PB with Improved Initiative (Alert) (faithful)",
                            nwmain::kInitiativeDexModCall, nwmain::kGetDexMod,
                            reinterpret_cast<void*>(&InitiativeModifier))
                   ? 1
                   : 0;
  }
  applied += RedirectCall("initiative: d20 with advantage when invisible or hidden, Halfling Luck (faithful)",
                          nwmain::kInitiativeD20Call, nwmain::kRollDice, reinterpret_cast<void*>(&InitiativeD20Stub))
                 ? 1
                 : 0;
  Log("initiative: %d of 3 faithful patches applied", applied);
}
