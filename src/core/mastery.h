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

// The creature's ended combat rounds (while weapon mastery is on).
uint32_t CombatRoundsEnded(uint32_t id);

// Called with a creature's stats each time the engine restores its feat uses
// after a rest (ResetFeatRemainingUses), for uses nwn5e keeps itself.
void AddRestListener(void (*listener)(void* stats));

// Fighter F6 Studied Attacks (rules_fighter.cpp): the advantage an earlier
// miss gives at this target (used up), and the mark a miss leaves.
MasteryRollSources TakeStudiedAttacks(void* attacker, uint32_t target_id);
void StudiedAttacksAfterAttackRoll(void* attacker, void* target_object);

// D4: after an attack roll resolved — a hit with a mastered Vex or Sap weapon
// leaves its mark. `base_item` is the weapon read before the roll.
void MasteryAfterAttackRoll(void* attacker, void* target_object, int base_item);
// Push's work on each creature update (creature_update.cpp).
void MasteryCreatureUpdate(void* creature);

// D4 Cleave: the damage bonus of the extra attack a Cleave hit added has no
// ability modifier unless it is negative. Called from the damage bonus hook
// (rules_two_weapon.cpp) for main-hand attacks; returns `bonus` unchanged for
// every other attack.
int MasteryCleaveDamage(void* stats, int bonus);
