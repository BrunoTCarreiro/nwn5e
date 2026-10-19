// Decision D3 (PLAN.md): who has weapon mastery with which weapons. Only
// characters (player characters and henchmen, decision 11) have it.
// Faithful: the 2024 slot counts by class; slots are chosen with `!mastery`
// (rules_chat.cpp) or fill with the weapon kinds the character fights with
// (announced in chat); one chosen weapon can be swapped after each long rest.
// Weapon Focus gives mastery with its weapon on top (2024 Weapon Master
// stand-in). NWN mode: Barbarians, Fighters, Paladins, Rangers and Rogues have
// mastery with every weapon they are proficient with, Weapon Focus the same.
// The choices are local variables on the creature, so they live in the save
// game. The masteries' effects are D4.

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <windows.h>

#include <string>
#include <unordered_map>
#include <vector>

#include "config.h"
#include "creature.h"
#include "engine_queries.h"
#include "hooks.h"
#include "log.h"
#include "log_once.h"
#include "engine_effects.h"
#include "mastery.h"
#include "nwmain_1_69.h"
#include "rules/core.h"
#include "rules/nwn_conversion.h"
#include "rules/weapons.h"

void TellPlayer(uint32_t object_id, const std::string& line);  // rules_chat.cpp
void RememberSaveDc2024(uint32_t caster, int dc);              // rules_save_roll.cpp

