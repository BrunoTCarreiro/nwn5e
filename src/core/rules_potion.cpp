// Owner request (2026-10-19): after a player character drinks a potion in a
// fight it goes back to attacking — NWN drops the attack order when an item
// is used, so the player had to click the enemy again. [rules]
// resume_attack_after_potion.
//
// The drink is noticed where the item-use action handles potions: the store
// `mov dword [esi+0xA78], 1` at 0x67B9EA, reached for every potion (base items
// 49, 54, 104) whatever the difficulty, becomes a call to a stub that does
// the store and records the drinker. Its last attack target comes from the
// attack roll (NotePotionResumeTarget). Once the drink is over and it has no
// orders, the attack is reissued as a click on that target (or, if it fell,
// the nearest enemy it sees within 10 m).

#include <stdint.h>
#include <string.h>

#include <windows.h>

#include <unordered_map>

#include "config.h"
#include "creature.h"
#include "engine_queries.h"
#include "hooks.h"
#include "log.h"
#include "nwmain_1_69.h"

bool IsPlayerControlled(uint32_t object_id);  // rules_chat.cpp

namespace {

struct Noted {
  uint32_t target;
  DWORD tick;
};
std::unordered_map<uint32_t, Noted> g_last_target;  // player character -> its last attack's target
std::unordered_map<uint32_t, Noted> g_resume;       // drank a potion -> the target to go back to
constexpr DWORD kLastTargetMaxMs = 30000;           // a fight's last attack, not an old one
constexpr DWORD kResumeWindowMs = 10000;            // the drink and its animation
constexpr float kFallbackRange = 10.0f;
int g_log_lines = 0;

bool LogSlot() { return g_config.log_attacks && g_log_lines < 200 && ++g_log_lines; }

uint32_t IdOf(const void* creature) { return Field<uint32_t>(creature, nwmain::kCreatureObjectId); }

void __stdcall PotionDrunk(void* creature) {
  if (!creature) return;
  const uint32_t id = IdOf(creature);
  const auto last = g_last_target.find(id);
  if (last == g_last_target.end() || GetTickCount() - last->second.tick > kLastTargetMaxMs) return;
  g_resume[id] = Noted{last->second.target, GetTickCount()};
  if (LogSlot()) Log("potion: 0x%08X drank a potion mid-fight; will go back to attacking 0x%08X", id, last->second.target);
}

// Replaces the 10-byte store at 0x67B9EA (called, so it returns there + 5;
// the 5 nops after the call finish the old instruction's length).
__declspec(naked) void PotionStub() {
  __asm {
    mov dword ptr [esi + 0xA78], 1
    pushad
    pushfd
    push esi
    call PotionDrunk
    popfd
    popad
    ret
  }
}

}  // namespace

void NotePotionResumeTarget(void* attacker, uint32_t target_id) {
  if (!g_config.resume_attack_after_potion || !attacker || target_id == nwmain::kObjectInvalid) return;
  const uint32_t id = IdOf(attacker);
  if (!IsPlayerControlled(id)) return;
  g_last_target[id] = Noted{target_id, GetTickCount()};
}

// From the creature-update hook (creature_update.cpp).
void PotionCreatureUpdate(void* creature) {
  if (g_resume.empty()) return;
  const auto it = g_resume.find(IdOf(creature));
  if (it == g_resume.end()) return;
  if (GetTickCount() - it->second.tick > kResumeWindowMs || engine::IsDeadOrDying(creature)) {
    g_resume.erase(it);
    return;
  }
  if (!engine::HasNoOrders(creature)) return;  // still drinking (or given another order)
  const uint32_t id = it->first;
  uint32_t target = it->second.target;
  g_resume.erase(it);
  void* last = engine::GetCreature(target);
  const bool last_stands = last && !engine::IsDeadOrDying(last) && engine::IsEnemy(creature, target) &&
                           engine::Sees(creature, target);
  if (!last_stands) target = engine::NearestSeenEnemy(creature, kFallbackRange);
  if (target == nwmain::kObjectInvalid) {
    if (LogSlot()) Log("potion: 0x%08X done drinking; no enemy to go back to", id);
    return;
  }
  engine::AttackAsClick(creature, target);
  if (LogSlot()) {
    Log("potion: 0x%08X done drinking; attacks 0x%08X again%s", id, target,
        last_stands ? "" : " (its last target fell: the nearest enemy it sees)");
  }
}

void InstallPotionRules() {
  if (!g_config.resume_attack_after_potion) return;
  uint8_t patch[10] = {0xe8, 0, 0, 0, 0, 0x90, 0x90, 0x90, 0x90, 0x90};
  const int32_t rel = static_cast<int32_t>(reinterpret_cast<uintptr_t>(&PotionStub) -
                                           (nwmain::kPotionDrunkStore + nwmain::kCallLength));
  memcpy(patch + 1, &rel, sizeof(rel));
  if (!PatchBytes("potions: back to attacking after drinking", nwmain::kPotionDrunkStore,
                  nwmain::kPotionDrunkStoreBytes, patch, sizeof(patch))) {
    g_config.resume_attack_after_potion = false;
  }
}
