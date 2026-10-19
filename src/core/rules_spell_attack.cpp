// Decision SK10 (PLAN.md), faithful: spell attacks the 2024 way. NWN's
// TouchAttackMelee / TouchAttackRanged (script commands 146 / 147, one handler
// 0x6F1170) roll their own d20 with the melee / ranged attack bonus against
// touch AC, with NWN's threat and confirmation roll (RE-NOTES "Touch
// attacks"). Faithful, for a creature casting on a creature:
// - the bonus is the proficiency bonus + the casting ability modifier of the
//   spell being cast (the class slot or item it is cast from, as the spell DC
//   reads it, C5; a scroll's 2024 attack bonus for items) + the touch-attack
//   effect bonuses the engine adds (Bless and the like);
// - against full AC (2024 has no touch AC);
// - the d20 with advantage / disadvantage as for any attack (NWN's
//   situational terms, close combat for ranged, conditions; Halfling Luck);
// - a natural 20 is a critical hit, no confirmation roll (the spell script
//   still decides the critical damage — Phase 4).
// A caster that is not a creature (a trap, a placeable) keeps NWN's roll.
// NWN mode: NWN's (with the global attack conversion).

#include <stdint.h>
#include <string.h>

#include "config.h"
#include "creature.h"
#include "hooks.h"
#include "log.h"
#include "log_once.h"
#include "nwmain_1_69.h"
#include "rules/saves.h"

bool IsPlayerControlled(uint32_t object_id);                                // rules_chat.cpp
bool SpellAttackCasting(void* creature, int* bonus, const char** source);  // rules_save_roll.cpp
int RollSpellAttackD20(void* caster, void* target, bool ranged);          // rules_advantage.cpp

