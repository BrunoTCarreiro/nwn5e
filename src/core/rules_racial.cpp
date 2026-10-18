// Decision R1 (PLAN.md): in faithful mode NWN's racial combat bonuses against
// specific enemies are removed — Battle Training (+1 to hit vs orcs,
// goblinoids, reptilians), Good Aim (+1 with thrown weapons), the halfling
// sling bonus (+1) and Defensive Training (+4 AC vs giants). 2024 species
// have no such bonuses. Each function returns what the engine added, by the
// engine's own checks (nwmain_1_69.h), so the hooks can take it back out; in
// NWN mode (or off) they return 0.

#include <stdint.h>

#include "config.h"
#include "creature.h"
#include "nwmain_1_69.h"

namespace {

using HasFeatFn = int(__fastcall*)(void* stats, void* edx, int feat);
using GetAttackFn = void*(__fastcall*)(void* round, void* edx, int index);
using GetItemFn = void*(__fastcall*)(void* owner, void* edx, int which);
using GetBaseItemFn = void*(__fastcall*)(void* base_items, void* edx, int row);

const auto g_has_feat = reinterpret_cast<HasFeatFn>(nwmain::kHasFeat);
const auto g_get_attack = reinterpret_cast<GetAttackFn>(nwmain::kGetAttack);
const auto g_get_item_in_slot = reinterpret_cast<GetItemFn>(nwmain::kGetItemInSlot);
const auto g_get_current_attack_weapon = reinterpret_cast<GetItemFn>(nwmain::kGetCurrentAttackWeapon);
const auto g_get_base_item = reinterpret_cast<GetBaseItemFn>(nwmain::kGetBaseItem);

bool Faithful() { return g_config.racial_combat_bonuses == rules::RuleVariant::kFaithful; }

bool HasFeat(void* stats, int feat) { return stats && g_has_feat(stats, nullptr, feat) != 0; }

int RaceOf(const void* stats) { return stats ? Field<uint16_t>(stats, nwmain::kStatsRace) : -1; }

const void* BaseItemOf(const void* item) {
  if (!item) return nullptr;
  void* rules_object = *reinterpret_cast<void**>(nwmain::kRulesObject);
  void* base_items = rules_object ? Field<void*>(rules_object, nwmain::kRulesBaseItems) : nullptr;
  return base_items ? g_get_base_item(base_items, nullptr, Field<int32_t>(item, nwmain::kItemBaseItem)) : nullptr;
}

void* RightHandItem(void* creature) {
  void* inventory = Field<void*>(creature, nwmain::kCreatureInventory);
  return inventory ? g_get_item_in_slot(inventory, nullptr, nwmain::kSlotRightHand) : nullptr;
}

bool IsThrown(const void* item) {
  const void* base = BaseItemOf(item);
  return base && Field<uint8_t>(base, nwmain::kBaseItemWield) == nwmain::kWieldThrown;
}

bool IsSling(const void* item) { return item && Field<int32_t>(item, nwmain::kItemBaseItem) == nwmain::kBaseItemSling; }

}  // namespace

// GetAttackModifierVersus(stats, target): Battle Training, Good Aim, sling.
int RacialAttackBonusToRemove(void* stats, void* target) {
  if (!Faithful() || !stats) return 0;
  void* creature = Field<void*>(stats, nwmain::kStatsCreature);
  if (!creature) return 0;
  int bonus = 0;
  if (target) {
    const int race = RaceOf(Field<void*>(target, nwmain::kCreatureStats));
    if ((HasFeat(stats, nwmain::kFeatBattleTrainingGoblins) && race == nwmain::kRaceGoblinoid) ||
        (HasFeat(stats, nwmain::kFeatBattleTrainingOrcs) && race == nwmain::kRaceOrc) ||
        (HasFeat(stats, nwmain::kFeatBattleTrainingReptilians) && race == nwmain::kRaceReptilian)) {
      bonus += 1;
    }
  }
  void* round = Field<void*>(creature, nwmain::kCreatureCombatRound);
  const void* attack = round ? g_get_attack(round, nullptr, Field<uint8_t>(round, nwmain::kRoundCurrentAttack)) : nullptr;
  if (attack && Field<int32_t>(attack, nwmain::kAttackDataRanged) != 0) {
    if (HasFeat(stats, nwmain::kFeatGoodAim) &&
        IsThrown(g_get_current_attack_weapon(round, nullptr, Field<uint8_t>(attack, nwmain::kAttackDataType)))) {
      bonus += 1;
    }
    if (RaceOf(stats) == nwmain::kRaceHalfling && IsSling(RightHandItem(creature))) bonus += 1;
  }
  return bonus;
}

// The sheet's ranged attack bonus (full path): Good Aim and the sling bonus,
// read from the right-hand item as the sheet does.
int RacialSheetRangedBonusToRemove(void* stats) {
  if (!Faithful() || !stats) return 0;
  void* creature = Field<void*>(stats, nwmain::kStatsCreature);
  if (!creature) return 0;
  void* item = RightHandItem(creature);
  int bonus = 0;
  if (HasFeat(stats, nwmain::kFeatGoodAim) && IsThrown(item)) bonus += 1;
  if (RaceOf(stats) == nwmain::kRaceHalfling && IsSling(item)) bonus += 1;
  return bonus;
}

// GetArmorClassVersus(defender stats, attacker): Defensive Training.
int RacialArmorBonusToRemove(void* defender_stats, void* attacker) {
  if (!Faithful() || !defender_stats || !attacker) return 0;
  const int attacker_race = RaceOf(Field<void*>(attacker, nwmain::kCreatureStats));
  return HasFeat(defender_stats, nwmain::kFeatDefensiveTrainingGiants) && attacker_race == nwmain::kRaceGiant ? 4 : 0;
}
