// Decision SK6 (PLAN.md), faithful: Knockdown is a 2024 Shove and Disarm does
// nothing (RE-NOTES "Knockdown and Disarm").
//
// NWN: Knockdown / Improved Knockdown (feats 23 / 17) is an attack at -4; on a
// hit (with its damage) the target, at most one size larger than the attacker
// (two with Improved), rolls d20 + Discipline against the attack total and is
// knocked down for 4.5 s on a loss. Disarm / Improved Disarm (9 / 16) is an
// attack at -6 / -4 that provokes; on a hit the same Discipline check decides.
//
// Faithful: a Knockdown attack makes no attack roll and deals no damage (the
// attack becomes an "automatic hit" before ResolveAttackRoll runs, and the
// special path's two damage calls are skipped for it); the target, at most
// one size larger than the attacker for both feats, makes the better of its
// Strength or Dexterity save against 8 + the attacker's Strength modifier + PB
// (the engine's save roll, so the combat log shows it, labelled with the
// ability) and is knocked down by the engine's own effect on a failure.
// Disarm is never added as a special attack, so the attack is an ordinary one
// (no penalty, no opportunity attack, no disarm). NWN mode: NWN's.

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <optional>
#include <string>

#include "config.h"
#include "creature.h"
#include "hooks.h"
#include "log.h"
#include "nwmain_1_69.h"
#include "rules/saves.h"

bool IsPlayerControlled(uint32_t object_id);                       // rules_chat.cpp
void TellPlayer(uint32_t object_id, const std::string& line);  // rules_chat.cpp
void RememberSaveDc2024(uint32_t caster, int dc);              // rules_save_roll.cpp
void SetForcedSaveAbility(std::optional<rules::Ability> ability);  // rules_save_roll.cpp
int SaveBonusFor(void* stats, rules::Ability ability);         // rules_saves.cpp