namespace {

using ResetFeatUsesFn = void(__fastcall*)(void* stats, void* edx);
using HasFeatFn = int(__fastcall*)(void* stats, void* edx, int feat);
using GetAttackFn = void*(__fastcall*)(void* round, void* edx, int index);
using GetItemFn = void*(__fastcall*)(void* owner, void* edx, int which);

const auto g_has_feat = reinterpret_cast<HasFeatFn>(nwmain::kHasFeat);
const auto g_get_attack = reinterpret_cast<GetAttackFn>(nwmain::kGetAttack);
const auto g_get_current_attack_weapon = reinterpret_cast<GetItemFn>(nwmain::kGetCurrentAttackWeapon);
const auto g_get_item_in_slot_mastery = reinterpret_cast<GetItemFn>(nwmain::kGetItemInSlot);
ResetFeatUsesFn g_reset_feat_uses = nullptr;

LogOnce g_logged_slots(256);

constexpr int kMaxSlots = 6;
const char* const kSlotVars[kMaxSlots] = {"nwn5e_mastery_1", "nwn5e_mastery_2", "nwn5e_mastery_3",
                                          "nwn5e_mastery_4", "nwn5e_mastery_5", "nwn5e_mastery_6"};
const char kSwapVar[] = "nwn5e_mastery_swap";

bool Faithful() { return g_config.weapon_mastery == rules::RuleVariant::kFaithful; }

int GetVar(void* creature, const char* name) { return engine::GetLocalInt(creature, name); }

void SetVar(void* creature, const char* name, int value) { engine::SetLocalInt(creature, name, value); }

bool HasFeat(void* stats, int feat) { return stats && feat >= 0 && g_has_feat(stats, nullptr, feat) != 0; }

bool Proficient(void* stats, int base_item) {
  const std::vector<int> feats = rules::ProficiencyFeatsForNwnBaseItem(base_item);
  if (feats.empty()) return true;
  for (int feat : feats) {
    if (HasFeat(stats, feat)) return true;
  }
  return false;
}

bool HasFocus(void* stats, int base_item) { return HasFeat(stats, rules::WeaponFocusFeatForNwnBaseItem(base_item)); }

std::vector<rules::ClassLevels> Classes(void* stats) { return rules::GroupClassLevels(ReadClassSlots(stats)); }

bool MasteryClass(void* stats) {
  for (const rules::ClassLevels& entry : Classes(stats)) {
    if (rules::HasWeaponMastery(entry.character_class)) return true;
  }
  return false;
}

struct Slots {
  int count = 0;
  std::vector<int> chosen = std::vector<int>(kMaxSlots, -1);  // base items, -1 empty
  bool swap_ready = false;
};

Slots ReadSlots(void* creature, void* stats) {
  Slots slots;
  slots.count = rules::WeaponMasterySlots(Classes(stats));
  if (slots.count > kMaxSlots) slots.count = kMaxSlots;
  for (int i = 0; i < kMaxSlots; ++i) {
    const int value = GetVar(creature, kSlotVars[i]);
    slots.chosen[i] = value > 0 ? value - 1 : -1;
  }
  slots.swap_ready = GetVar(creature, kSwapVar) != 0;
  return slots;
}

int UsedSlots(const Slots& slots) {
  int used = 0;
  for (int i = 0; i < slots.count; ++i) used += slots.chosen[i] >= 0 ? 1 : 0;
  return used;
}

std::string Label(int base_item) {
  const char* name = rules::NwnWeaponName(base_item);
  if (!name) return "?";
  return std::string(name) + " (" + rules::MasteryName(rules::MasteryForNwnBaseItem(base_item)) + ")";
}

std::string SlotList(const Slots& slots) {
  std::string text;
  for (int i = 0; i < slots.count; ++i) {
    if (i) text += ", ";
    text += slots.chosen[i] >= 0 ? Label(slots.chosen[i]) : "empty";
  }
  return text;
}

std::string FocusList(void* stats) {
  std::string text;
  for (int base_item : rules::NwnMasteryWeapons()) {
    if (!HasFocus(stats, base_item)) continue;
    if (!text.empty()) text += ", ";
    text += Label(base_item);
  }
  return text;
}

uint32_t IdOf(void* creature) { return Field<uint32_t>(creature, nwmain::kCreatureObjectId); }

// The player character to tell about this character: itself, or a henchman's master.
uint32_t PlayerFor(void* creature, const CreatureFacts& facts) {
  return facts.is_player ? IdOf(creature) : Field<uint32_t>(creature, nwmain::kCreatureMaster);
}

void Announce(void* creature, const CreatureFacts& facts, const std::string& for_player,
              const std::string& for_henchman) {
  TellPlayer(PlayerFor(creature, facts), facts.is_player ? for_player : for_henchman);
}

// The base item of the weapon making the current attack; -1 unarmed or unknown.
int CurrentAttackBaseItem(void* creature) {
  if (!creature) return -1;
  void* round = Field<void*>(creature, nwmain::kCreatureCombatRound);
  if (!round) return -1;
  const void* attack = g_get_attack(round, nullptr, Field<uint8_t>(round, nwmain::kRoundCurrentAttack));
  if (!attack) return -1;
  void* item = g_get_current_attack_weapon(round, nullptr, Field<uint8_t>(attack, nwmain::kAttackDataType));
  return item ? Field<int32_t>(item, nwmain::kItemBaseItem) : -1;
}

void __fastcall ResetFeatRemainingUses(void* stats, void* edx) {
  g_reset_feat_uses(stats, edx);
  if (!Faithful()) return;
  void* creature = Field<void*>(stats, nwmain::kStatsCreature);
  if (!creature) return;
  const CreatureFacts facts = ReadCreatureFacts(stats);
  if (!facts.character) return;
  const Slots slots = ReadSlots(creature, stats);
  if (slots.count == 0 || slots.swap_ready) return;
  SetVar(creature, kSwapVar, 1);
  if (g_config.log_attacks) Log("mastery: object 0x%08X rested: one swap available", IdOf(creature));
  if (facts.is_player && rules::FreeMasterySlot(slots.chosen, slots.count) < 0) {
    TellPlayer(IdOf(creature), "nwn5e: rested - you can swap one weapon mastery: !mastery <old weapon> for <new weapon>");
  }
}

std::vector<std::string> Status(void* creature, void* stats) {
  const Slots slots = ReadSlots(creature, stats);
  const std::string focus = FocusList(stats);
  std::vector<std::string> lines;
  if (slots.count == 0) {
    lines.push_back("nwn5e: your classes have no Weapon Mastery (Barbarian, Fighter, Paladin, Ranger and Rogue do)." +
                    (focus.empty() ? std::string() : " Weapon Focus gives you: " + focus + "."));
    return lines;
  }
  lines.push_back("nwn5e: weapon mastery, " + std::to_string(UsedSlots(slots)) + " of " + std::to_string(slots.count) +
                  " slots: " + SlotList(slots) + "." + (focus.empty() ? "" : " Weapon Focus: " + focus + "."));
  lines.push_back(std::string("nwn5e: !mastery <weapon> fills an empty slot (empty slots also fill with the weapons "
                              "you fight with); !mastery <old> for <new> swaps one after each long rest (") +
                  (slots.swap_ready ? "swap ready)." : "no swap until you rest)."));
  return lines;
}

std::string UnknownWeapon(const std::string& text) {
  return "nwn5e: unknown weapon \"" + text + "\" (e.g. longsword, short sword, dagger, shortbow, greataxe).";
}

std::string AlreadyMastered(void* stats, const Slots& slots, int base_item) {
  if (rules::MasterySlotOf(slots.chosen, slots.count, base_item) >= 0 || HasFocus(stats, base_item)) {
    return "nwn5e: you already have mastery with " + Label(base_item) + ".";
  }
  return std::string();
}

// "!mastery <weapon>" fills an empty slot; "!mastery <A> for <B>" swaps one
// slot after a long rest, in either word order (the one already in a slot is
// given up).
std::string Choose(void* creature, void* stats, const std::string& args) {
  std::string lower = args;
  for (char& c : lower) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
  const size_t split = lower.find(" for ");
  const std::string first = split == std::string::npos ? args : args.substr(0, split);
  const int first_item = rules::NwnWeaponFromName(first);
  if (first_item < 0) return UnknownWeapon(first);
  const Slots slots = ReadSlots(creature, stats);
  if (slots.count == 0) return Status(creature, stats)[0];
  const uint32_t id = IdOf(creature);
  if (split == std::string::npos) {
    if (!Proficient(stats, first_item)) return "nwn5e: you are not proficient with " + Label(first_item) + ".";
    const std::string already = AlreadyMastered(stats, slots, first_item);
    if (!already.empty()) return already;
    const int free_slot = rules::FreeMasterySlot(slots.chosen, slots.count);
    if (free_slot < 0) {
      return "nwn5e: all " + std::to_string(slots.count) + " slots are used (" + SlotList(slots) +
             "). After a long rest you can swap one: !mastery <weapon> for " + rules::NwnWeaponName(first_item) + ".";
    }
    SetVar(creature, kSlotVars[free_slot], first_item + 1);
    if (g_config.log_attacks) Log("mastery: object 0x%08X slot %d = %s (chosen)", id, free_slot + 1, Label(first_item).c_str());
    return "nwn5e: weapon mastery with " + Label(first_item) + " - slot " + std::to_string(free_slot + 1) + " of " +
           std::to_string(slots.count) + ".";
  }
  const std::string second = args.substr(split + 5);
  const int second_item = rules::NwnWeaponFromName(second);
  if (second_item < 0) return UnknownWeapon(second);
  rules::MasterySwap swap = {-1, -1};
  if (!rules::ResolveMasterySwap(slots.chosen, slots.count, first_item, second_item, &swap)) {
    return "nwn5e: to swap, name one weapon in your slots and one that is not (your slots: " + SlotList(slots) + ").";
  }
  if (!Proficient(stats, swap.take)) return "nwn5e: you are not proficient with " + Label(swap.take) + ".";
  if (HasFocus(stats, swap.take)) return AlreadyMastered(stats, slots, swap.take);
  if (!slots.swap_ready) return "nwn5e: you can swap one weapon mastery after each long rest - rest first.";
  const int slot = rules::MasterySlotOf(slots.chosen, slots.count, swap.give_up);
  SetVar(creature, kSlotVars[slot], swap.take + 1);
  SetVar(creature, kSwapVar, 0);
  if (g_config.log_attacks) {
    Log("mastery: object 0x%08X slot %d = %s instead of %s (swap)", id, slot + 1, Label(swap.take).c_str(),
        Label(swap.give_up).c_str());
  }
  return "nwn5e: weapon mastery with " + Label(swap.take) + " instead of " + Label(swap.give_up) + ".";
}

}  // namespace

