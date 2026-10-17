// Decision 14 (2024 Extra Attack): when an attack kills its target and the
// attacker has attacks left this round, the next attack goes to the nearest
// enemy in reach instead of being lost.
//
// NWN already does this for Great Cleave. ResolvePostMeleeDamage asks
// HasFeat(Great Cleave) after a killing blow (nwmain_1_69.h); that one call is
// redirected here, and answers yes when the attacker really has Great Cleave
// or has attacks left. The engine then finds the nearest enemy in reach and
// adds the attack itself.
//
// One attack moves per kill: exact for two attacks a round; with three or
// four, attacks after a non-killing moved attack are still lost (known gap).
// Melee only — ranged attacks keep NWN's behaviour for now.

#include <stdint.h>

#include "config.h"
#include "hooks.h"
#include "log.h"
#include "log_once.h"
#include "nwmain_1_69.h"
#include "rules/nwn_conversion.h"

namespace {

using HasFeatFn = int(__fastcall*)(void* stats, void* edx, int feat);
using TotalAttacksFn = int(__fastcall*)(void* round, void* edx);

const auto g_has_feat = reinterpret_cast<HasFeatFn>(nwmain::kHasFeat);
const auto g_get_total_attacks = reinterpret_cast<TotalAttacksFn>(nwmain::kGetTotalAttacks);

LogOnce g_retarget_logged(500);

template <typename T>
T Field(const void* object, uintptr_t offset) {
  return *reinterpret_cast<const T*>(static_cast<const uint8_t*>(object) + offset);
}

int __fastcall HasGreatCleaveOrAttacksLeft(void* stats, void* edx, int feat) {
  const int has_feat = g_has_feat(stats, edx, feat);
  if (has_feat || !g_config.retarget_remaining_attacks) return has_feat;
  const void* creature = Field<void*>(stats, nwmain::kStatsCreature);
  if (!creature) return has_feat;
  void* round = Field<void*>(creature, nwmain::kCreatureCombatRound);
  if (!round) return has_feat;
  const int total = g_get_total_attacks(round, nullptr);
  const int index = Field<uint8_t>(round, nwmain::kRoundCurrentAttack);
  const int remaining = rules::RemainingAttacksInRound(total, index);
  if (remaining <= 0) return has_feat;
  const uint32_t id = Field<uint32_t>(creature, nwmain::kCreatureObjectId);
  if (g_config.log_attacks && g_retarget_logged.First(LogOnce::Key(id, index, total))) {
    Log("retarget: object 0x%08X killed its target with attack %d of %d; next attack moves to the nearest enemy",
        id, index + 1, total);
  }
  return 1;
}

}  // namespace

void InstallRetargetRules() {
  if (!g_config.retarget_remaining_attacks) return;
  const uintptr_t next_instruction = nwmain::kGreatCleaveCheckCall + nwmain::kCallLength;
  const int32_t rel = static_cast<int32_t>(reinterpret_cast<uintptr_t>(&HasGreatCleaveOrAttacksLeft) - next_instruction);
  uint8_t call[sizeof(nwmain::kGreatCleaveCheckCallBytes)] = {0xe8};
  for (int i = 0; i < 4; ++i) call[1 + i] = static_cast<uint8_t>(static_cast<uint32_t>(rel) >> (8 * i));
  PatchBytes("ResolvePostMeleeDamage Great Cleave check (remaining attacks move on)", nwmain::kGreatCleaveCheckCall,
             nwmain::kGreatCleaveCheckCallBytes, call, sizeof(call));
}
