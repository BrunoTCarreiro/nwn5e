// Fighter features built from its brief (PLAN.md "Phase 3 — Fighter"), in
// faithful (class_features):
// - C1 Champion's Improved / Superior Critical: weapon and unarmed attacks
//   crit on 19-20 from Fighter 3, 18-20 from 15 (GetCriticalHitRoll,
//   rules_crit.cpp); as decision 8 says, a 19 that misses still misses;
// - C2 Remarkable Athlete (Champion 3): advantage on initiative
//   (rules_initiative.cpp);
// - F6 Studied Attacks (13): a miss marks the target; the Fighter's next
//   attack at it before the end of its next turn has advantage
//   (rules_advantage.cpp, like Vex).
// Prestige-class levels count as Fighter levels (decision 10).

#include <windows.h>
#include <stdint.h>
#include <string.h>

#include <unordered_map>

#include "config.h"
#include "creature.h"
#include "engine_queries.h"
#include "hooks.h"
#include "log.h"
#include "log_once.h"
#include "mastery.h"
#include "nwmain_1_69.h"
#include "rules/class_features.h"
#include "rules/weapons.h"
#include "rules_loaded.h"

namespace {

using GetAttackFn = void*(__fastcall*)(void* round, void* edx, int index);
const auto g_get_attack = reinterpret_cast<GetAttackFn>(nwmain::kGetAttack);

struct StudiedMark {
  uint32_t target;
  uint32_t rounds;  // the Fighter's ended rounds when it missed
  DWORD tick;
};
std::unordered_map<uint32_t, StudiedMark> g_studied;  // by attacker id
constexpr DWORD kMarkMaxAgeMs = 30000;
LogOnce g_logged(200);

bool Faithful() { return g_config.class_features == rules::RuleVariant::kFaithful; }

// Fighter levels of a character (player character or henchman), else 0.
int FighterLevelOf(void* stats) {
  if (!stats || !Faithful()) return 0;
  const CreatureFacts facts = ReadCreatureFacts(stats);
  if (!facts.character || !facts.plausible) return 0;
  return ClassLevel2024(stats, rules::CharacterClass::kFighter);
}

uint32_t IdOf(void* creature) { return Field<uint32_t>(creature, nwmain::kCreatureObjectId); }

}  // namespace

// C1: the lowest d20 that crits for this creature's weapon attacks (20 = no change).
int ChampionCriticalFloorFor(void* stats) {
  const int floor = rules::ChampionCriticalFloor(FighterLevelOf(stats));
  if (floor < 20 && g_logged.First(LogOnce::Key(ObjectIdOf(stats), floor, 1))) {
    Log("fighter: 0x%08X Champion crits on %d-20", ObjectIdOf(stats), floor);
  }
  return floor;
}

// C2: advantage on initiative.
bool HasRemarkableAthleteAdvantage(void* creature) {
  return creature && rules::HasRemarkableAthlete(FighterLevelOf(Field<void*>(creature, nwmain::kCreatureStats)));
}

// F6: advantage from an earlier miss at this target, used up by this attack.
MasteryRollSources TakeStudiedAttacks(void* attacker, uint32_t target_id) {
  MasteryRollSources sources;
  if (!attacker || !Faithful()) return sources;
  const auto mark = g_studied.find(IdOf(attacker));
  if (mark == g_studied.end()) return sources;
  const bool live = GetTickCount() - mark->second.tick <= kMarkMaxAgeMs &&
                    rules::VexActive(CombatRoundsEnded(IdOf(attacker)), mark->second.rounds);
  if (!live) {
    g_studied.erase(mark);
  } else if (mark->second.target == target_id) {
    ++sources.advantage;
    sources.note = "Studied Attacks advantage";
    g_studied.erase(mark);
  }
  return sources;
}

// F6: after an attack roll — a miss marks the target.
void StudiedAttacksAfterAttackRoll(void* attacker, void* target_object) {
  if (!attacker || !target_object || !Faithful()) return;
  if (!rules::HasStudiedAttacks(FighterLevelOf(Field<void*>(attacker, nwmain::kCreatureStats)))) return;
  void* round = Field<void*>(attacker, nwmain::kCreatureCombatRound);
  const void* attack = round ? g_get_attack(round, nullptr, Field<uint8_t>(round, nwmain::kRoundCurrentAttack)) : nullptr;
  if (!attack || !rules::MasteryAttackMiss(Field<uint8_t>(attack, nwmain::kAttackDataResult))) return;
  const uint32_t target_id = Field<uint32_t>(target_object, nwmain::kCreatureObjectId);
  if (!engine::GetCreature(target_id)) return;  // creatures only
  g_studied[IdOf(attacker)] = StudiedMark{target_id, CombatRoundsEnded(IdOf(attacker)), GetTickCount()};
  if (g_config.log_attacks && g_logged.First(LogOnce::Key(IdOf(attacker), static_cast<int>(target_id), 2))) {
    Log("fighter: 0x%08X missed 0x%08X - Studied Attacks: advantage on its next attack at it", IdOf(attacker),
        target_id);
  }
}

