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

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <string>
#include <unordered_map>
#include <unordered_set>

#include "config.h"
#include "creature.h"
#include "engine_effects.h"
#include "engine_queries.h"
#include "hooks.h"
#include "log.h"
#include "nwmain_1_69.h"
#include "rules/skills.h"

bool IsPlayerControlled(uint32_t object_id);                       // rules_chat.cpp
void TellPlayer(uint32_t object_id, const std::string& line);  // rules_chat.cpp

namespace {

using RollDiceFn = int(__fastcall*)(void* rules_object, void* edx, int count, int sides);
using SetStealthModeFn = void(__fastcall*)(void* creature, void* edx, uint8_t mode);
using DetectionFn = int(__fastcall*)(void* observer, void* edx, void* target, int flag);
using GetSkillRankFn = char(__fastcall*)(void* stats, void* edx, uint8_t skill, void* vs, int base_only);

const auto g_roll_dice = reinterpret_cast<RollDiceFn>(nwmain::kRollDice);
SetStealthModeFn g_set_stealth_mode = nullptr;
DetectionFn g_listen_detection = nullptr;
DetectionFn g_spot_detection = nullptr;
const auto g_get_skill_rank = reinterpret_cast<GetSkillRankFn>(nwmain::kGetSkillRank);  // through the skills hook
constexpr uint8_t kSkillHide = 5;  // skills.2da (Stealth)
constexpr int kHideDc = 15;        // the 2024 Hide action

std::unordered_set<uint32_t> g_rolled;  // hiding creatures whose Stealth roll for this hide is made
std::unordered_set<uint32_t> g_not_hidden;  // ... whose Stealth total missed 15 (hiding_needs_15)
std::unordered_map<uint64_t, int> g_noticed;  // (observer, hider, Spot / Listen) -> last result, for the log
int g_log_lines = 0;
constexpr int kNoScore = -100000;
volatile int g_last_score = kNoScore;  // the last detection score (score stubs), for the log

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
  const uint32_t id = IdOf(creature);
  const int roll = D20();
  SetField<uint8_t>(creature, nwmain::kCreatureHideRoll, static_cast<uint8_t>(roll));
  SetField<uint8_t>(creature, nwmain::kCreatureMoveSilentlyRoll, static_cast<uint8_t>(roll));
  g_rolled.insert(id);
  void* stats = Field<void*>(creature, nwmain::kCreatureStats);
  const int skill = stats ? static_cast<int8_t>(g_get_skill_rank(stats, nullptr, kSkillHide, nullptr, 0)) : 0;
  const int total = roll + skill;
  const bool hidden = !g_config.hiding_needs_15 || total >= kHideDc;
  if (hidden) {
    g_not_hidden.erase(id);
  } else {
    g_not_hidden.insert(id);
  }
  if (!IsPlayerControlled(id)) return;
  char line[160];
  if (!g_config.hiding_needs_15) {
    sprintf_s(line, "nwn5e: Stealth %d (d20 %d %+d), kept while you stay hidden", total, roll, skill);
  } else if (hidden) {
    sprintf_s(line, "nwn5e: Stealth %d (d20 %d %+d) - hidden; that roll holds while you stay hidden", total, roll, skill);
  } else {
    sprintf_s(line, "nwn5e: Stealth %d (d20 %d %+d) - below 15, you are not hidden (stop and hide again to retry)",
              total, roll, skill);
  }
  TellPlayer(id, line);
  if (LogSlot()) {
    Log("stealth: 0x%08X %s: Stealth d20 %d %+d = %d%s, kept until it stops hiding", id, when, roll, skill, total,
        g_config.hiding_needs_15 ? (hidden ? " (hidden)" : " (below 15: not hidden)") : "");
  }
}

// What Spot / Listen read as the hider's stealth mode: 0 (not hiding, noticed
// once in range) when its Stealth total missed 15.
int __stdcall SeenStealthMode(void* hider) {
  const uint8_t mode = Field<uint8_t>(hider, nwmain::kCreatureStealthMode);
  return mode == 1 && g_not_hidden.count(IdOf(hider)) ? 0 : mode;
}

// Called in place of `mov al,[edi+0x4A9]` (6 bytes, + 1 nop) at 0x653A05 /
// 0x6535AB.
__declspec(naked) void StealthReadStub() {
  __asm {
    push ecx
    push edx
    push edi
    call SeenStealthMode
    pop edx
    pop ecx
    ret
  }
}

// Search's die (SK5 for traps): a d20 when the searcher searches (detect
// mode), else passive Perception's 10.
int __stdcall SearchDie(void* searcher) {
  return rules::PerceptionDie(Field<uint8_t>(searcher, nwmain::kCreatureDetectMode) != 0, D20());
}