// D4 asks this: does `creature` have mastery with weapons of `base_item`?
bool HasWeaponMasteryWith(void* creature, int base_item) {
  if (g_config.weapon_mastery == rules::RuleVariant::kOff || !creature) return false;
  void* stats = Field<void*>(creature, nwmain::kCreatureStats);
  if (!stats || !ReadCreatureFacts(stats).character) return false;
  const int kind = rules::MasteryKindOfNwnBaseItem(base_item);
  if (!rules::NwnWeaponName(kind)) return false;
  if (HasFocus(stats, kind)) return true;
  if (!Faithful()) return MasteryClass(stats) && Proficient(stats, base_item);
  const Slots slots = ReadSlots(creature, stats);
  return rules::MasterySlotOf(slots.chosen, slots.count, kind) >= 0;
}

// Called for every attack roll (rules_advantage.cpp): fills an empty slot with
// the weapon kind a character fights with, and says so in chat.
void NoteWeaponUsed(void* creature) {
  if (!Faithful() || !creature) return;
  void* stats = Field<void*>(creature, nwmain::kCreatureStats);
  if (!stats) return;
  const CreatureFacts facts = ReadCreatureFacts(stats);
  if (!facts.character) return;
  const int base_item = CurrentAttackBaseItem(creature);
  const int kind = base_item < 0 ? -1 : rules::MasteryKindOfNwnBaseItem(base_item);
  if (kind < 0 || !rules::NwnWeaponName(kind)) return;
  const Slots slots = ReadSlots(creature, stats);
  if (g_config.log_attacks &&
      g_logged_slots.First(LogOnce::Key(IdOf(creature), slots.count, UsedSlots(slots)))) {
    Log("mastery: object 0x%08X %s: %d slots: %s", IdOf(creature), KindOf(facts), slots.count, SlotList(slots).c_str());
  }
  if (slots.count == 0 || rules::MasterySlotOf(slots.chosen, slots.count, kind) >= 0 || HasFocus(stats, kind)) return;
  const int free_slot = rules::FreeMasterySlot(slots.chosen, slots.count);
  if (free_slot < 0 || !Proficient(stats, base_item)) return;
  SetVar(creature, kSlotVars[free_slot], kind + 1);
  if (g_config.log_attacks) {
    Log("mastery: object 0x%08X slot %d = %s (used in a fight)", IdOf(creature), free_slot + 1, Label(kind).c_str());
  }
  const std::string where = " - slot " + std::to_string(free_slot + 1) + " of " + std::to_string(slots.count) + ".";
  Announce(creature, facts, "nwn5e: you now have weapon mastery with " + Label(kind) + where + " Change it with !mastery.",
           "nwn5e: your henchman now has weapon mastery with " + Label(kind) + where);
}

std::vector<std::string> MasteryCommand(void* creature, const std::string& args) {
  if (g_config.weapon_mastery == rules::RuleVariant::kOff) {
    return {"nwn5e: weapon mastery is off (weapon_mastery=off in nwn5e.ini)."};
  }
  void* stats = Field<void*>(creature, nwmain::kCreatureStats);
  if (!stats) return {};
  if (!Faithful()) {
    const std::string focus = FocusList(stats);
    return {std::string("nwn5e: weapon mastery (NWN mode): ") +
            (MasteryClass(stats) ? "every weapon you are proficient with." : "your classes have none.") +
            (focus.empty() ? "" : " Weapon Focus: " + focus + ".")};
  }
  if (args.empty()) return Status(creature, stats);
  return {Choose(creature, stats, args)};
}

// --- D4 Vex / Sap -----------------------------------------------------------

