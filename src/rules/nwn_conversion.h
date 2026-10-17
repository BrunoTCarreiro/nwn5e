#pragma once

// Converts NWN's 3e engine values to 2024 values. The hooks read the engine's
// numbers, call these, and hand the result back to the engine.

#include <optional>
#include <vector>

#include "rules/combat.h"

namespace rules {

// NWN class id (classes.2da row) -> 2024 class. The base classes are rows 0-10
// (Barbarian, Bard, Cleric, Druid, Fighter, Monk, Paladin, Ranger, Rogue,
// Sorcerer, Wizard). Prestige classes (27-37, 41) map to the class they would
// be a 2024 subclass of (decision 10). Monster/racial classes (11-26, 38) have
// no 2024 class.
std::optional<CharacterClass> ClassFromNwnId(int nwn_class_id);

struct NwnClassSlot {
  int nwn_class_id;
  int levels;
};

// Main-hand attacks per round. Player characters make the attacks of the
// 2024 Attack action (Extra Attack from their classes, prestige levels added
// to the parent class); other creatures keep the engine's count, standing in
// for a 5e Multiattack.
int MainHandAttacksPerRound(bool is_player_character, const std::vector<NwnClassSlot>& classes, int engine_attacks);

// Decision 11: player characters and henchmen use the character rules (gear
// AC, proficiency by level, Extra Attack from their classes). Animal
// companions, familiars, summons and dominated creatures keep the creature
// rules — they have stat blocks, not gear. NWN associate types: 1 henchman,
// 2 animal companion, 3 familiar, 4 summoned, 5 dominated (0 = none).
constexpr int kAssociateTypeHenchman = 1;
bool UsesCharacterRules(bool is_player_character, int associate_type);

// Player characters use their total level; every other creature (monsters,
// NPCs, henchmen) uses its challenge rating.
int ProficiencyBonusForCreature(bool is_player_character, int total_level, double challenge_rating);

// NWN's attack modifier is built on the 3e base attack bonus; swap that part
// for the proficiency bonus and keep everything else the engine added
// (ability modifier, weapon enhancement, effects, feats).
int ConvertAttackModifier(int engine_attack_modifier, int base_attack_bonus, int proficiency_bonus);

// Player characters keep the engine's AC; other creatures get the expected AC
// for their challenge rating (rules::ExpectedMonsterArmorClass).
int ConvertArmorClass(bool is_player_character, int engine_armor_class, double challenge_rating);

// 2024 critical hits on NWN's crit functions. The engine answers 21 / x0 for a
// hand that has nothing to attack with (seen for the empty off-hand when the
// character sheet's combat information is computed); those stay as they are.
// Every real attack threatens only on a natural 20 and crits for x2.
int ConvertCriticalThreatFloor(int engine_threat_floor);
int ConvertCriticalMultiplier(int engine_multiplier);

// Which part of a critical's damage the engine is multiplying. NWN repeats
// both its dice rolls and its flat bonuses `multiplier` times on a critical;
// 2024 rolls the damage dice twice and adds the flat bonuses once.
enum class CritDamagePart { kDice, kFlatBonus };
int CriticalMultiplierFor(CritDamagePart part, int engine_multiplier);

// Decision 14: attacks of the round still to come after the one with index
// `current_attack_index` (0-based) out of `total_attacks`. When an attack
// kills its target and this is above 0, the next attack moves to the nearest
// enemy in reach instead of being lost.
int RemainingAttacksInRound(int total_attacks, int current_attack_index);

// Guards the challenge rating read from engine memory: a finite value in
// [0, 60]. Anything else means a wrong read, and the hooks leave the engine's
// numbers alone.
bool IsPlausibleChallengeRating(float challenge_rating);

}  // namespace rules