namespace {

using MeleeAttackBonusFn = int(__fastcall*)(void* stats, void* edx, int offhand, int a, int b);
using RangedAttackBonusFn = int(__fastcall*)(void* stats, void* edx, int a, int b);
using TotalEffectBonusFn = int(__fastcall*)(void* creature, void* edx, int type, void* vs, int a3, int a4, int a5,
                                            int a6, int subtype, int a8, int a9);
using VirtualCastFn = void*(__fastcall*)(void* object, void* edx);

// Through their addresses: the engine's NWN-mode values, converted by the
// attack-proficiency hooks, for the log only.
const auto g_melee_attack_bonus = reinterpret_cast<MeleeAttackBonusFn>(nwmain::kGetMeleeAttackBonus);
const auto g_ranged_attack_bonus = reinterpret_cast<RangedAttackBonusFn>(nwmain::kGetRangedAttackBonus);
const auto g_total_effect_bonus = reinterpret_cast<TotalEffectBonusFn>(nwmain::kGetTotalEffectBonus);
LogOnce g_logged(300);

void* AsCreature(void* object) {
  if (!object) return nullptr;
  void** vtable = *reinterpret_cast<void***>(object);
  return reinterpret_cast<VirtualCastFn>(vtable[nwmain::kVtableAsCreature / sizeof(void*)])(object, nullptr);
}

// The 2024 spell attack bonus of the creature behind `stats`, or `engine_value`
// when its numbers are not known.
int SpellAttackBonus(void* stats, int engine_value, const char* which) {
  void* creature = stats ? Field<void*>(stats, nwmain::kStatsCreature) : nullptr;
  int casting = 0;
  const char* source = "";
  if (!creature || !SpellAttackCasting(creature, &casting, &source)) return engine_value;
  // GetMeleeAttackBonus(touch) adds the touch-attack effect bonuses (type 6).
  const int effects = g_total_effect_bonus(creature, nullptr, nwmain::kEffectBonusTypeTouchAttack, nullptr, 0, 0, 0,
                                           0, 0xFF, 0xFF, 0);
  const int bonus = casting + effects;
  if (g_config.log_attacks &&
      g_logged.First(LogOnce::Key(ObjectIdOf(stats), engine_value, bonus) ^ (which[0] == 'r' ? 1ull << 63 : 0))) {
    Log("spell attack: object 0x%08X %s touch (%s): NWN %+d -> 2024 %+d (casting %+d, effects %+d)",
        ObjectIdOf(stats), which, source, engine_value, bonus, casting, effects);
  }
  return bonus;
}

// In place of the handler's GetMeleeAttackBonus(0, 1, 1) / GetRangedAttackBonus(1, 1).
int __fastcall TouchMeleeBonus(void* stats, void* edx, int offhand, int a, int b) {
  return SpellAttackBonus(stats, g_melee_attack_bonus(stats, edx, offhand, a, b), "melee");
}

int __fastcall TouchRangedBonus(void* stats, void* edx, int a, int b) {
  return SpellAttackBonus(stats, g_ranged_attack_bonus(stats, edx, a, b), "ranged");
}

int g_roll_lines = 0;

// The d20 (with the attack roll mode), and a log line with what the handler
// will compare: spells cast from items often hide the touch attack's own
// feedback (bDisplayFeedback = FALSE).
int __stdcall TouchAttackD20(void* caster_object, void* target_object, int command, int bonus, int armor_class) {
  void* caster = AsCreature(caster_object);
  void* target = AsCreature(target_object);
  const bool ranged = command != nwmain::kCommandTouchAttackMelee;
  const int d20 = RollSpellAttackD20(caster, target, ranged);
  if (g_config.log_attacks && caster && target && g_roll_lines < 300) {
    const uint32_t caster_id = Field<uint32_t>(caster, nwmain::kCreatureObjectId);
    const uint32_t target_id = Field<uint32_t>(target, nwmain::kCreatureObjectId);
    if (IsPlayerControlled(caster_id) || IsPlayerControlled(target_id)) {
      ++g_roll_lines;
      const int total = d20 + bonus;
      Log("spell attack roll: 0x%08X vs 0x%08X (%s touch): d20 %d %+d = %d vs AC %d -> %s", caster_id, target_id,
          ranged ? "ranged" : "melee", d20, bonus, total, armor_class,
          d20 == 20 ? "critical hit (natural 20)" : d20 == 1 ? "miss (natural 1)" : total >= armor_class ? "hit" : "miss");
    }
  }
  return d20;
}

// In place of the handler's RollDice(1, 20) at 0x6F12E1 (ecx = rules object, two
// stack arguments, ret 8). There ebx = the attack bonus and esi = the AC; the
// handler's frame: caster object [esp+0x10], target object [esp+0x14], command
// [esp+0x3C], +8 for the two arguments and +4 for our return address (on entry:
// 0x1C / 0x20 / 0x48).
__declspec(naked) void TouchAttackD20Stub() {
  __asm {
    push esi
    push ebx
    mov eax, dword ptr [esp + 0x50]
    push eax
    mov eax, dword ptr [esp + 0x2C]
    push eax
    mov eax, dword ptr [esp + 0x2C]
    push eax
    call TouchAttackD20
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

}  // namespace

void InstallSpellAttackRules() {
  if (g_config.spell_attacks != rules::RuleVariant::kFaithful) return;
  int applied = 0;
  applied += RedirectCall("spell attacks: melee touch bonus = PB + casting ability (faithful)",
                          nwmain::kTouchMeleeBonusCall, nwmain::kGetMeleeAttackBonus,
                          reinterpret_cast<void*>(&TouchMeleeBonus))
                 ? 1
                 : 0;
  applied += RedirectCall("spell attacks: ranged touch bonus = PB + casting ability (faithful)",
                          nwmain::kTouchRangedBonusCall, nwmain::kGetRangedAttackBonus,
                          reinterpret_cast<void*>(&TouchRangedBonus))
                 ? 1
                 : 0;
  static const uint8_t kTouchFlag[1] = {0x01};
  static const uint8_t kFullAc[1] = {0x00};
  applied += PatchBytes("spell attacks: against full AC, not touch AC (faithful)", nwmain::kTouchAcFlagPush + 1,
                        kTouchFlag, kFullAc, sizeof(kFullAc))
                 ? 1
                 : 0;
  applied += RedirectCall("spell attacks: d20 with advantage / disadvantage (faithful)", nwmain::kTouchD20Call,
                          nwmain::kRollDice, reinterpret_cast<void*>(&TouchAttackD20Stub))
                 ? 1
                 : 0;
  int count = 4;
  if (g_config.crit_no_confirmation) {
    static const uint8_t kSetge[3] = {0x0f, 0x9d, 0xc1};  // setge cl
    static const uint8_t kAlways[3] = {0xb1, 0x01, 0x90};  // mov cl, 1
    applied += PatchBytes("spell attacks: a natural 20 is a critical hit, no confirmation (faithful)",
                          nwmain::kTouchConfirmSetge, kSetge, kAlways, sizeof(kAlways))
                   ? 1
                   : 0;
    ++count;
  }
  Log("spell attacks: %d of %d faithful patches applied", applied, count);
}
