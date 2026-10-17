#pragma once

// Phase 1b (proposed — PLAN.md "Phase 1b proposal", owner to decide): NWN's
// situational attack modifiers become 2024 advantage/disadvantage. The engine
// adds them as flat 3e bonuses inside GetAttackModifierVersus (RE-NOTES); the
// hook works out which ones it applied, takes their total back out and rolls
// the d20 with the matching roll mode instead.

#include "rules/d20.h"

namespace rules {

// The situational terms NWN applied to one attack roll.
struct NwnSituationalTerms {
  bool ranged = false;
  bool target_blind = false;        // +2 (not against Blind-Fight in melee)
  bool attacker_invisible = false;  // +2: the target can't see the attacker (same exception)
  bool cant_see_target = false;     // -4
  bool target_moving = false;       // -2, ranged attacks only
  bool target_prone = false;        // melee +4, ranged -4
  bool target_stunned = false;      // +2
  bool target_flanked = false;      // +2 (ranged: within 10 m)
  bool ranged_in_melee = false;     // -4: target within its melee reach, no Point Blank Shot
};

// The flat total those terms added to the engine's attack modifier.
int NwnSituationalTotal(const NwnSituationalTerms& terms);

// The 2024 roll mode for the same situation. Advantage: target blinded or
// stunned, attacker unseen, prone target in melee. Disadvantage: target
// unseen, prone target at range, ranged attack in close combat. Nothing for a
// moving target (no 2024 rule) or flanking (an optional 2024 rule; proposed
// off). Sources cancel as in ResolveRollMode.
RollMode SituationalRollMode(const NwnSituationalTerms& terms);

}  // namespace rules
