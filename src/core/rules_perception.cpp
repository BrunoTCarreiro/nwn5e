// Decision SK5 (PLAN.md), faithful: Stealth against Perception the 2024 way.
// NWN caches a Hide / Move Silently d20 for a creature in stealth mode and a
// Spot / Listen roll for every creature every 6 s (0x65302C), and compares
// them in the Listen (0x653180) and Spot (0x653870) detection checks
// (RE-NOTES "Stealth and perception"). Faithful:
// - the hider rolls Stealth once, when it starts hiding (SetStealthMode
//   0x67AFF0) — one d20 for Hide and Move Silently, both Stealth (SK1) — and
//   keeps it until it stops hiding;
// - an observer uses passive Perception (10 + its modifier) unless it searches
//   (detect mode: one d20 for Spot and Listen, both Perception); NWN gave an
//   observer that moves without searching a d10 and half its skill;
// - no size modifier, no -10 while in combat, no Favored Enemy bonus (feat
//   bonuses to skills are gone in faithful, SK2), and a Stealth total of 0 no
//   longer means "can't hide" (a 3e ranks rule).
// Distance, area, movement, facing and light terms stay (NWN's real-time
// stealth needs them). NWN mode: NWN's.

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <unordered_map>
#include <unordered_set>

#include "config.h"
#include "creature.h"
#include "hooks.h"
#include "log.h"
#include "nwmain_1_69.h"
#include "rules/skills.h"

bool IsPlayerControlled(uint32_t object_id);  // rules_chat.cpp

namespace {

using RollDiceFn = int(__fastcall*)(void* rules_object, void* edx, int count, int sides);
using SetStealthModeFn = void(__fastcall*)(void* creature, void* edx, uint8_t mode);
using DetectionFn = int(__fastcall*)(void* observer, void* edx, void* target, int flag);

const auto g_roll_dice = reinterpret_cast<RollDiceFn>(nwmain::kRollDice);
SetStealthModeFn g_set_stealth_mode = nullptr;
DetectionFn g_listen_detection = nullptr;
DetectionFn g_spot_detection = nullptr;

std::unordered_set<uint32_t> g_rolled;  // hiding creatures whose Stealth roll for this hide is made
std::unordered_map<uint64_t, int> g_noticed;  // (observer, hider, Spot / Listen) -> last result, for the log
int g_log_lines = 0;

uint32_t IdOf(const void* creature) { return Field<uint32_t>(creature, nwmain::kCreatureObjectId); }

bool LogSlot() {
  if (!g_config.log_attacks || g_log_lines >= 400) return false;
  ++g_log_lines;
  return true;
}

int D20() {
  void* rules_object = *reinterpret_cast<void**>(nwmain::kRulesObject);
  return rules_object ? (g_roll_dice(rules_object, nullptr, 1, 20) & 0xFF) : 10;
}

void RollStealth(void* creature, const char* when) {
  const int roll = D20();
  SetField<uint8_t>(creature, nwmain::kCreatureHideRoll, static_cast<uint8_t>(roll));
  SetField<uint8_t>(creature, nwmain::kCreatureMoveSilentlyRoll, static_cast<uint8_t>(roll));
  g_rolled.insert(IdOf(creature));
  if (IsPlayerControlled(IdOf(creature)) && LogSlot()) {
    Log("stealth: 0x%08X %s: Stealth d20 %d, kept until it stops hiding", IdOf(creature), when, roll);
  }
}

void __fastcall SetStealthMode(void* creature, void* edx, uint8_t mode) {
  const bool starts = mode == 1 && Field<uint8_t>(creature, nwmain::kCreatureStealthMode) != 1;
  g_set_stealth_mode(creature, edx, mode);
  if (starts) {
    RollStealth(creature, "starts hiding");
  } else if (mode != 1) {
    g_rolled.erase(IdOf(creature));
  }
}

// The 6 s perception rolls (from PerceptionRollsStub), the 2024 way.
void __stdcall PerceptionRolls(void* creature) {
  const uint32_t id = IdOf(creature);
  if (Field<uint8_t>(creature, nwmain::kCreatureStealthMode) == 1) {
    if (!g_rolled.count(id)) RollStealth(creature, "is hiding without a roll (loaded hidden)");
  } else {
    g_rolled.erase(id);
  }
  const bool searching = Field<uint8_t>(creature, nwmain::kCreatureDetectMode) != 0;
  const int die = rules::PerceptionDie(searching, searching ? D20() : 0);
  SetField<uint8_t>(creature, nwmain::kCreatureSpotRoll, static_cast<uint8_t>(die));
  SetField<uint8_t>(creature, nwmain::kCreatureListenRoll, static_cast<uint8_t>(die));
}

// Jumped to from 0x65303A, in place of the roll block (through 0x653112); goes
// on at 0x653118 with ecx / edx loaded as the block left them (the time of
// these rolls, stored there).
__declspec(naked) void PerceptionRollsStub() {
  __asm {
    pushad
    push esi
    call PerceptionRolls
    popad
    mov ecx, dword ptr [esp + 0x20]
    mov edx, dword ptr [esp + 0x0C]
    push 0x00653118
    ret
  }
}

// Logs when an observer starts or stops noticing a hiding creature, if either
// is a player's (owner tests; the engine has no log of its own for this).
int Detection(DetectionFn original, const char* sense, void* observer, void* edx, void* target, int flag) {
  const int result = original(observer, edx, target, flag);
  if (!g_config.log_attacks || !target || Field<uint8_t>(target, nwmain::kCreatureStealthMode) != 1) return result;
  const uint64_t key = (static_cast<uint64_t>(IdOf(observer)) << 32 | IdOf(target)) ^ (sense[0] == 'S' ? 1ull << 63 : 0);
  const auto it = g_noticed.find(key);
  if (it != g_noticed.end() && it->second == result) return result;
  g_noticed[key] = result;
  if ((IsPlayerControlled(IdOf(target)) || IsPlayerControlled(IdOf(observer))) && LogSlot()) {
    const bool searching = Field<uint8_t>(observer, nwmain::kCreatureDetectMode) != 0;
    Log("perception: 0x%08X %s 0x%08X (%s; its Stealth d20 %d; observer %s, die %d)", IdOf(observer),
        result ? (sense[0] == 'S' ? "sees" : "hears") : (sense[0] == 'S' ? "does not see" : "does not hear"),
        IdOf(target), sense, Field<uint8_t>(target, nwmain::kCreatureHideRoll), searching ? "searching" : "passive",
        Field<uint8_t>(observer, sense[0] == 'S' ? nwmain::kCreatureSpotRoll : nwmain::kCreatureListenRoll));
  }
  return result;
}

int __fastcall ListenDetection(void* observer, void* edx, void* target, int flag) {
  return Detection(g_listen_detection, "Listen", observer, edx, target, flag);
}

int __fastcall SpotDetection(void* observer, void* edx, void* target, int flag) {
  return Detection(g_spot_detection, "Spot", observer, edx, target, flag);
}

}  // namespace

