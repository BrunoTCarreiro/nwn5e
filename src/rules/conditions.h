#pragma once

// Conditions (PLAN.md "Conditions — decided by the owner", 2026-10-19): what
// a creature's conditions do to saves and attack rolls, in the faithful
// (2024) and NWN-mode versions. The engine side reads the conditions from the
// creature's effects; these functions only decide.

#include "rules/modes.h"
#include "rules/saves.h"

namespace rules {

// The conditions one creature has that change its saves or attacks.
struct Conditions {
  bool stunned = false;     // NWN state 6
  bool paralyzed = false;   // NWN state 8 (Hold Person and the like)
  bool asleep = false;      // NWN state 9 (2024: Unconscious)
  bool restrained = false;  // NWN entangle (Entangle, Web, ...)
  bool poisoned = false;    // an NWN poison effect is on it
  bool frightened_source_in_sight = false;  // frightened, and it can see the source
  bool concealed = false;   // an NWN concealment effect (Blur, Displacement, ...)
};

enum class SaveOutcome { kRoll, kAutoFail };

// CN2/CN3: a stunned, paralyzed or unconscious creature automatically fails
// Strength and Dexterity saves. Stunned: both modes (in NWN mode the save is
// Reflex, i.e. `ability` Dexterity). Paralyzed and unconscious: faithful only.
// `ability` is the save's 2024 ability (NWN mode: the NWN save's mapping).
SaveOutcome ConditionSaveOutcome(const Conditions& creature, Ability ability, RuleVariant variant);

// CN1: a restrained creature has disadvantage on Dexterity saves (faithful).
int ConditionSaveDisadvantage(const Conditions& creature, Ability ability, RuleVariant variant);

// Advantage / disadvantage sources that conditions add to one attack roll,
// counted like the situational ones (rules::SituationalRollMode extras).
struct RollSources {
  int advantage = 0;
  int disadvantage = 0;
};

// Faithful only (NWN mode: none — NWN's own terms stay):
//   target paralyzed or unconscious (CN2), restrained (CN1): advantage;
//   target concealed (CN6, NWN's miss chance removed): disadvantage;
//   attacker restrained (CN1), poisoned (CN4), frightened with the source in
//   sight (CN5): disadvantage.
RollSources ConditionAttackSources(const Conditions& attacker, const Conditions& target, RuleVariant variant);

// CN2 (faithful): a hit on a paralyzed or unconscious creature from an
// attacker within 5 ft (a melee attack) is a critical hit.
bool ConditionForcesCritical(const Conditions& target, bool melee, RuleVariant variant);

}  // namespace rules