namespace {

using EndCombatRoundFn = void(__fastcall*)(void* round, void* edx);
EndCombatRoundFn g_end_combat_round = nullptr;

struct VexMark {
  uint32_t target;
  uint32_t attacker_rounds;  // the attacker's ended rounds when it hit
  DWORD tick;
};
struct SapMark {
  uint32_t source;
  uint32_t source_rounds;
  DWORD tick;
};
std::unordered_map<uint32_t, uint32_t> g_rounds_ended;  // by creature id
std::unordered_map<uint32_t, VexMark> g_vex;            // by attacker id
std::unordered_map<uint32_t, SapMark> g_sap;            // by sapped creature id
std::unordered_map<uint32_t, DWORD> g_slowed;           // D4 Slow / Topple: last applied, by target id
std::unordered_map<uint32_t, DWORD> g_toppled;
// Vex / Slow need the hit to deal damage (2024): the hit's damage is applied a
// moment after the roll, so they wait for it here (attacker -> target).
struct AwaitDamage {
  uint32_t target;
  DWORD tick;
};
std::unordered_map<uint32_t, AwaitDamage> g_slow_awaiting;  // Slow applies once the damage lands
std::unordered_map<uint32_t, AwaitDamage> g_vex_awaiting;   // Vex's mark is withdrawn if it dealt none
std::unordered_map<uint32_t, AwaitDamage> g_push_awaiting;  // Push moves the target when the hit lands
constexpr float kPushMetres = 3.0f;  // 10 ft
constexpr int kPushFallMs = 1200;    // the fall's round pause
constexpr float kPushSlideSeconds = 0.25f;  // the move, stepped while it falls (owner: shorter than 0.45 s)
constexpr DWORD kPushMoveMaxMs = 3000;      // then it is put at the end
// A pushed creature sliding along its planned line (target id ->).
struct PushMove {
  engine::Vector3 from;
  engine::Vector3 to;
  float metres;
  float travelled;
  void* area;
  DWORD tick;
};
std::unordered_map<uint32_t, PushMove> g_push_moves;
// After a push the attacker went for a closer enemy (attacker id ->): made
// to stick at the end of its round.
struct PushRetarget {
  uint32_t pushed;
  uint32_t next;
  DWORD tick;
};
std::unordered_map<uint32_t, PushRetarget> g_push_retargets;
using GetNearestEnemyFn = uint32_t(__fastcall*)(void* creature, void* edx, float range, uint32_t exclude, int seen,
                                                int path);
using AddAttackActionsFn = int(__fastcall*)(void* creature, void* edx, uint32_t target, int passive, int clear_first,
                                            int front);
const auto g_get_nearest_enemy = reinterpret_cast<GetNearestEnemyFn>(nwmain::kGetNearestEnemy);
const auto g_add_attack_actions = reinterpret_cast<AddAttackActionsFn>(nwmain::kAddAttackActions);
using AIUpdateFn = void(__fastcall*)(void* creature, void* edx);
AIUpdateFn g_ai_update = nullptr;
constexpr DWORD kAwaitDamageMs = 5000;
using OnApplyDamageFn = int(__fastcall*)(void* handler, void* edx, void* object, void* effect, int loading);
OnApplyDamageFn g_on_apply_damage = nullptr;
struct CleavePending {
  uint32_t target;
  uint32_t attacker_rounds;
  DWORD tick;
  bool rolled = false;  // the extra attack's roll has happened
  int attack_index = -1;  // its slot in the round, for the damage adjustment
  uint32_t first_target = 0;  // the round's target to go back to after it
};

// The round's target goes back to the first target after the extra attack
// (if that one still stands), so the round's other attacks stay on it.
void RestoreRoundTarget(void* attacker, void* round, uint32_t first_target) {
  void* first = engine::GetCreature(first_target);
  if (!first || engine::IsDeadOrDying(first)) return;
  SetField<uint32_t>(round, nwmain::kRoundTarget, first_target);
  SetField<uint32_t>(attacker, nwmain::kCreatureTargetSwitched, 1);
}
std::unordered_map<uint32_t, uint32_t> g_cleave_used;         // attacker -> its ended rounds + 1 when it cleaved
std::unordered_map<uint32_t, CleavePending> g_cleave_pending;  // attacker -> the extra attack not resolved yet

using AddCleaveAttackFn = void(__fastcall*)(void* round, void* edx, uint32_t target, int great_cleave);
using GetBaseItemFn = void*(__fastcall*)(void* base_items, void* edx, int row);
const auto g_add_cleave_attack = reinterpret_cast<AddCleaveAttackFn>(nwmain::kAddCleaveAttack);
const auto g_get_base_item = reinterpret_cast<GetBaseItemFn>(nwmain::kGetBaseItem);

// The nearest other enemy within 5 ft of the first target and in reach (D4 Cleave).
uint32_t CleaveTarget(void* attacker, void* first, uint32_t attacker_id) {
  const float reach = engine::MaxAttackRange(attacker, nwmain::kObjectInvalid, nwmain::kMaxAttackRangeRetarget);
  uint32_t best = nwmain::kObjectInvalid;
  float best_distance = 1.0e9f;
  engine::ForEachOtherCreatureInArea(first, [&](uint32_t id, void* creature) {
    if (id == attacker_id || !engine::IsEnemy(attacker, id) || engine::IsDeadOrDying(creature)) return;
    if (engine::EdgeDistance(first, creature) > nwmain::kCleaveSplashMetres) return;
    const float distance = engine::EdgeDistance(attacker, creature);
    if (!(distance < reach) || distance >= best_distance) return;
    best = id;
    best_distance = distance;
  });
  return best;
}

constexpr DWORD kRoundMs = 6000;
constexpr float kRoundSeconds = 6.0f;

using SaveRollFn = uint32_t(__fastcall*)(void* creature, void* edx, uint32_t save, uint32_t dc, uint32_t type,
                                         uint32_t versus, uint32_t feedback, uint32_t feat, uint32_t from_combat);
using DexModFn = int(__fastcall*)(void* stats, void* edx, int armor_dex_cap);
const auto g_saving_throw_roll = reinterpret_cast<SaveRollFn>(nwmain::kSavingThrowRoll);  // the hooked one
const auto g_get_dex_mod = reinterpret_cast<DexModFn>(nwmain::kGetDexMod);

// The attack's ability modifier (D4 Graze, Topple).
int AttackAbility(void* stats, const void* attack, int base_item) {
  const bool ranged = Field<int32_t>(attack, nwmain::kAttackDataRanged) != 0;
  const int strength = Field<int8_t>(stats, nwmain::kStatsStrModifier);
  const int dexterity = static_cast<int8_t>(g_get_dex_mod(stats, nullptr, 0) & 0xFF);
  return rules::AttackAbilityModifier(ranged, HasFeat(stats, nwmain::kFeatWeaponFinesse),
                                      rules::IsFinesseNwnBaseItem(base_item), strength, dexterity);
}

bool RecentlyApplied(std::unordered_map<uint32_t, DWORD>* applied, uint32_t target_id, DWORD now) {
  const auto it = applied->find(target_id);
  if (it != applied->end() && now - it->second < kRoundMs) return true;
  (*applied)[target_id] = now;
  return false;
}
// Rounds only end in combat; a mark from an old fight also lapses after this.
constexpr DWORD kMarkMaxAgeMs = 30000;
int g_effect_log_lines = 0;
constexpr int kEffectLogLimit = 400;

bool EffectLogSlot() { return g_config.log_attacks && g_effect_log_lines < kEffectLogLimit && ++g_effect_log_lines; }

bool MasteryOn() { return g_config.weapon_mastery != rules::RuleVariant::kOff; }

uint32_t RoundsEnded(uint32_t id) {
  const auto it = g_rounds_ended.find(id);
  return it == g_rounds_ended.end() ? 0 : it->second;
}

// The attack action the creature carries out next (the head of its queue),
// or null.
void* HeadAttackAction(void* creature) {
  void* list = Field<void*>(creature, nwmain::kObjectActionQueue);
  void* head = list ? Field<void*>(list, nwmain::kListHead) : nullptr;
  void* data = head ? Field<void*>(head, nwmain::kListNodeData) : nullptr;
  return data && Field<int32_t>(data, nwmain::kActionId) == nwmain::kActionAttackObject ? data : nullptr;
}

// After a push, at the end of the attacker's round: the in-place switch
// holds when its attack action now targets the closer enemy; if it went back
// to the pushed one, the switch is made the way a player's attack click
// makes it (the round is over, so no attack is lost).
void KeepPushRetarget(void* creature) {
  const auto it = g_push_retargets.find(IdOf(creature));
  if (it == g_push_retargets.end()) return;
  const PushRetarget retarget = it->second;
  g_push_retargets.erase(it);
  if (GetTickCount() - retarget.tick > 15000) return;
  const void* action = HeadAttackAction(creature);
  const uint32_t ordered = action ? Field<uint32_t>(action, nwmain::kActionFirstParam) : nwmain::kObjectInvalid;
  void* round = Field<void*>(creature, nwmain::kCreatureCombatRound);
  const uint32_t round_target = round ? Field<uint32_t>(round, nwmain::kRoundTarget) : nwmain::kObjectInvalid;
  const char* outcome = "kept";
  if (ordered == retarget.pushed) {
    void* next = engine::GetCreature(retarget.next);
    if (!next || engine::IsDeadOrDying(next) || !engine::IsEnemy(creature, retarget.next)) {
      outcome = "went back to the pushed one; the closer enemy is gone";
    } else {
      g_add_attack_actions(creature, nullptr, retarget.next, 0, 1, 0);
      outcome = "went back to the pushed one; attack order switched (as a click)";
    }
  } else if (ordered != retarget.next) {
    outcome = action ? "another attack order since" : "no attack order";
  }
  if (EffectLogSlot()) {
    Log("mastery: 0x%08X round end after Push: attack order 0x%08X, round target 0x%08X, wanted 0x%08X: %s",
        IdOf(creature), ordered, round_target, retarget.next, outcome);
  }
}

void __fastcall EndCombatRound(void* round, void* edx) {
  void* owner = Field<void*>(round, nwmain::kRoundOwner);
  if (owner) ++g_rounds_ended[IdOf(owner)];
  g_end_combat_round(round, edx);
  if (owner && !g_push_retargets.empty()) KeepPushRetarget(owner);
}

// Push v2: a pushed creature slides along its line during the fall, a step
// each AI update (after the engine's own, so its walking can't pull it off
// the line).
void __fastcall AIUpdate(void* creature, void* edx) {
  g_ai_update(creature, edx);
  if (g_push_moves.empty() || !creature) return;
  const auto it = g_push_moves.find(IdOf(creature));
  if (it == g_push_moves.end()) return;
  PushMove& move = it->second;
  if (engine::IsDeadOrDying(creature) || engine::GetArea(creature) != move.area) {
    g_push_moves.erase(it);
    return;
  }
  int ms = Field<int32_t>(creature, nwmain::kCreatureUpdateMs);
  if (ms < 0 || ms > 250) ms = 50;
  move.travelled += move.metres / kPushSlideSeconds * static_cast<float>(ms) / 1000.0f;
  if (move.travelled >= move.metres || GetTickCount() - move.tick > kPushMoveMaxMs) move.travelled = move.metres;
  const float t = move.travelled / move.metres;
  engine::PlaceOnGround(creature, move.from.x + (move.to.x - move.from.x) * t,
                       move.from.y + (move.to.y - move.from.y) * t);
  if (move.travelled >= move.metres) g_push_moves.erase(it);
}

}  // namespace