void InstallPerceptionRules() {
  if (g_config.perception != rules::RuleVariant::kFaithful) return;
  uint8_t jump[7] = {0xe9, 0, 0, 0, 0, 0x90, 0x90};  // jmp rel32 (as long as a call) + 2 nops
  const int32_t rel = static_cast<int32_t>(reinterpret_cast<uintptr_t>(&PerceptionRollsStub) -
                                           (nwmain::kPerceptionRollBlock + nwmain::kCallLength));
  memcpy(jump + 1, &rel, sizeof(rel));
  if (!PatchBytes("perception: 6 s rolls, passive Perception and one Stealth roll per hide (faithful)",
                  nwmain::kPerceptionRollBlock, nwmain::kPerceptionRollBlockBytes, jump, sizeof(jump))) {
    return;
  }
  InstallHook({"CNWSCreature::SetStealthMode (Stealth rolled when hiding starts)", nwmain::kSetStealthMode,
               nwmain::kSetStealthModePrologue, sizeof(nwmain::kSetStealthModePrologue),
               reinterpret_cast<void*>(&SetStealthMode), reinterpret_cast<void**>(&g_set_stealth_mode)});
  int applied = 0;
  for (const nwmain::MediumPatch& patch : nwmain::kFaithfulPerceptionPatches) {
    char name[112];
    sprintf_s(name, "perception: %s (faithful)", patch.what);
    applied += PatchBytes(name, patch.address, patch.original, patch.patched, patch.size) ? 1 : 0;
  }
  Log("perception: %d of %u faithful patches applied", applied,
      static_cast<unsigned>(sizeof(nwmain::kFaithfulPerceptionPatches) / sizeof(nwmain::kFaithfulPerceptionPatches[0])));
  InstallHook({"Listen detection (log)", nwmain::kListenDetection, nwmain::kListenDetectionPrologue,
               sizeof(nwmain::kListenDetectionPrologue), reinterpret_cast<void*>(&ListenDetection),
               reinterpret_cast<void**>(&g_listen_detection)});
  InstallHook({"Spot detection (log)", nwmain::kSpotDetection, nwmain::kSpotDetectionPrologue,
               sizeof(nwmain::kSpotDetectionPrologue), reinterpret_cast<void*>(&SpotDetection),
               reinterpret_cast<void**>(&g_spot_detection)});
}