// Called in place of Search's RollDice(1, 10 / 20) (ecx = rules object, two
// stack arguments, ret 8); esi = the searcher.
__declspec(naked) void SearchDieStub() {
  __asm {
    push ecx
    push edx
    push esi
    call SearchDie
    pop edx
    pop ecx
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

void __fastcall SetStealthMode(void* creature, void* edx, uint8_t mode) {
  const bool starts = mode == 1 && Field<uint8_t>(creature, nwmain::kCreatureStealthMode) != 1;
  g_set_stealth_mode(creature, edx, mode);
  if (starts && engine::GetArea(creature)) {
    RollStealth(creature, "starts hiding");
  } else if (starts) {
    // Being placed (a loaded game restores stealth mode before the creature is
    // in its area, when its skills are not the 2024 ones yet): the first 6 s
    // update rolls instead.
    g_rolled.erase(IdOf(creature));
  } else if (mode != 1) {
    g_rolled.erase(IdOf(creature));
    g_not_hidden.erase(IdOf(creature));
  }
}

// The 6 s perception rolls (from PerceptionRollsStub), the 2024 way.
void __stdcall PerceptionRolls(void* creature) {
  const uint32_t id = IdOf(creature);
  if (Field<uint8_t>(creature, nwmain::kCreatureStealthMode) == 1) {
    if (!g_rolled.count(id)) RollStealth(creature, "is hiding without a roll (loaded hidden)");
  } else {
    g_rolled.erase(id);
    g_not_hidden.erase(id);
  }
  // Detect mode is the Search action; with alert_creatures_search a creature in
  // combat searches too (SK5 C).
  const bool searching = Field<uint8_t>(creature, nwmain::kCreatureDetectMode) != 0 ||
                         (g_config.alert_creatures_search && Field<int32_t>(creature, nwmain::kCreatureInCombat) != 0);
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

using AnimationStillFn = int(__fastcall*)(void* object, void* edx, int animation);
const auto g_animation_still = reinterpret_cast<AnimationStillFn>(0x0063A600);
bool rules_still(void* creature) {
  return g_animation_still(creature, nullptr, Field<int32_t>(creature, nwmain::kCreatureAnimation)) != 0;
}

// SK5 B: an observer whose Spot check got as far as its score (it can see:
// not blind, in range) finds a hider in front of it within kUpCloseMetres
// when nothing stands between them.
constexpr float kUpCloseMetres = 2.0f;
constexpr float kUpCloseLogMetres = 3.0f;
std::unordered_map<uint64_t, int> g_up_close_logged;  // (observer, hider) -> last logged outcome
bool FoundUpClose(void* observer, void* hider, float* metres) {
  const float* at = reinterpret_cast<const float*>(static_cast<const uint8_t*>(observer) + nwmain::kObjectPosition);
  const float* to = reinterpret_cast<const float*>(static_cast<const uint8_t*>(hider) + nwmain::kObjectPosition);
  const float* facing = reinterpret_cast<const float*>(static_cast<const uint8_t*>(observer) + nwmain::kObjectFacing);
  const float dx = to[0] - at[0];
  const float dy = to[1] - at[1];
  *metres = sqrtf(dx * dx + dy * dy);
  const bool near = *metres <= kUpCloseMetres;
  const bool in_front = facing[0] * dx + facing[1] * dy > 0.0f;
  const bool clear = near && in_front && engine::ClearLineBetween(observer, hider);
  // Test log (0.9.34): each change of outcome within 3 m for a player's pair.
  if (g_config.log_attacks && *metres <= kUpCloseLogMetres &&
      (IsPlayerControlled(IdOf(hider)) || IsPlayerControlled(IdOf(observer)))) {
    const int outcome = (near ? 1 : 0) | (in_front ? 2 : 0) | (clear ? 4 : 0);
    const uint64_t key = static_cast<uint64_t>(IdOf(observer)) << 32 | IdOf(hider);
    const auto it = g_up_close_logged.find(key);
    if ((it == g_up_close_logged.end() || it->second != outcome) && LogSlot()) {
      g_up_close_logged[key] = outcome;
      Log("perception: up close 0x%08X -> 0x%08X: %.1f m, %s, %s", IdOf(observer), IdOf(hider), *metres,
          in_front ? "in front" : "behind / beside",
          !near || !in_front ? "line not checked" : clear ? "clear line - found" : "line blocked");
    }
  }
  return near && in_front && clear && rules::InFrontWithin(dx, dy, facing[0], facing[1], kUpCloseMetres);
}

// Logs when an observer starts or stops noticing a hiding creature, if either
// is a player's (owner tests; the engine has no log of its own for this).
int Detection(DetectionFn original, const char* sense, void* observer, void* edx, void* target, int flag) {
  g_last_score = kNoScore;
  int result = original(observer, edx, target, flag);
  const int score = g_last_score;
  if (!target || Field<uint8_t>(target, nwmain::kCreatureStealthMode) != 1) return result;
  char up_close[64] = "";
  float metres = 0.0f;
  if (sense[0] == 'S' && !result && score != kNoScore && g_config.found_up_close &&
      FoundUpClose(observer, target, &metres)) {
    result = 1;
    sprintf_s(up_close, "; found up close: %.1f m in front, clear line", metres);
  }
  if (!g_config.log_attacks) return result;
  const uint64_t key = (static_cast<uint64_t>(IdOf(observer)) << 32 | IdOf(target)) ^ (sense[0] == 'S' ? 1ull << 63 : 0);
  const auto it = g_noticed.find(key);
  if (it != g_noticed.end() && it->second == result) return result;
  g_noticed[key] = result;
  if ((IsPlayerControlled(IdOf(target)) || IsPlayerControlled(IdOf(observer))) && LogSlot()) {
    const bool searching = Field<uint8_t>(observer, nwmain::kCreatureDetectMode) != 0 ||
                           (g_config.alert_creatures_search &&
                            Field<int32_t>(observer, nwmain::kCreatureInCombat) != 0);
    char score_text[48];
    if (score == kNoScore) {
      sprintf_s(score_text, "decided before the score");
    } else {
      sprintf_s(score_text, "score %+d", score);
    }
    Log("perception: 0x%08X %s 0x%08X (%s, %s; its Stealth d20 %d; observer %s, die %d, %s%s)", IdOf(observer),
        result ? (sense[0] == 'S' ? "sees" : "hears") : (sense[0] == 'S' ? "does not see" : "does not hear"),
        IdOf(target), sense, score_text, Field<uint8_t>(target, nwmain::kCreatureHideRoll),
        searching ? "searching" : "passive",
        Field<uint8_t>(observer, sense[0] == 'S' ? nwmain::kCreatureSpotRoll : nwmain::kCreatureListenRoll),
        rules_still(observer) ? "still" : "moving", up_close);
  }
  return result;
}

// Called in place of `mov esi,[esi+0xC64]` at the score sites: notes ebx.
__declspec(naked) void ScoreStub() {
  __asm {
    mov dword ptr [g_last_score], ebx
    mov esi, dword ptr [esi + 0xC64]
    ret
  }
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
  int search_dice = 0;
  for (uintptr_t site : nwmain::kSearchDieCalls) {
    search_dice += RedirectCall("perception: Search die - d20 when searching, else passive 10 (faithful)", site,
                                nwmain::kRollDice, reinterpret_cast<void*>(&SearchDieStub))
                       ? 1
                       : 0;
  }
  Log("perception: %d of %u Search dice redirected", search_dice,
      static_cast<unsigned>(sizeof(nwmain::kSearchDieCalls) / sizeof(nwmain::kSearchDieCalls[0])));
  if (g_config.hiding_needs_15) {
    for (const uintptr_t site : {nwmain::kSpotStealthRead, nwmain::kListenStealthRead}) {
      uint8_t call[6] = {0xe8, 0, 0, 0, 0, 0x90};
      const int32_t rel = static_cast<int32_t>(reinterpret_cast<uintptr_t>(&StealthReadStub) -
                                               (site + nwmain::kCallLength));
      memcpy(call + 1, &rel, sizeof(rel));
      PatchBytes(site == nwmain::kSpotStealthRead ? "perception: Spot - a hide below Stealth 15 is no hide (faithful)"
                                                   : "perception: Listen - a hide below Stealth 15 is no hide (faithful)",
                 site, nwmain::kStealthReadBytes, call, sizeof(call));
    }
  }
  if (g_config.log_attacks || g_config.found_up_close) {  // B needs to know the check got to its score
    for (const uintptr_t site : {nwmain::kListenScoreSite, nwmain::kSpotScoreSite}) {
      uint8_t call[6] = {0xe8, 0, 0, 0, 0, 0x90};
      const int32_t rel = static_cast<int32_t>(reinterpret_cast<uintptr_t>(&ScoreStub) - (site + nwmain::kCallLength));
      memcpy(call + 1, &rel, sizeof(rel));
      PatchBytes(site == nwmain::kSpotScoreSite ? "perception: Spot score (log, found up close)"
                                                : "perception: Listen score (log)",
                 site,
                 nwmain::kScoreSiteBytes, call, sizeof(call));
    }
  }
  InstallHook({"Listen detection (log)", nwmain::kListenDetection, nwmain::kListenDetectionPrologue,
               sizeof(nwmain::kListenDetectionPrologue), reinterpret_cast<void*>(&ListenDetection),
               reinterpret_cast<void**>(&g_listen_detection)});
  InstallHook({"Spot detection (log)", nwmain::kSpotDetection, nwmain::kSpotDetectionPrologue,
               sizeof(nwmain::kSpotDetectionPrologue), reinterpret_cast<void*>(&SpotDetection),
               reinterpret_cast<void**>(&g_spot_detection)});
}
