#pragma once

// Weapon mastery (D3 who, D4 what), rules_mastery.cpp.

#include <stdint.h>

#include <string>

// Does `creature` have mastery with weapons of `base_item`? Characters only.
bool HasWeaponMasteryWith(void* creature, int base_item);

// The base item of the weapon making the creature's current attack; -1 for
// unarmed or unknown.
int MasteryAttackWeapon(void* creature);

// D3: fills an empty mastery slot with the weapon kind of the attack about to
// be rolled (faithful), and says so in chat.
void NoteWeaponUsed(void* creature);

// D4 Vex / Sap sources for the attack about to be rolled; takes (uses up) the
// marks that apply.
struct MasteryRollSources {
  int advantage = 0;
  int disadvantage = 0;
  std::string note;  // "Vex advantage", "Sap disadvantage" for the log
};
MasteryRollSources TakeMasteryRollSources(void* attacker, uint32_t target_id);

// D4: after an attack roll resolved — a hit with a mastered Vex or Sap weapon
// leaves its mark. `base_item` is the weapon read before the roll.
void MasteryAfterAttackRoll(void* attacker, void* target_object, int base_item);