// --- F1 Fighting Styles -------------------------------------------------------

namespace {

using HasFeatFn = int(__fastcall*)(void* stats, void* edx, int feat);
const auto g_has_feat = reinterpret_cast<HasFeatFn>(nwmain::kHasFeat);

bool HasStyle(void* stats, int feat) {
  if (!stats || !Faithful()) return false;
  const CreatureFacts facts = ReadCreatureFacts(stats);
  return facts.character && g_has_feat(stats, nullptr, feat) != 0;
}

bool CurrentAttackRanged(void* stats) {
  void* creature = Field<void*>(stats, nwmain::kStatsCreature);
  void* round = creature ? Field<void*>(creature, nwmain::kCreatureCombatRound) : nullptr;
  const void* attack = round ? g_get_attack(round, nullptr, Field<uint8_t>(round, nwmain::kRoundCurrentAttack)) : nullptr;
  return attack && Field<int32_t>(attack, nwmain::kAttackDataRanged) != 0;
}

void HideFightingStyleRows(void* rules_object, bool reload) {
  auto* feats = Field<uint8_t*>(rules_object, nwmain::kRulesFeats);
  const int count = Field<uint16_t>(rules_object, nwmain::kRulesFeatCount);
  if (!feats) return;
  for (int id = rules::kFeatFightingStyleArchery; id <= rules::kFeatFightingStyleTwoWeapon && id < count; ++id) {
    SetField<uint8_t>(feats + id * nwmain::kFeatStride, nwmain::kFeatMinLevel, 0xFF);
  }
  if (!reload) Log("fighter: NWN's - the Fighting Style feats hidden");
}

}  // namespace

// Archery: +2 to the current attack roll when it is ranged (GetAttackModifierVersus).
int FightingStyleAttackBonus(void* stats) {
  if (!HasStyle(stats, rules::kFeatFightingStyleArchery) || !CurrentAttackRanged(stats)) return 0;
  if (g_config.log_attacks && g_logged.First(LogOnce::Key(ObjectIdOf(stats), 0, 3))) {
    Log("fighter: 0x%08X Archery: +%d to ranged attack rolls", ObjectIdOf(stats), rules::kArcheryAttackBonus);
  }
  return rules::kArcheryAttackBonus;
}

// Archery on the character sheet's ranged attack bonus.
int FightingStyleSheetRangedBonus(void* stats) {
  return HasStyle(stats, rules::kFeatFightingStyleArchery) ? rules::kArcheryAttackBonus : 0;
}

// Two-Weapon Fighting (style): the ability modifier on the off-hand attack.
bool HasTwoWeaponFightingStyle(void* stats) { return HasStyle(stats, rules::kFeatFightingStyleTwoWeapon); }

// --- F1 Defense and Great Weapon Fighting -------------------------------------
// Defense: +1 AC while wearing armour (the chest item's base AC above 0 — the
// engine's own test in GetDEXMod(1)), on attacks (GetArmorClassVersus,
// rules_attack.cpp) and on the sheet (CNWSCreature::GetArmorClass hooked).
// Great Weapon Fighting: the weapon's damage dice (GetDamageRoll's RollDice
// calls for manufactured weapons, 0x69B0E9 on a critical, 0x69B2D1 otherwise)
// are rolled one by one, a 1 or 2 counting as 3, for a melee attack with a
// two-handed or versatile weapon held in two hands (RE-NOTES "Fighting Styles").