int MasteryAttackWeapon(void* creature) { return CurrentAttackBaseItem(creature); }

MasteryRollSources TakeMasteryRollSources(void* attacker, uint32_t target_id) {
  MasteryRollSources sources;
  if (!MasteryOn() || !attacker) return sources;
  const uint32_t attacker_id = IdOf(attacker);
  const DWORD now = GetTickCount();
  const auto vex = g_vex.find(attacker_id);
  if (vex != g_vex.end()) {
    const bool live = now - vex->second.tick <= kMarkMaxAgeMs &&
                      rules::VexActive(RoundsEnded(attacker_id), vex->second.attacker_rounds);
    if (!live) {
      g_vex.erase(vex);
    } else if (vex->second.target == target_id) {
      ++sources.advantage;
      sources.note = "Vex advantage";
      g_vex.erase(vex);
    }
  }
  const auto sap = g_sap.find(attacker_id);
  if (sap != g_sap.end()) {
    if (now - sap->second.tick <= kMarkMaxAgeMs &&
        rules::SapActive(RoundsEnded(sap->second.source), sap->second.source_rounds)) {
      ++sources.disadvantage;
      sources.note += sources.note.empty() ? "Sap disadvantage" : ", Sap disadvantage";
    }
    g_sap.erase(sap);
  }
  return sources;
}

