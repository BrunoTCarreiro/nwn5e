#pragma once

// Converts NWN's 3e engine values to 2024 values. The hooks read the engine's
// numbers, call these, and hand the result back to the engine.

namespace rules {

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

// Guards the challenge rating read from engine memory: a finite value in
// [0, 60]. Anything else means a wrong read, and the hooks leave the engine's
// numbers alone.
bool IsPlausibleChallengeRating(float challenge_rating);

}  // namespace rules
