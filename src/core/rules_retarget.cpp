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
//   kill (attack data +0x40), the switch runs within 20 m (the engine's cleave
//   range for ranged weapons, close to a bow's normal range), and only to an
//   enemy already in the fight — one whose attack order targets the attacker
//   or the attacker's side (owner, 2026-10-17: a bow must not pick an idle,
//   caged or sleeping enemy). GetNearestEnemy can't filter that, so the area
//   is walked here with GetNearestEnemy's own checks (nwmain_1_69.h).

#include <math.h>
#include <stdint.h>

#include <vector>

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
using GetAreaFn = void*(__fastcall*)(void* object, void* edx);
using GetReactionToFn = uint8_t(__fastcall*)(void* creature, void* edx, uint32_t id);
using CreatureCheckFn = int(__fastcall*)(void* creature, void* edx);
using GetVisibleListElementFn = void*(__fastcall*)(void* creature, void* edx, uint32_t id);

const auto g_has_feat = reinterpret_cast<HasFeatFn>(nwmain::kHasFeat);
const auto g_get_total_attacks = reinterpret_cast<TotalAttacksFn>(nwmain::kGetTotalAttacks);
const auto g_get_attack = reinterpret_cast<GetAttackFn>(nwmain::kGetAttack);
const auto g_max_attack_range = reinterpret_cast<MaxAttackRangeFn>(nwmain::kMaxAttackRange);
const auto g_get_nearest_enemy = reinterpret_cast<GetNearestEnemyFn>(nwmain::kGetNearestEnemy);
const auto g_get_creature = reinterpret_cast<GetCreatureFn>(nwmain::kGetCreatureByGameObjectId);
const auto g_get_area = reinterpret_cast<GetAreaFn>(nwmain::kGetArea);
const auto g_get_reaction_to = reinterpret_cast<GetReactionToFn>(nwmain::kGetReactionTo);
const auto g_get_dead = reinterpret_cast<CreatureCheckFn>(nwmain::kGetDead);
const auto g_get_dying = reinterpret_cast<CreatureCheckFn>(nwmain::kGetDying);
const auto g_get_visible_list_element = reinterpret_cast<GetVisibleListElementFn>(nwmain::kGetVisibleListElement);
ResolvePostRangedDamageFn g_resolve_post_ranged_damage = nullptr;

LogOnce g_retarget_logged(500);

void* ServerInternal() {
  return Field<void*>(*reinterpret_cast<void**>(nwmain::kServerExoAppInternalHolder), 4);
}

int FactionOf(const void* creature) {
  const void* stats = Field<void*>(creature, nwmain::kCreatureStats);
  return stats ? Field<int32_t>(stats, nwmain::kStatsFaction) : -1;
}

float PersonalSpace(const void* creature) {
  const void* info = Field<void*>(creature, nwmain::kCreaturePersonalSpaceInfo);
  return info ? Field<float>(info, nwmain::kPersonalSpace) : 0.0f;
}

// Edge-to-edge distance, as GetNearestEnemy measures it.
float EdgeDistance(const void* a, const void* b) {
  const float dx = Field<float>(a, nwmain::kObjectPosition) - Field<float>(b, nwmain::kObjectPosition);
  const float dy = Field<float>(a, nwmain::kObjectPosition + 4) - Field<float>(b, nwmain::kObjectPosition + 4);
  const float dz = Field<float>(a, nwmain::kObjectPosition + 8) - Field<float>(b, nwmain::kObjectPosition + 8);
  return sqrtf(dx * dx + dy * dy + dz * dz) - PersonalSpace(a) - PersonalSpace(b);
}

// The creature's current or first queued attack-object action, and who that
// target is (its master and faction).
rules::AttackOrder ReadAttackOrder(void* server_internal, const void* creature) {
  rules::AttackOrder order{false, nwmain::kObjectInvalid, nwmain::kObjectInvalid, -1};
  if (Field<int16_t>(creature, nwmain::kObjectCurrentAction) == nwmain::kActionAttackObject) {
    order.exists = true;
    order.target = Field<uint32_t>(creature, nwmain::kObjectCurrentActionTarget);
  } else if (const void* queue = Field<void*>(creature, nwmain::kObjectActionQueue)) {
    const void* node = Field<void*>(queue, nwmain::kListHead);
    for (int guard = 0; node && guard < 64; ++guard, node = Field<void*>(node, nwmain::kListNodeNext)) {
      const void* action = Field<void*>(node, nwmain::kListNodeData);
      if (action && Field<int32_t>(action, nwmain::kActionId) == nwmain::kActionAttackObject) {
        order.exists = true;
        order.target = Field<uint32_t>(action, nwmain::kActionFirstParam);
        break;
      }
    }
  }
  if (order.exists && order.target != nwmain::kObjectInvalid) {
    if (const void* target = g_get_creature(server_internal, nullptr, order.target)) {
      order.target_master = Field<uint32_t>(target, nwmain::kCreatureMaster);
      order.target_faction = FactionOf(target);
    }
  }
  return order;
}

struct RangedSearch {
  int seen_in_range = 0;  // living enemies the attacker has seen, under the range
  int in_the_fight = 0;   // of those, the ones attacking the attacker's side
};

