#pragma once

// Phase 3 CM9 / CM10 (PLAN.md): the 2024 class features, level by level, and
// each class's SRD 5.2 subclass (granted at class level 3). The status of
// each feature in nwn5e is part of the table, so the game can show what
// works. Filled one class at a time, each with its own brief.

#include <vector>

#include "rules/combat.h"

namespace rules {

constexpr int kSubclassLevel = 3;

// The SRD 5.2 subclass of each class (one per class).
const char* Subclass2024(CharacterClass character_class);

struct ClassFeature {
  int level;
  const char* name;
  bool subclass;       // a feature of the SRD subclass
  const char* status;  // "built", "built: <how>", or the decision / phase it waits for
};

// The class's features up to `level` (all of them for level 20); empty for a
// class whose brief hasn't been written yet.
std::vector<ClassFeature> ClassFeatures2024(CharacterClass character_class, int level);

// Has the class's brief been written (its features listed)?
bool HasFeatureList(CharacterClass character_class);

}  // namespace rules