void MasteryAfterAttackRoll(void* attacker, void* target_object, int base_item) {
  if (!MasteryOn() || !attacker || !target_object || base_item < 0) return;
  const rules::Mastery mastery = rules::MasteryForNwnBaseItem(base_item);
  // Push: faithful only (D4: NWN mode is the same without Push).
  const bool push = mastery == rules::Mastery::kPush && g_config.weapon_mastery == rules::RuleVariant::kFaithful;
  if (mastery != rules::Mastery::kVex && mastery != rules::Mastery::kSap && mastery != rules::Mastery::kGraze &&
      mastery != rules::Mastery::kSlow && mastery != rules::Mastery::kTopple && mastery != rules::Mastery::kCleave &&
      !push) {
    return;
  }
  void* round = Field<void*>(attacker, nwmain::kCreatureCombatRound);
  const void* attack = round ? g_get_attack(round, nullptr, Field<uint8_t>(round, nwmain::kRoundCurrentAttack)) : nullptr;
  if (!attack) return;
  const int result = Field<uint8_t>(attack, nwmain::kAttackDataResult);
  const bool hit = rules::MasteryAttackHit(result);
  // The first roll at a pending Cleave target, in the same round, is the
  // extra attack Cleave added; a round that ended without it lost it (e.g. a
  // potion was drunk).
  {
    const uint32_t rolled_at = Field<uint32_t>(target_object, nwmain::kCreatureObjectId);
    const auto pending = g_cleave_pending.find(IdOf(attacker));
    if (pending != g_cleave_pending.end() && !pending->second.rolled) {
      if (RoundsEnded(IdOf(attacker)) != pending->second.attacker_rounds) {
        g_cleave_pending.erase(pending);
      } else if (pending->second.target == rolled_at) {
        pending->second.rolled = true;
        pending->second.attack_index = Field<uint8_t>(round, nwmain::kRoundCurrentAttack);
        RestoreRoundTarget(attacker, round, pending->second.first_target);
        if (EffectLogSlot()) {
          Log("mastery: 0x%08X Cleave extra attack at 0x%08X: %s (result %d)", IdOf(attacker), rolled_at,
              hit ? "hit" : "miss", result);
        }
        if (!hit) g_cleave_pending.erase(pending);
        return;  // the extra attack itself does not cleave again
      }
    }
  }
  if (mastery == rules::Mastery::kGraze ? !rules::MasteryAttackMiss(result) : !hit) return;
  const uint32_t target_id = Field<uint32_t>(target_object, nwmain::kCreatureObjectId);
  void* target = engine::GetCreature(target_id);  // creatures only (not doors / placeables)
  if (!target || !HasWeaponMasteryWith(attacker, base_item)) return;
  void* stats = Field<void*>(attacker, nwmain::kCreatureStats);
  const uint32_t attacker_id = IdOf(attacker);
  const uint32_t rounds = RoundsEnded(attacker_id);
  const DWORD now = GetTickCount();
  // Effects land when the swing does, like the engine schedules a hit.
  const uint32_t delay = Field<uint16_t>(attack, nwmain::kAttackDataAnimationLength) >> 1;
  std::string what;
  switch (mastery) {
    case rules::Mastery::kVex:
      g_vex[attacker_id] = VexMark{target_id, rounds, now};
      g_vex_awaiting[attacker_id] = AwaitDamage{target_id, now};
      what = "Vex (advantage on its next attack at that target, if the hit deals damage)";
      break;
    case rules::Mastery::kSap:
      g_sap[target_id] = SapMark{attacker_id, rounds, now};
      what = "Sap (target's next attack has disadvantage)";
      break;
    case rules::Mastery::kGraze: {
      const int damage = rules::GrazeDamage(AttackAbility(stats, attack, base_item));
      if (damage <= 0) return;
      engine::ApplyDamage(target, attacker_id, damage, nwmain::kDamageTypeSlashing, delay);
      what = "Graze: " + std::to_string(damage) + " slashing damage on a miss";
      break;
    }
    case rules::Mastery::kSlow:
      g_slow_awaiting[attacker_id] = AwaitDamage{target_id, now};  // applied when the damage lands
      what = "Slow: waiting for the hit's damage";
      break;
    case rules::Mastery::kTopple: {
      if (RecentlyApplied(&g_toppled, target_id, now)) return;
      const CreatureFacts facts = ReadCreatureFacts(stats);
      const int dc = rules::ToppleDc(AttackAbility(stats, attack, base_item),
                                     rules::ProficiencyBonusForCreature(facts.character, facts.level,
                                                                        facts.challenge_rating));
      RememberSaveDc2024(attacker_id, dc);
      const uint32_t save = g_saving_throw_roll(target, nullptr, nwmain::kSaveFortitude, static_cast<uint32_t>(dc), 0,
                                                attacker_id, 1, 0, 1) & 0xFF;
      if (save == 0) engine::ApplyKnockdown(target, attacker_id, kRoundSeconds, delay);
      what = "Topple: Constitution save DC " + std::to_string(dc) +
             (save == 0 ? " failed, knocked down for a round" : save == 2 ? " - immune" : " made");
      break;
    }
    case rules::Mastery::kPush:
      g_push_awaiting[attacker_id] = AwaitDamage{target_id, now};  // moved when the hit lands
      what = "Push: when the hit lands";
      break;
    case rules::Mastery::kCleave: {
      if (Field<int32_t>(attack, nwmain::kAttackDataRanged) != 0) return;  // melee attack rolls only
      const auto used = g_cleave_used.find(attacker_id);
      if (used != g_cleave_used.end() && used->second == rounds + 1) return;  // once per round
      const uint32_t second = CleaveTarget(attacker, target, attacker_id);
      if (second == nwmain::kObjectInvalid) return;
      // As the engine's Great Cleave does: the round's target becomes the
      // second creature, then the cleave attack is queued next.
      SetField<uint32_t>(round, nwmain::kRoundTarget, second);
      SetField<uint32_t>(attacker, nwmain::kCreatureTargetSwitched, 1);
      g_add_cleave_attack(round, nullptr, second, 0);
      g_cleave_used[attacker_id] = rounds + 1;
      CleavePending pending;
      pending.target = second;
      pending.attacker_rounds = rounds;
      pending.tick = now;
      pending.first_target = target_id;
      g_cleave_pending[attacker_id] = pending;
      char text[96];
      sprintf_s(text, "Cleave: extra attack at 0x%08X (its damage without Strength)", second);
      what = text;
      break;
    }
    default:
      return;
  }
  if (EffectLogSlot()) {
    Log("mastery: 0x%08X %s 0x%08X with %s: %s", attacker_id, hit ? "hit" : "missed", target_id,
        Label(base_item).c_str(), what.c_str());
  }
}