namespace {

using GetAttackFn = void*(__fastcall*)(void* round, void* edx, uint8_t index);
using TargetFn = void(__fastcall*)(void* creature, void* edx, void* target);
using AddSpecialAttackFn = void(__fastcall*)(void* round, void* edx, uint32_t feat);
using SavingThrowRollFn = uint32_t(__fastcall*)(void* creature, void* edx, uint32_t save, uint32_t dc, uint32_t type,
                                                uint32_t versus, uint32_t feedback, uint32_t feat, uint32_t a7);

const auto g_get_attack = reinterpret_cast<GetAttackFn>(nwmain::kGetAttack);
const auto g_resolve_damage = reinterpret_cast<TargetFn>(nwmain::kResolveMeleeDamage);
const auto g_resolve_post_damage = reinterpret_cast<TargetFn>(nwmain::kResolvePostMeleeDamage);
const auto g_add_special_attack = reinterpret_cast<AddSpecialAttackFn>(nwmain::kAddSpecialAttack);
const auto g_saving_throw_roll = reinterpret_cast<SavingThrowRollFn>(nwmain::kSavingThrowRoll);  // through the hook
int g_log_lines = 0;
bool g_installed = false;  // all four patches in: the Shove is on

bool LogSlot() {
  if (!g_config.log_attacks || g_log_lines >= 300) return false;
  ++g_log_lines;
  return true;
}

uint32_t IdOf(const void* object) { return Field<uint32_t>(object, nwmain::kCreatureObjectId); }

void* CurrentAttack(void* creature) {
  void* round = creature ? Field<void*>(creature, nwmain::kCreatureCombatRound) : nullptr;
  return round ? g_get_attack(round, nullptr, Field<uint8_t>(round, nwmain::kRoundCurrentAttack)) : nullptr;
}

bool IsKnockdown(int special) { return special == nwmain::kFeatKnockdown || special == nwmain::kFeatImprovedKnockdown; }

bool KnockdownAttack(void* creature) {
  void* attack = CurrentAttack(creature);
  return attack && IsKnockdown(Field<uint16_t>(attack, nwmain::kAttackDataSpecial));
}

// In place of the special path's ResolveDamage / post-damage calls (0x742C54 /
// 0x742C5C): none for a Shove; its damage values stay at -1 (none), so the
// hit applies no damage.
void __fastcall ShoveResolveDamage(void* creature, void* edx, void* target) {
  if (g_installed && KnockdownAttack(creature)) return;
  g_resolve_damage(creature, edx, target);
}

void __fastcall ShoveResolvePostDamage(void* creature, void* edx, void* target) {
  if (g_installed && KnockdownAttack(creature)) return;
  g_resolve_post_damage(creature, edx, target);
}

// The Shove: 0 = knocked down (the engine applies its effect), 1 = resisted or
// immune, 2 = too large (the engine's own "too large" feedback).
int __cdecl ShoveResolve(void* attacker, void* target, void* attack, int special) {
  const uint32_t attacker_id = IdOf(attacker);
  const uint32_t target_id = IdOf(target);
  const bool player = IsPlayerControlled(attacker_id) || IsPlayerControlled(target_id);
  const int attacker_size = static_cast<int>(Field<uint32_t>(attacker, nwmain::kCreatureSize));
  const int target_size = static_cast<int>(Field<uint32_t>(target, nwmain::kCreatureSize));
  if (!rules::ShoveSizeAllowed(attacker_size, target_size)) {
    if (player && LogSlot()) {
      Log("shove: 0x%08X on 0x%08X: too large (size %d against %d; at most one larger)", attacker_id, target_id,
          target_size, attacker_size);
    }
    return 2;
  }
  void* attacker_stats = Field<void*>(attacker, nwmain::kCreatureStats);
  void* target_stats = Field<void*>(target, nwmain::kCreatureStats);
  if (!attacker_stats || !target_stats) return 1;
  const CreatureFacts facts = ReadCreatureFacts(attacker_stats);
  const int pb = rules::ProficiencyBonusForCreature(facts.character, facts.level, facts.challenge_rating);
  const int strength = Field<int8_t>(attacker_stats, nwmain::kStatsStrModifier);
  const int dc = rules::ShoveDc(strength, pb);
  const int str_save = SaveBonusFor(target_stats, rules::Ability::kStrength);
  const int dex_save = SaveBonusFor(target_stats, rules::Ability::kDexterity);
  const rules::Ability ability = rules::ShoveSaveAbility(str_save, dex_save);
  RememberSaveDc2024(attacker_id, dc);
  if (ability == rules::Ability::kStrength) SetForcedSaveAbility(rules::Ability::kStrength);
  const uint32_t save = g_saving_throw_roll(target, nullptr, nwmain::kSaveReflex, static_cast<uint32_t>(dc), 0,
                                            attacker_id, 1, 0, 1) &
                        0xFF;
  SetForcedSaveAbility(std::nullopt);
  const char* ability_name = ability == rules::Ability::kStrength ? "Strength" : "Dexterity";
  const char* outcome = save == 0 ? "failed - knocked down" : save == 2 ? "immune" : "made - stays up";
  if (player) {
    char line[200];
    sprintf_s(line, "nwn5e: Shove (%s): %s save %+d vs DC %d (8 + Str %+d + PB %+d) %s",
              special == nwmain::kFeatImprovedKnockdown ? "Improved Knockdown" : "Knockdown", ability_name,
              ability == rules::Ability::kStrength ? str_save : dex_save, dc, strength, pb, outcome);
    if (IsPlayerControlled(attacker_id)) TellPlayer(attacker_id, line);
    if (IsPlayerControlled(target_id) && target_id != attacker_id) TellPlayer(target_id, line);
    if (LogSlot()) {
      Log("shove: 0x%08X on 0x%08X: no attack roll, no damage; %s save (Str %+d, Dex %+d) vs DC %d: %s",
          attacker_id, target_id, ability_name, str_save, dex_save, dc, outcome);
    }
  }
  (void)attack;
  return save == 0 ? 0 : 1;
}

// Jumped to from the Knockdown handler at 0x742F8F (in place of its size check
// and Discipline roll): edi = attacker, ebp = target creature, esi = attack
// data, cx = the special attack (0x17 / 0x11); ebx, esi, edi, ebp survive the
// call. 0 -> 0x742FF6 (build and queue the knockdown effect), 1 -> 0x742F5D
// (resisted, result 5), 2 -> bl = 6 and the engine's "too large" feedback for
// the feat (0x743027 / 0x743076).
__declspec(naked) void ShoveStub() {
  __asm {
    movzx ecx, cx
    push ecx
    push esi
    push ebp
    push edi
    call ShoveResolve
    add esp, 16
    test eax, eax
    jnz not_down
    push 0x00742FF6
    ret
  not_down:
    dec eax
    jnz too_large
    push 0x00742F5D
    ret
  too_large:
    mov bl, 6
    cmp word ptr [esi + 0x52], 0x17
    jne improved
    push 0x00743027
    ret
  improved:
    push 0x00743076
    ret
  }
}

// In place of the feat-use handler's AddSpecialAttack call (0x657AEC): Disarm
// is not added, so the action is an ordinary attack.
void __fastcall AddSpecialAttackGate(void* round, void* edx, uint32_t feat) {
  const int special = static_cast<int>(feat & 0xFFFF);
  if (special == nwmain::kFeatDisarm || special == nwmain::kFeatImprovedDisarm) return;
  g_add_special_attack(round, edx, feat);
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

// From the ResolveAttackRoll hook (rules_advantage.cpp), before anything else:
// a faithful Knockdown makes no attack roll — the attack is an automatic hit
// (result 7, d20 and modifier 0) and the engine's roll is not called.
bool ShoveInsteadOfAttackRoll(void* creature) {
  if (!g_installed) return false;
  void* attack = CurrentAttack(creature);
  if (!attack || !IsKnockdown(Field<uint16_t>(attack, nwmain::kAttackDataSpecial))) return false;
  SetField<uint8_t>(attack, nwmain::kAttackDataD20, 0);
  SetField<int32_t>(attack, nwmain::kAttackDataModifier, 0);
  SetField<uint8_t>(attack, nwmain::kAttackDataResult, nwmain::kAttackResultAutomaticHit);
  return true;
}

void InstallShoveRules() {
  if (g_config.knockdown != rules::RuleVariant::kFaithful) return;
  int applied = 0;
  applied += RedirectCall("knockdown: Shove deals no damage (faithful)", nwmain::kSpecialDamageCall,
                          nwmain::kResolveMeleeDamage, reinterpret_cast<void*>(&ShoveResolveDamage))
                 ? 1
                 : 0;
  applied += RedirectCall("knockdown: Shove deals no damage, post-damage (faithful)", nwmain::kSpecialPostDamageCall,
                          nwmain::kResolvePostMeleeDamage, reinterpret_cast<void*>(&ShoveResolvePostDamage))
                 ? 1
                 : 0;
  uint8_t jump[6] = {0xe9, 0, 0, 0, 0, 0x90};
  const int32_t rel =
      static_cast<int32_t>(reinterpret_cast<uintptr_t>(&ShoveStub) - (nwmain::kKnockdownSizeCheck + nwmain::kCallLength));
  memcpy(jump + 1, &rel, sizeof(rel));
  applied += PatchBytes("knockdown: a Strength or Dexterity save instead of Discipline (faithful)",
                        nwmain::kKnockdownSizeCheck, nwmain::kKnockdownSizeCheckBytes, jump, sizeof(jump))
                 ? 1
                 : 0;
  applied += RedirectCall("disarm: an ordinary attack (faithful)", nwmain::kAddSpecialAttackCall,
                          nwmain::kAddSpecialAttack, reinterpret_cast<void*>(&AddSpecialAttackGate))
                 ? 1
                 : 0;
  g_installed = applied == 4;
  Log("knockdown: %d of 4 faithful patches applied%s", applied, g_installed ? "" : " - Shove off");
}
