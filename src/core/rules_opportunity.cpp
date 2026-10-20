// Decision D2 (PLAN.md): opportunity attacks. In 2024 only leaving a
// creature's reach provokes, once per round (one reaction). NWN already gives
// each creature one AoO per round (nwmain_1_69.h) and has no Combat Reflexes,
// but it also provokes on casting, drinking potions, ranged shots, unarmed
// attacks without Improved Unarmed Strike and Disarm, and Spring Attack
// shields movement. Faithful turns those off; moving away still provokes and Tumble still
// avoids it (until Phase 6 adds Disengage). NWN mode leaves NWN's rules.
// NWN gives the AoO to every enemy a running creature passes near, also when
// it runs toward them (a goblin charging back after a Push); faithful lets
// only those it is running away from take it (its facing, the way it runs,
// points away from them).

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "config.h"
#include "creature.h"
#include "engine_queries.h"
#include "hooks.h"
#include "log.h"
#include "nwmain_1_69.h"

namespace {

using GetReactionToFn = uint8_t(__fastcall*)(void* creature, void* edx, uint32_t id);
const auto g_get_reaction_to = reinterpret_cast<GetReactionToFn>(nwmain::kGetReactionTo);
int g_aoo_log_lines = 0;

struct Vector {
  float x, y, z;
};

bool Running(int animation) { return animation == 0x04 || animation == 0x56 || animation == 0x57; }

// Stands in for GetReactionTo at the AoO broadcast's enemy check (0x6560F2):
// an enemy the running provoker is not moving away from does not take the
// AoO. Anything but running (other provocations) keeps NWN's answer.
uint8_t __fastcall AooReaction(void* provoker, void* edx, uint32_t candidate_id) {
  const uint8_t reaction = g_get_reaction_to(provoker, edx, candidate_id);
  if (reaction != nwmain::kReactionEnemy || !Running(Field<int32_t>(provoker, nwmain::kCreatureAnimation))) {
    return reaction;
  }
  // Fighter F7 Tactical Shift: after Second Wind, moving provokes nothing this
  // round (scripts/nwn5e_secwind.nss sets the variable for 6 s).
  if (engine::GetLocalInt(provoker, "nwn5e_tactical_shift")) {
    if (g_config.log_attacks && g_aoo_log_lines < 100) {
      ++g_aoo_log_lines;
      Log("opportunity attacks: 0x%08X Tactical Shift - no AoO from 0x%08X",
          Field<uint32_t>(provoker, nwmain::kCreatureObjectId), candidate_id);
    }
    return 1;
  }
  void* candidate = engine::GetCreature(candidate_id);
  if (!candidate) return reaction;
  const Vector at = Field<Vector>(provoker, nwmain::kObjectPosition);
  const Vector from = Field<Vector>(candidate, nwmain::kObjectPosition);
  const Vector facing = Field<Vector>(provoker, nwmain::kObjectFacing);
  const float dx = at.x - from.x, dy = at.y - from.y;
  const float away = dx * facing.x + dy * facing.y;  // > 0: running away from the candidate
  if (away > 0.0f) return reaction;
  if (g_config.log_attacks && g_aoo_log_lines < 100) {
    ++g_aoo_log_lines;
    Log("opportunity attacks: 0x%08X runs toward 0x%08X, not away: no AoO (faithful)",
        Field<uint32_t>(provoker, nwmain::kCreatureObjectId), candidate_id);
  }
  return 1;  // not an enemy, for this check only
}

bool PatchAooReactionCall() {
  const auto* code = reinterpret_cast<const uint8_t*>(nwmain::kAooReactionCall);
  int32_t rel = 0;
  memcpy(&rel, code + 1, sizeof(rel));
  if (code[0] != 0xe8 || nwmain::kAooReactionCall + nwmain::kCallLength + rel != nwmain::kGetReactionTo) {
    Log("SKIP opportunity attacks: 0x%08X is not the mapped GetReactionTo call", nwmain::kAooReactionCall);
    return false;
  }
  uint8_t expected[5];
  memcpy(expected, code, sizeof(expected));
  const int32_t new_rel = static_cast<int32_t>(reinterpret_cast<uintptr_t>(&AooReaction) -
                                               (nwmain::kAooReactionCall + nwmain::kCallLength));
  uint8_t call[5] = {0xe8};
  memcpy(call + 1, &new_rel, sizeof(new_rel));
  return PatchBytes("opportunity attacks: only moving away provokes (faithful)", nwmain::kAooReactionCall, expected,
                    call, sizeof(call));
}

}  // namespace

void InstallOpportunityRules() {
  if (g_config.opportunity_attacks != rules::RuleVariant::kFaithful) return;
  PatchAooReactionCall();
  int applied = 0;
  for (const nwmain::SmallPatch& patch : nwmain::kFaithfulOpportunityPatches) {
    char name[96];
    sprintf_s(name, "opportunity attacks: %s (faithful)", patch.what);
    applied += PatchBytes(name, patch.address, patch.original, patch.patched, patch.size) ? 1 : 0;
  }
  Log("opportunity attacks: %d of %u faithful patches applied", applied,
      static_cast<unsigned>(sizeof(nwmain::kFaithfulOpportunityPatches) /
                            sizeof(nwmain::kFaithfulOpportunityPatches[0])));
}
