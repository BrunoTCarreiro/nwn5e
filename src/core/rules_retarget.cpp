// Decision 14 (2024 Extra Attack): when an attack kills its target and the
// attacker has attacks left this round, the remaining attacks go to the
// nearest enemy in reach instead of being lost.
//
// The round's target (combat round +0x2134) is switched to the nearest enemy
// — the same lookup the engine's Great Cleave uses — without adding an attack.
//
// - Melee: after a killing blow ResolvePostMeleeDamage asks HasFeat(Great
//   Cleave) (nwmain_1_69.h); that call is redirected here, does the switch,
//   and returns the real HasFeat answer, so creatures with Great Cleave or
//   Cleave still get their bonus attack as in NWN.
// - Ranged: ResolvePostRangedDamage is hooked; after it marks the attack as a
//   kill (attack data +0x40), the switch uses the weapon's long range.

#include <stdint.h>

#include "config.h"
#include "creature.h"
#include "hooks.h"
#include "log.h"
#include "log_once.h"
#include "nwmain_1_69.h"
#include "rules/nwn_conversion.h"

namespace {

using HasFeatFn = int(__fastcall*)(void* stats, void* edx, int feat);
using TotalAttacksFn = int(__fastcall*)(void* round, void* edx);
using GetAttackFn = void*(__fastcall*)(void* round, void* edx, int index);
using MaxAttackRangeFn = float(__fastcall*)(void* creature, void* edx, uint32_t target, int a, int b);
using GetNearestEnemyFn = uint32_t(__fastcall*)(void* creature, void* edx, float range, uint32_t exclude, int a,
                                                int b);
using GetCreatureFn = void*(__fastcall*)(void* server_internal, void* edx, uint32_t id);
using ResolvePostRangedDamageFn = void(__fastcall*)(void* creature, void* edx, void* target);

const auto g_has_feat = reinterpret_cast<HasFeatFn>(nwmain::kHasFeat);
const auto g_get_total_attacks = reinterpret_cast<TotalAttacksFn>(nwmain::kGetTotalAttacks);
const auto g_get_attack = reinterpret_cast<GetAttackFn>(nwmain::kGetAttack);
const auto g_max_attack_range = reinterpret_cast<MaxAttackRangeFn>(nwmain::kMaxAttackRange);
const auto g_get_nearest_enemy = reinterpret_cast<GetNearestEnemyFn>(nwmain::kGetNearestEnemy);
const auto g_get_creature = reinterpret_cast<GetCreatureFn>(nwmain::kGetCreatureByGameObjectId);
ResolvePostRangedDamageFn g_resolve_post_ranged_damage = nullptr;

LogOnce g_retarget_logged(500);

// Switches the round's target to the nearest enemy within the attacker's
// range. Returns the new target's id, or kObjectInvalid when there is none.
uint32_t SwitchToNearestEnemy(void* attacker, void* round, uint32_t killed_id, int range_mode) {
  const float range = g_max_attack_range(attacker, nullptr, nwmain::kObjectInvalid, 0, range_mode);
  const uint32_t enemy = g_get_nearest_enemy(attacker, nullptr, range, killed_id, 1, 0);
  void* server_internal = Field<void*>(*reinterpret_cast<void**>(nwmain::kServerExoAppInternalHolder), 4);
  if (enemy == nwmain::kObjectInvalid || !server_internal || !g_get_creature(server_internal, nullptr, enemy)) {
    return nwmain::kObjectInvalid;
  }
  SetField<uint32_t>(round, nwmain::kRoundTarget, enemy);
  SetField<uint32_t>(attacker, nwmain::kCreatureTargetSwitched, 1);
  return enemy;
}

// After the current attack killed its target: if attacks are left, move the
// rest of the round to the nearest enemy. `ranged` only changes the range.
void RetargetAfterKill(void* attacker, bool ranged) {
  void* round = Field<void*>(attacker, nwmain::kCreatureCombatRound);
  if (!round) return;
  const int total = g_get_total_attacks(round, nullptr);
  const int index = Field<uint8_t>(round, nwmain::kRoundCurrentAttack);
  if (rules::RemainingAttacksInRound(total, index) <= 0) return;
  const void* attack = g_get_attack(round, nullptr, index);
  if (!attack) return;
  const uint32_t killed = Field<uint32_t>(attack, nwmain::kAttackDataTarget);
  const uint32_t next = SwitchToNearestEnemy(attacker, round, killed,
                                             ranged ? nwmain::kMaxAttackRangeRanged : nwmain::kMaxAttackRangeMelee);
  const uint32_t id = Field<uint32_t>(attacker, nwmain::kCreatureObjectId);
  if (g_config.log_attacks && g_retarget_logged.First(LogOnce::Key(id ^ killed, index, total))) {
    if (next != nwmain::kObjectInvalid) {
      Log("retarget (%s): object 0x%08X killed 0x%08X with attack %d of %d; remaining attacks go to 0x%08X",
          ranged ? "ranged" : "melee", id, killed, index + 1, total, next);
    } else {
      Log("retarget (%s): object 0x%08X killed 0x%08X with attack %d of %d; no enemy in range",
          ranged ? "ranged" : "melee", id, killed, index + 1, total);
    }
  }
}

// Replaces the HasFeat(Great Cleave) call made after a melee killing blow.
int __fastcall HasGreatCleaveOrRetarget(void* stats, void* edx, int feat) {
  const int has_feat = g_has_feat(stats, edx, feat);
  if (!has_feat) {
    void* attacker = Field<void*>(stats, nwmain::kStatsCreature);
    if (attacker) RetargetAfterKill(attacker, false);
  }
  return has_feat;
}

void __fastcall ResolvePostRangedDamage(void* creature, void* edx, void* target) {
  g_resolve_post_ranged_damage(creature, edx, target);
  void* round = Field<void*>(creature, nwmain::kCreatureCombatRound);
  if (!round) return;
  const void* attack = g_get_attack(round, nullptr, Field<uint8_t>(round, nwmain::kRoundCurrentAttack));
  if (attack && Field<int32_t>(attack, nwmain::kAttackDataKilledTarget) == 1) RetargetAfterKill(creature, true);
}

}  // namespace

void InstallRetargetRules() {
  if (!g_config.retarget_remaining_attacks) return;
  const uintptr_t next_instruction = nwmain::kGreatCleaveCheckCall + nwmain::kCallLength;
  const int32_t rel = static_cast<int32_t>(reinterpret_cast<uintptr_t>(&HasGreatCleaveOrRetarget) - next_instruction);
  uint8_t call[sizeof(nwmain::kGreatCleaveCheckCallBytes)] = {0xe8};
  for (int i = 0; i < 4; ++i) call[1 + i] = static_cast<uint8_t>(static_cast<uint32_t>(rel) >> (8 * i));
  PatchBytes("ResolvePostMeleeDamage Great Cleave check (remaining attacks move on)", nwmain::kGreatCleaveCheckCall,
             nwmain::kGreatCleaveCheckCallBytes, call, sizeof(call));
  InstallHook({"CNWSCreature::ResolvePostRangedDamage (remaining attacks move on)", nwmain::kResolvePostRangedDamage,
               nwmain::kResolvePostRangedDamagePrologue, sizeof(nwmain::kResolvePostRangedDamagePrologue),
               reinterpret_cast<void*>(&ResolvePostRangedDamage),
               reinterpret_cast<void**>(&g_resolve_post_ranged_damage)});
}