// GetNearestEnemy(range, exclude, bMustBeSeen = 1, bCheckPath = 0) plus the
// "in the fight" filter (decision 14, ranged).
uint32_t NearestEnemyInTheFight(void* attacker, float range, uint32_t exclude, RangedSearch* search) {
  void* server_internal = ServerInternal();
  const void* area = g_get_area(attacker, nullptr);
  if (!server_internal || !area) return nwmain::kObjectInvalid;
  const uint32_t* ids = Field<const uint32_t*>(area, nwmain::kAreaObjectIds);
  const int count = Field<int32_t>(area, nwmain::kAreaObjectCount);
  if (!ids || count <= 0) return nwmain::kObjectInvalid;
  const uint32_t self = Field<uint32_t>(attacker, nwmain::kCreatureObjectId);
  const int faction = FactionOf(attacker);
  std::vector<rules::RetargetCandidate> candidates;
  std::vector<uint32_t> candidate_ids;
  for (int i = 0; i < count; ++i) {
    const uint32_t id = ids[i];
    if (id == self || id == exclude) continue;
    void* creature = g_get_creature(server_internal, nullptr, id);
    if (!creature || g_get_reaction_to(attacker, nullptr, id) != nwmain::kReactionEnemy) continue;
    if (g_get_dead(creature, nullptr) || g_get_dying(creature, nullptr)) continue;
    const void* seen = g_get_visible_list_element(attacker, nullptr, id);
    if (!seen || (Field<uint8_t>(seen, nwmain::kVisibleElementFlags) & 1) == 0) continue;
    const float distance = EdgeDistance(attacker, creature);
    if (!(distance < range)) continue;
    const bool fighting = rules::IsAgainstSide(ReadAttackOrder(server_internal, creature), self, faction);
    candidates.push_back({distance, fighting});
    candidate_ids.push_back(id);
    ++search->seen_in_range;
    if (fighting) ++search->in_the_fight;
  }
  const int best = rules::NearestEnemyInTheFight(candidates.data(), static_cast<int>(candidates.size()), range);
  return best < 0 ? nwmain::kObjectInvalid : candidate_ids[best];
}

// Switches the round's target to the nearest enemy within the attacker's
// range (melee reach, or 20 m and in the fight with a ranged weapon). Returns
// the new target's id, or kObjectInvalid when there is none.
uint32_t SwitchToNearestEnemy(void* attacker, void* round, uint32_t killed_id, bool ranged, RangedSearch* search) {
  const float range =
      g_max_attack_range(attacker, nullptr, nwmain::kObjectInvalid, 0, nwmain::kMaxAttackRangeRetarget);
  const uint32_t enemy = ranged ? NearestEnemyInTheFight(attacker, range, killed_id, search)
                                : g_get_nearest_enemy(attacker, nullptr, range, killed_id, 1, 0);
  void* server_internal = ServerInternal();
  if (enemy == nwmain::kObjectInvalid || !server_internal || !g_get_creature(server_internal, nullptr, enemy)) {
    return nwmain::kObjectInvalid;
  }
  SetField<uint32_t>(round, nwmain::kRoundTarget, enemy);
  SetField<uint32_t>(attacker, nwmain::kCreatureTargetSwitched, 1);
  return enemy;
}

// After the current attack killed its target: if attacks are left, move the
// rest of the round to the nearest enemy (for `ranged`, within 20 m and in the
// fight).
void RetargetAfterKill(void* attacker, bool ranged) {
  void* round = Field<void*>(attacker, nwmain::kCreatureCombatRound);
  if (!round) return;
  const int total = g_get_total_attacks(round, nullptr);
  const int index = Field<uint8_t>(round, nwmain::kRoundCurrentAttack);
  if (rules::RemainingAttacksInRound(total, index) <= 0) return;
  const void* attack = g_get_attack(round, nullptr, index);
  if (!attack) return;
  const uint32_t killed = Field<uint32_t>(attack, nwmain::kAttackDataTarget);
  RangedSearch search;
  const uint32_t next = SwitchToNearestEnemy(attacker, round, killed, ranged, &search);
  const uint32_t id = Field<uint32_t>(attacker, nwmain::kCreatureObjectId);
  if (g_config.log_attacks && g_retarget_logged.First(LogOnce::Key(id ^ killed, index, total))) {
    if (ranged && next != nwmain::kObjectInvalid) {
      Log("retarget (ranged): object 0x%08X killed 0x%08X with attack %d of %d; %d seen enemies within range, %d in "
          "the fight; remaining attacks go to 0x%08X",
          id, killed, index + 1, total, search.seen_in_range, search.in_the_fight, next);
    } else if (ranged) {
      Log("retarget (ranged): object 0x%08X killed 0x%08X with attack %d of %d; %d seen enemies within range, none "
          "in the fight",
          id, killed, index + 1, total, search.seen_in_range);
    } else if (next != nwmain::kObjectInvalid) {
      Log("retarget (melee): object 0x%08X killed 0x%08X with attack %d of %d; remaining attacks go to 0x%08X", id,
          killed, index + 1, total, next);
    } else {
      Log("retarget (melee): object 0x%08X killed 0x%08X with attack %d of %d; no enemy in range", id, killed,
          index + 1, total);
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