namespace {

// After a push: when an enemy is now closer to the attacker than the pushed
// one, the attacker goes for the nearest such enemy instead of chasing (owner,
// 2026-10-19): a living, hostile enemy it sees, edge to edge nearer than the
// pushed one will be. Switched in place for the rest of this round (round
// target, queued attack action, attack targets); KeepPushRetarget makes it
// stick. Every decision is logged with the candidates.
uint32_t RetargetAfterPush(void* attacker, uint32_t attacker_id, uint32_t pushed, float pushed_distance) {
  void* round = Field<void*>(attacker, nwmain::kCreatureCombatRound);
  const uint32_t round_target = round ? Field<uint32_t>(round, nwmain::kRoundTarget) : nwmain::kObjectInvalid;
  const void* head = HeadAttackAction(attacker);
  const uint32_t ordered = head ? Field<uint32_t>(head, nwmain::kActionFirstParam) : nwmain::kObjectInvalid;
  uint32_t next = nwmain::kObjectInvalid;
  float best = pushed_distance;
  std::string candidates;
  engine::ForEachOtherCreatureInArea(attacker, [&](uint32_t id, void* creature) {
    if (id == pushed || !engine::IsEnemy(attacker, id) || engine::IsDeadOrDying(creature)) return;
    const float distance = engine::EdgeDistance(attacker, creature);
    const bool seen = engine::Sees(attacker, id);
    char text[48];
    sprintf_s(text, "%s0x%08X %.1f m%s", candidates.empty() ? "" : ", ", id, distance, seen ? "" : " unseen");
    candidates += text;
    if (seen && distance < best) {
      best = distance;
      next = id;
    }
  });
  const bool attacking_pushed = round_target == pushed || ordered == pushed;
  if (EffectLogSlot()) {
    Log("mastery: 0x%08X Push retarget: round target 0x%08X, attack order 0x%08X; pushed 0x%08X ends %.1f m away; "
        "enemies: %s -> %s",
        attacker_id, round_target, ordered, pushed, pushed_distance, candidates.empty() ? "none" : candidates.c_str(),
        !round ? "no round" : !attacking_pushed ? "not attacking the pushed one" :
        next == nwmain::kObjectInvalid ? "none closer, keeps the pushed one" : "switch");
  }
  if (!round || !attacking_pushed || next == nwmain::kObjectInvalid) return nwmain::kObjectInvalid;
  SetField<uint32_t>(round, nwmain::kRoundTarget, next);
  SetField<uint32_t>(attacker, nwmain::kCreatureTargetSwitched, 1);
  if (void* action = HeadAttackAction(attacker)) {
    if (Field<uint32_t>(action, nwmain::kActionFirstParam) == pushed) {
      SetField<uint32_t>(action, nwmain::kActionFirstParam, next);
    }
  }
  if (Field<uint32_t>(attacker, nwmain::kCreatureAttackTarget) == pushed) {
    SetField<uint32_t>(attacker, nwmain::kCreatureAttackTarget, next);
  }
  if (Field<uint32_t>(attacker, nwmain::kCreatureAttemptedAttackTarget) == pushed) {
    SetField<uint32_t>(attacker, nwmain::kCreatureAttemptedAttackTarget, next);
  }
  if (Field<int16_t>(attacker, nwmain::kObjectCurrentAction) == nwmain::kActionAttackObject &&
      Field<uint32_t>(attacker, nwmain::kObjectCurrentActionTarget) == pushed) {
    SetField<uint32_t>(attacker, nwmain::kObjectCurrentActionTarget, next);
  }
  g_push_retargets[attacker_id] = PushRetarget{pushed, next, GetTickCount()};
  return next;
}

// D4 Push, on the hit's landing: plan the line (walls, placeables), start the
// fall, slide the target along the line in the AI updates, and retarget the
// attacker if an enemy is now closer.
void Push(void* attacker, uint32_t attacker_id, void* target, uint32_t target_id) {
  const engine::PushPlan plan = engine::PlanPush(target, attacker, kPushMetres);
  if (plan.refused) {
    if (EffectLogSlot()) {
      Log("mastery: 0x%08X Push on 0x%08X: not pushed (%s%s)", attacker_id, target_id, plan.refused,
          plan.blocker != nwmain::kObjectInvalid ? ", see the blocker below" : "");
      if (plan.blocker != nwmain::kObjectInvalid) Log("mastery:   blocker 0x%08X", plan.blocker);
    }
    return;
  }
  const float pushed_distance = engine::EdgeDistance(attacker, target) + plan.metres;
  // It can't walk while it falls and slides (its own attack order would pull
  // it back toward the attacker); its orders stay queued.
  engine::ApplyCantMove(target, attacker_id, static_cast<float>(kPushFallMs) / 1000.0f, 0);
  uint32_t paused_by = nwmain::kObjectInvalid;
  const engine::FallResult fall = engine::PlayPushFall(target, attacker_id, plan.away, kPushFallMs, &paused_by);
  g_push_moves[target_id] = PushMove{plan.from, plan.to, plan.metres, 0.0f, engine::GetArea(target), GetTickCount()};
  const uint32_t next = RetargetAfterPush(attacker, attacker_id, target_id, pushed_distance);
  if (EffectLogSlot()) {
    char blocker[48] = "";
    if (plan.blocker != nwmain::kObjectInvalid) sprintf_s(blocker, " (cut short by placeable 0x%08X)", plan.blocker);
    char retarget[64] = "";
    if (next != nwmain::kObjectInvalid) sprintf_s(retarget, "; attacker now attacks the closer 0x%08X", next);
    char fall_note[96];
    if (fall == engine::FallResult::kFell) {
      sprintf_s(fall_note, ", falls and gets up");
    } else if (fall == engine::FallResult::kFellUnpaused && paused_by != nwmain::kObjectInvalid) {
      sprintf_s(fall_note, ", falls without a round pause (its round is paused by 0x%08X)", paused_by);
    } else if (fall == engine::FallResult::kFellUnpaused) {
      sprintf_s(fall_note, ", falls without a round pause (its round has no time left)");
    } else {
      sprintf_s(fall_note, " (no fall: already lying down)");
    }
    Log("mastery: 0x%08X Push on 0x%08X: slides %.1f m away%s%s%s", attacker_id, target_id, plan.metres, blocker,
        fall_note, retarget);
  }
}

int HitPointsOf(const void* object) {
  return Field<int16_t>(object, nwmain::kObjectCurrentHitPoints) +
         Field<int32_t>(object, nwmain::kObjectTemporaryHitPoints);
}

// A damage effect landed: Slow takes hold if it dealt damage, Vex's mark is
// withdrawn if it dealt none, Push moves the target (any hit, 2024).
void AfterDamage(uint32_t attacker_id, void* target, int damage) {
  const uint32_t target_id = Field<uint32_t>(target, nwmain::kCreatureObjectId);
  const DWORD now = GetTickCount();
  const auto push = g_push_awaiting.find(attacker_id);
  if (push != g_push_awaiting.end() && push->second.target == target_id) {
    const bool fresh = now - push->second.tick <= kAwaitDamageMs;
    g_push_awaiting.erase(push);
    void* attacker = fresh ? engine::GetCreature(attacker_id) : nullptr;
    if (attacker) Push(attacker, attacker_id, target, target_id);
  }
  const auto slow = g_slow_awaiting.find(attacker_id);
  if (slow != g_slow_awaiting.end() && slow->second.target == target_id) {
    const bool fresh = now - slow->second.tick <= kAwaitDamageMs;
    g_slow_awaiting.erase(slow);
    if (fresh && damage > 0 && !RecentlyApplied(&g_slowed, target_id, now)) {  // 2024: no more than 10 ft
      engine::ApplyMovementSpeedDecrease(target, attacker_id, 33, kRoundSeconds, 0);
      if (EffectLogSlot()) Log("mastery: 0x%08X Slow on 0x%08X: speed -33%% for a round", attacker_id, target_id);
    } else if (fresh && damage <= 0 && EffectLogSlot()) {
      Log("mastery: 0x%08X Slow on 0x%08X: no damage dealt, no Slow", attacker_id, target_id);
    }
  }
  const auto vex = g_vex_awaiting.find(attacker_id);
  if (vex != g_vex_awaiting.end() && vex->second.target == target_id) {
    const bool fresh = now - vex->second.tick <= kAwaitDamageMs;
    g_vex_awaiting.erase(vex);
    const auto mark = g_vex.find(attacker_id);
    if (fresh && damage <= 0 && mark != g_vex.end() && mark->second.target == target_id) {
      g_vex.erase(mark);
      if (EffectLogSlot()) Log("mastery: 0x%08X Vex on 0x%08X withdrawn: no damage dealt", attacker_id, target_id);
    }
  }
}

int __fastcall OnApplyDamage(void* handler, void* edx, void* object, void* effect, int loading) {
  const bool awaiting = !g_slow_awaiting.empty() || !g_vex_awaiting.empty() || !g_push_awaiting.empty();
  void* target = object && awaiting ? engine::GetCreature(Field<uint32_t>(object, nwmain::kCreatureObjectId)) : nullptr;
  const int before = target ? HitPointsOf(target) : 0;
  const int result = g_on_apply_damage(handler, edx, object, effect, loading);
  if (target && effect) AfterDamage(Field<uint32_t>(effect, nwmain::kEffectCreator), target, before - HitPointsOf(target));
  return result;
}

}  // namespace

