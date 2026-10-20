// Decisions CM9 / CM10 (PLAN.md): the frame for the 2024 class features —
// `!features` lists a character's features by class level with what nwn5e
// does with each (rules::ClassFeatures2024), and its SRD subclass, granted at
// class level 3 (the prestige classes are off in faithful: rules_progression).
// The features themselves are built class by class, each from its brief.

#include <string>
#include <vector>

#include "config.h"
#include "creature.h"
#include "nwmain_1_69.h"
#include "rules/class_features.h"
#include "rules/nwn_conversion.h"

namespace {

const char* ClassName(rules::CharacterClass c) {
  static const char* const kNames[] = {"Barbarian", "Bard",   "Cleric", "Druid",    "Fighter", "Monk",
                                       "Paladin",   "Ranger", "Rogue",  "Sorcerer", "Warlock", "Wizard"};
  return kNames[static_cast<int>(c)];
}

}  // namespace

// !features
std::vector<std::string> FeaturesCommand(void* creature, const std::string& /*args*/) {
  if (g_config.class_features != rules::RuleVariant::kFaithful) {
    return {"nwn5e: !features shows the 2024 class features (class_features=faithful)."};
  }
  void* stats = Field<void*>(creature, nwmain::kCreatureStats);
  std::vector<std::string> lines;
  for (const rules::ClassLevels& c : rules::GroupClassLevels(ReadClassSlots(stats))) {
    const int level = c.levels < 20 ? c.levels : 20;
    std::string head = "nwn5e: " + std::string(ClassName(c.character_class)) + " " + std::to_string(level);
    head += level >= rules::kSubclassLevel ? std::string(" - ") + rules::Subclass2024(c.character_class)
                                           : std::string(" - ") + rules::Subclass2024(c.character_class) + " at 3";
    lines.push_back(head);
    if (!rules::HasFeatureList(c.character_class)) {
      lines.push_back("  its 2024 features come with its own brief (Phase 3, one class at a time).");
      continue;
    }
    for (const rules::ClassFeature& f : rules::ClassFeatures2024(c.character_class, level)) {
      lines.push_back("  " + std::to_string(f.level) + " " + f.name + (f.subclass ? " (subclass)" : "") + " - " +
                      f.status);
    }
  }
  if (lines.empty()) lines.push_back("nwn5e: no 2024 class.");
  return lines;
}
