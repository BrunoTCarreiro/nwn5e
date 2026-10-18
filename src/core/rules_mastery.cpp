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
#include <string.h>

#include <windows.h>

#include <string>
#include <unordered_map>
#include <vector>

#include "config.h"
#include "creature.h"
#include "hooks.h"
#include "log.h"
#include "log_once.h"
#include "mastery.h"
#include "nwmain_1_69.h"
#include "rules/nwn_conversion.h"
#include "rules/weapons.h"

void TellPlayer(uint32_t object_id, const std::string& line);  // rules_chat.cpp

namespace {

struct ExoString {
  const char* text;
  uint32_t length;  // including the null
};

using GetIntFn = int(__fastcall*)(void* table, void* edx, ExoString* name);
using SetIntFn = void(__fastcall*)(void* table, void* edx, ExoString* name, int value, int flag);
using ResetFeatUsesFn = void(__fastcall*)(void* stats, void* edx);
using HasFeatFn = int(__fastcall*)(void* stats, void* edx, int feat);
using GetAttackFn = void*(__fastcall*)(void* round, void* edx, int index);
using GetItemFn = void*(__fastcall*)(void* owner, void* edx, int which);

const auto g_get_int = reinterpret_cast<GetIntFn>(nwmain::kVarTableGetInt);
const auto g_set_int = reinterpret_cast<SetIntFn>(nwmain::kVarTableSetInt);
const auto g_has_feat = reinterpret_cast<HasFeatFn>(nwmain::kHasFeat);
const auto g_get_attack = reinterpret_cast<GetAttackFn>(nwmain::kGetAttack);
const auto g_get_current_attack_weapon = reinterpret_cast<GetItemFn>(nwmain::kGetCurrentAttackWeapon);
ResetFeatUsesFn g_reset_feat_uses = nullptr;

LogOnce g_logged_slots(256);

constexpr int kMaxSlots = 6;
const char* const kSlotVars[kMaxSlots] = {"nwn5e_mastery_1", "nwn5e_mastery_2", "nwn5e_mastery_3",
                                          "nwn5e_mastery_4", "nwn5e_mastery_5", "nwn5e_mastery_6"};
const char kSwapVar[] = "nwn5e_mastery_swap";

bool Faithful() { return g_config.weapon_mastery == rules::RuleVariant::kFaithful; }

void* VarTable(void* creature) { return static_cast<uint8_t*>(creature) + nwmain::kObjectVarTable; }

int GetVar(void* creature, const char* name) {
  ExoString text = {name, static_cast<uint32_t>(strlen(name) + 1)};
  return g_get_int(VarTable(creature), nullptr, &text);
}

void SetVar(void* creature, const char* name, int value) {
  ExoString text = {name, static_cast<uint32_t>(strlen(name) + 1)};
  g_set_int(VarTable(creature), nullptr, &text, value, 0);
}

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

void __fastcall EndCombatRound(void* round, void* edx) {
  void* owner = Field<void*>(round, nwmain::kRoundOwner);
  if (owner) ++g_rounds_ended[IdOf(owner)];
  g_end_combat_round(round, edx);
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
  if (mastery != rules::Mastery::kVex && mastery != rules::Mastery::kSap) return;
  void* round = Field<void*>(attacker, nwmain::kCreatureCombatRound);
  const void* attack = round ? g_get_attack(round, nullptr, Field<uint8_t>(round, nwmain::kRoundCurrentAttack)) : nullptr;
  if (!attack || !rules::MasteryAttackHit(Field<uint8_t>(attack, nwmain::kAttackDataResult))) return;
  if (!HasWeaponMasteryWith(attacker, base_item)) return;
  const uint32_t attacker_id = IdOf(attacker);
  const uint32_t target_id = Field<uint32_t>(target_object, nwmain::kCreatureObjectId);
  const uint32_t rounds = RoundsEnded(attacker_id);
  if (mastery == rules::Mastery::kVex) {
    g_vex[attacker_id] = VexMark{target_id, rounds, GetTickCount()};
  } else {
    g_sap[target_id] = SapMark{attacker_id, rounds, GetTickCount()};
  }
  if (EffectLogSlot()) {
    Log("mastery: 0x%08X hit 0x%08X with %s: %s", attacker_id, target_id, Label(base_item).c_str(),
        mastery == rules::Mastery::kVex ? "Vex (advantage on its next attack at that target)"
                                        : "Sap (target's next attack has disadvantage)");
  }
}

namespace {

void InstallVexSapRound() {
  InstallHook({"CNWSCombatRound::EndCombatRound (mastery rounds)", nwmain::kEndCombatRound,
               nwmain::kEndCombatRoundPrologue, sizeof(nwmain::kEndCombatRoundPrologue),
               reinterpret_cast<void*>(&EndCombatRound), reinterpret_cast<void**>(&g_end_combat_round)});
}

}  // namespace

void InstallMasteryRules() {
  if (MasteryOn()) InstallVexSapRound();
  if (!Faithful()) return;
  InstallHook({"CNWSCreatureStats::ResetFeatRemainingUses (mastery swap after a rest)", nwmain::kResetFeatRemainingUses,
               nwmain::kResetFeatRemainingUsesPrologue, sizeof(nwmain::kResetFeatRemainingUsesPrologue),
               reinterpret_cast<void*>(&ResetFeatRemainingUses), reinterpret_cast<void**>(&g_reset_feat_uses)});
}