int MasteryCleaveDamage(void* stats, int bonus) {
  if (!MasteryOn() || g_cleave_pending.empty() || !stats) return bonus;
  void* creature = Field<void*>(stats, nwmain::kStatsCreature);
  if (!creature) return bonus;
  const uint32_t id = IdOf(creature);
  const auto pending = g_cleave_pending.find(id);
  if (pending == g_cleave_pending.end()) return bonus;
  if (GetTickCount() - pending->second.tick > kMarkMaxAgeMs ||
      RoundsEnded(id) > pending->second.attacker_rounds + 1) {
    g_cleave_pending.erase(pending);
    return bonus;
  }
  void* round = Field<void*>(creature, nwmain::kCreatureCombatRound);
  const void* attack = round ? g_get_attack(round, nullptr, Field<uint8_t>(round, nwmain::kRoundCurrentAttack)) : nullptr;
  if (!attack || !pending->second.rolled ||
      Field<uint8_t>(round, nwmain::kRoundCurrentAttack) != pending->second.attack_index ||
      Field<uint32_t>(attack, nwmain::kAttackDataTarget) != pending->second.target) {
    return bonus;
  }
  g_cleave_pending.erase(pending);
  const int strength = Field<int8_t>(stats, nwmain::kStatsStrModifier);
  const int nwn_term = rules::NwnMainHandStrengthDamage(strength, engine::CurrentAttackGetsTwoHandedStrength(creature));
  const int term = rules::OffHandDamageAbilityModifier(strength, false);  // only when negative
  if (EffectLogSlot()) {
    Log("mastery: 0x%08X Cleave attack damage: Strength %+d -> %+d", id, nwn_term, term);
  }
  return bonus - nwn_term + term;
}

namespace {

void InstallVexSapRound() {
  InstallHook({"CNWSCombatRound::EndCombatRound (mastery rounds)", nwmain::kEndCombatRound,
               nwmain::kEndCombatRoundPrologue, sizeof(nwmain::kEndCombatRoundPrologue),
               reinterpret_cast<void*>(&EndCombatRound), reinterpret_cast<void**>(&g_end_combat_round)});
  InstallHook({"CNWSEffectListHandler::OnApplyDamage (Vex / Slow / Push need the hit)", nwmain::kOnApplyDamage,
               nwmain::kOnApplyDamagePrologue, sizeof(nwmain::kOnApplyDamagePrologue),
               reinterpret_cast<void*>(&OnApplyDamage), reinterpret_cast<void**>(&g_on_apply_damage)});
  if (g_config.weapon_mastery == rules::RuleVariant::kFaithful) {
    InstallHook({"CNWSCreature::AIUpdate (Push slides the target)", nwmain::kCreatureAIUpdate,
                 nwmain::kCreatureAIUpdatePrologue, sizeof(nwmain::kCreatureAIUpdatePrologue),
                 reinterpret_cast<void*>(&AIUpdate), reinterpret_cast<void**>(&g_ai_update)});
  }
}

}  // namespace

void InstallMasteryRules() {
  if (MasteryOn()) InstallVexSapRound();
  if (!Faithful()) return;
  InstallHook({"CNWSCreatureStats::ResetFeatRemainingUses (mastery swap after a rest)", nwmain::kResetFeatRemainingUses,
               nwmain::kResetFeatRemainingUsesPrologue, sizeof(nwmain::kResetFeatRemainingUsesPrologue),
               reinterpret_cast<void*>(&ResetFeatRemainingUses), reinterpret_cast<void**>(&g_reset_feat_uses)});
}