namespace {

using GetItemInSlotFn = void*(__fastcall*)(void* inventory, void* edx, uint32_t slot);
using ComputeArmorClassFn = int(__fastcall*)(void* item, void* edx);
using GetBaseItemFn = void*(__fastcall*)(void* base_items, void* edx, int row);
using RollDiceFn = int(__fastcall*)(void* rules_object, void* edx, int count, int sides);
using CreatureArmorClassFn = int(__fastcall*)(void* creature, void* edx);

const auto g_get_item_in_slot = reinterpret_cast<GetItemInSlotFn>(nwmain::kGetItemInSlot);
const auto g_compute_armor_class = reinterpret_cast<ComputeArmorClassFn>(nwmain::kItemComputeArmorClass);
const auto g_get_base_item = reinterpret_cast<GetBaseItemFn>(nwmain::kGetBaseItem);
const auto g_roll_dice = reinterpret_cast<RollDiceFn>(nwmain::kRollDice);
CreatureArmorClassFn g_creature_armor_class = nullptr;

void* ItemInSlot(void* creature, uint32_t slot) {
  void* inventory = Field<void*>(creature, nwmain::kCreatureInventory);
  return inventory ? g_get_item_in_slot(inventory, nullptr, slot) : nullptr;
}

bool WearingArmor(void* creature) {
  void* chest = ItemInSlot(creature, nwmain::kSlotChest);
  return chest && (g_compute_armor_class(chest, nullptr) & 0xFFFF) > 0;
}

void* BaseItemOf(void* item) {
  void* rules_object = *reinterpret_cast<void**>(nwmain::kRulesObject);
  void* base_items = rules_object ? Field<void*>(rules_object, nwmain::kRulesBaseItems) : nullptr;
  return base_items ? g_get_base_item(base_items, nullptr, Field<int32_t>(item, nwmain::kItemBaseItem)) : nullptr;
}

bool GreatWeaponFightingApplies(void* stats, void* item, int off_hand) {
  if (off_hand || !item || !HasStyle(stats, rules::kFeatFightingStyleGreatWeapon) || CurrentAttackRanged(stats)) {
    return false;
  }
  const int id = Field<int32_t>(item, nwmain::kItemBaseItem);
  const bool two_handed = rules::IsTwoHandedWeapon2024(id), versatile = rules::IsVersatileWeapon2024(id);
  if (!two_handed && !versatile) return false;
  void* base = BaseItemOf(item);
  void* creature = Field<void*>(stats, nwmain::kStatsCreature);
  if (!base || !creature || Field<uint8_t>(base, nwmain::kBaseItemWield) == nwmain::kBaseItemWieldDouble) return false;
  if (engine::CurrentAttackGetsTwoHandedStrength(creature)) return true;  // NWN wields it two-handed
  // Otherwise in two hands = nothing in the off hand, and a versatile weapon
  // of the wielder's own size (a two-handed one always).
  if (ItemInSlot(creature, nwmain::kSlotLeftHand)) return false;
  return two_handed || Field<uint8_t>(base, nwmain::kBaseItemSize) == Field<uint32_t>(creature, nwmain::kCreatureSize);
}

// The weapon-dice roll (from GreatWeaponDiceStub).
int __cdecl GreatWeaponDice(void* rules_object, int count, int sides, void* stats, void* item, int off_hand) {
  if (!GreatWeaponFightingApplies(stats, item, off_hand)) return g_roll_dice(rules_object, nullptr, count, sides);
  int total = 0, raised = 0;
  for (int i = 0; i < count; ++i) {
    const int roll = g_roll_dice(rules_object, nullptr, 1, sides) & 0xFFFF;
    raised += roll < 3 ? 1 : 0;
    total += rules::GreatWeaponFightingDie(roll);
  }
  if (g_config.log_attacks && raised && g_logged.First(LogOnce::Key(ObjectIdOf(stats), count * 100 + sides, 4))) {
    Log("fighter: 0x%08X Great Weapon Fighting: %dd%d, %d die(s) of 1-2 counted as 3 (logged once per dice)",
        ObjectIdOf(stats), count, sides, raised);
  }
  return total;
}

// In place of RollDice(count, sides) at GetDamageRoll's weapon-dice sites:
// [esp+4] count, [esp+8] sides; GetDamageRoll's frame F = esp+0x0C (item
// F+0x24, bOffHand F+0x6C); esi = stats, ecx = rules object; ret 8, AX.
__declspec(naked) void GreatWeaponDiceStub() {
  __asm {
    mov  eax, [esp + 0x30]
    mov  edx, [esp + 0x78]
    push edx
    push eax
    push esi
    push dword ptr [esp + 0x14]
    push dword ptr [esp + 0x14]
    push ecx
    call GreatWeaponDice
    add  esp, 0x18
    ret  8
  }
}

int __fastcall CreatureArmorClass(void* creature, void* edx);

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

// Defense: +1 AC in armour (GetArmorClassVersus and the sheet).
int DefenseArmorClassBonus(void* creature) {
  if (!creature) return 0;
  void* stats = Field<void*>(creature, nwmain::kCreatureStats);
  return HasStyle(stats, rules::kFeatFightingStyleDefense) && WearingArmor(creature) ? rules::kDefenseArmorClassBonus
                                                                                       : 0;
}

namespace {
int __fastcall CreatureArmorClass(void* creature, void* edx) {
  const int ac = static_cast<int16_t>(g_creature_armor_class(creature, edx) & 0xFFFF);
  return ac + DefenseArmorClassBonus(creature);
}
}  // namespace

void InstallFightingStyleHooks() {
  int applied = 0;
  for (uintptr_t site : nwmain::kWeaponDiceRollCalls) {
    applied += RedirectCall("fighter: Great Weapon Fighting - weapon dice 1-2 count as 3 (faithful)", site,
                            nwmain::kRollDice, reinterpret_cast<void*>(&GreatWeaponDiceStub))
                   ? 1
                   : 0;
  }
  applied += InstallHook({"CNWSCreature::GetArmorClass (sheet: Defense)", nwmain::kCreatureGetArmorClass,
                          nwmain::kCreatureGetArmorClassPrologue, sizeof(nwmain::kCreatureGetArmorClassPrologue),
                          reinterpret_cast<void*>(&CreatureArmorClass),
                          reinterpret_cast<void**>(&g_creature_armor_class)})
                 ? 1
                 : 0;
  Log("fighter: Fighting Style hooks %d of 3", applied);
}

void InstallFighterRules() {
  if (!Faithful()) {
    OnRulesLoaded(&HideFightingStyleRows);
    return;
  }
  InstallFightingStyleHooks();
}
