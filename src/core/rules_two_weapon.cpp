// Decision D1 (PLAN.md): two-weapon fighting. Faithful: no attack penalty on
// either hand; one off-hand attack per round, only with two Light weapons, a
// double weapon, or Improved Two-Weapon Fighting as Dual Wielder (any weapons
// NWN allows in both hands); the off-hand attack adds no Strength to damage
// unless it is negative, or with Two-Weapon Fighting (the 2024 fighting
// style); Ambidexterity does nothing. NWN mode keeps NWN's rules. Engine
// details in nwmain_1_69.h.

#include <stdint.h>

#include "config.h"
#include "creature.h"
#include "hooks.h"
#include "log.h"
#include "log_once.h"
#include "mastery.h"
#include "nwmain_1_69.h"
#include "rules/weapons.h"

namespace {

using GetOffHandAttacksFn = int(__fastcall*)(void* round, void* edx);
using GetDamageBonusFn = int(__fastcall*)(void* stats, void* edx, void* target, int off_hand);
using HasFeatFn = int(__fastcall*)(void* stats, void* edx, int feat);
using GetItemFn = void*(__fastcall*)(void* owner, void* edx, int which);
using GetBaseItemFn = void*(__fastcall*)(void* base_items, void* edx, int row);

GetOffHandAttacksFn g_get_off_hand_attacks = nullptr;
GetDamageBonusFn g_get_damage_bonus = nullptr;
const auto g_has_feat = reinterpret_cast<HasFeatFn>(nwmain::kHasFeat);
const auto g_get_item_in_slot = reinterpret_cast<GetItemFn>(nwmain::kGetItemInSlot);
const auto g_get_base_item = reinterpret_cast<GetBaseItemFn>(nwmain::kGetBaseItem);

LogOnce g_logged_attacks(512);
LogOnce g_logged_damage(512);

bool HasFeat(void* stats, int feat) { return stats && g_has_feat(stats, nullptr, feat) != 0; }

// baseitems.2da row of the item in `slot`, or -1 when the slot is empty.
int BaseItemRowInSlot(void* creature, int slot) {
  void* inventory = Field<void*>(creature, nwmain::kCreatureInventory);
  void* item = inventory ? g_get_item_in_slot(inventory, nullptr, slot) : nullptr;
  return item ? Field<int32_t>(item, nwmain::kItemBaseItem) : -1;
}

bool IsDoubleWeapon(int row) {
  if (row < 0) return false;
  void* rules_object = *reinterpret_cast<void**>(nwmain::kRulesObject);
  void* base_items = rules_object ? Field<void*>(rules_object, nwmain::kRulesBaseItems) : nullptr;
  void* base_item = base_items ? g_get_base_item(base_items, nullptr, row) : nullptr;
  return base_item && Field<uint8_t>(base_item, nwmain::kBaseItemWield) == nwmain::kBaseItemWieldDouble;
}

uint32_t ObjectIdOfStats(void* stats) {
  void* creature = Field<void*>(stats, nwmain::kStatsCreature);
  return creature ? Field<uint32_t>(creature, nwmain::kCreatureObjectId) : 0;
}

int __fastcall GetOffHandAttacks(void* round, void* edx) {
  const int nwn_attacks = g_get_off_hand_attacks(round, edx);
  if (nwn_attacks <= 0) return nwn_attacks;
  void* creature = Field<void*>(round, nwmain::kRoundOwner);
  void* stats = creature ? Field<void*>(creature, nwmain::kCreatureStats) : nullptr;
  if (!stats) return nwn_attacks;
  const int main_row = BaseItemRowInSlot(creature, nwmain::kSlotRightHand);
  const int off_row = BaseItemRowInSlot(creature, nwmain::kSlotLeftHand);
  const bool double_weapon = IsDoubleWeapon(main_row);
  const bool dual_wielder = HasFeat(stats, nwmain::kFeatImprovedTwoWeaponFighting);
  const int attacks = rules::OffHandAttacks(nwn_attacks, double_weapon, dual_wielder,
                                            rules::IsLightNwnBaseItem(main_row), rules::IsLightNwnBaseItem(off_row));
  SetField<int32_t>(round, nwmain::kRoundOffHandAttacks, attacks);
  const uint32_t id = Field<uint32_t>(creature, nwmain::kCreatureObjectId);
  if (g_config.log_attacks &&
      g_logged_attacks.First(LogOnce::Key(id, (nwn_attacks << 8) | attacks, ((main_row & 0xFF) << 8) | (off_row & 0xFF)))) {
    Log("two-weapon: object 0x%08X off-hand attacks 3e %d -> 5e %d (base items %d / %d%s%s)", id, nwn_attacks,
        attacks, main_row, off_row, double_weapon ? ", double weapon" : "", dual_wielder ? ", Dual Wielder" : "");
  }
  return attacks;
}

int __fastcall GetDamageBonus(void* stats, void* edx, void* target, int off_hand) {
  const int bonus = g_get_damage_bonus(stats, edx, target, off_hand);
  if (!off_hand) return MasteryCleaveDamage(stats, bonus);  // D4 Cleave's extra attack
  if (g_config.two_weapon_fighting != rules::RuleVariant::kFaithful) return bonus;
  const int strength = Field<int8_t>(stats, nwmain::kStatsStrModifier);
  const int nwn_term = rules::NwnOffHandStrengthDamage(strength);
  const bool style = HasFeat(stats, nwmain::kFeatTwoWeaponFighting);
  const int term = rules::OffHandDamageAbilityModifier(strength, style);
  const uint32_t id = ObjectIdOfStats(stats);
  if (g_config.log_attacks && g_logged_damage.First(LogOnce::Key(id, nwn_term, term))) {
    Log("two-weapon: object 0x%08X off-hand Strength damage 3e %+d -> 5e %+d (Str %+d%s)", id, nwn_term, term,
        strength, style ? ", Two-Weapon Fighting style" : "");
  }
  return bonus - nwn_term + term;
}

}  // namespace

void InstallTwoWeaponRules() {
  const bool two_weapon = g_config.two_weapon_fighting == rules::RuleVariant::kFaithful;
  const bool mastery = g_config.weapon_mastery != rules::RuleVariant::kOff;  // D4 Cleave uses the damage hook
  if (two_weapon) {
    InstallHook({"CNWSCombatRound off-hand attacks (two-weapon fighting)", nwmain::kGetOffHandAttacks,
                 nwmain::kGetOffHandAttacksPrologue, sizeof(nwmain::kGetOffHandAttacksPrologue),
                 reinterpret_cast<void*>(&GetOffHandAttacks), reinterpret_cast<void**>(&g_get_off_hand_attacks)});
    for (const nwmain::SmallPatch& patch : nwmain::kFaithfulTwoWeaponPatches) {
      PatchBytes(patch.what, patch.address, patch.original, patch.patched, patch.size);
    }
  }
  if (two_weapon || mastery) {
    InstallHook({"CNWSCreatureStats damage bonus (off-hand Strength, Cleave)", nwmain::kGetDamageBonus,
                 nwmain::kGetDamageBonusPrologue, sizeof(nwmain::kGetDamageBonusPrologue),
                 reinterpret_cast<void*>(&GetDamageBonus), reinterpret_cast<void**>(&g_get_damage_bonus)});
  }
}
