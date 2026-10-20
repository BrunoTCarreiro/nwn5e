// Decision CM7 (PLAN.md), faithful: feats come at 2024's levels (RE-NOTES
// "Feat and ability cadence"):
// - NWN's general feat only at level 1 (and the human bonus feat): the
//   server's count (ValidateLevelUp, LevelUpAutomatic) and the client's;
// - no ability +1 every 4 levels: neither side puts it in the level-up
//   message, and the level-up screen skips its ability page;
// - the class bonus-feat slots (CNWClass +0x1ED + class level, read by both
//   sides and by henchmen's automatic level-ups) become 2024's feat levels
//   for the 11 base classes — 4, 8, 12, 16, 19, the Fighter also 6 and 14,
//   the Rogue 10 — rewritten after every rules load;
// - those slots offer Ability Score Improvement (data/rows/feat.txt and the
//   classes' bonus lists, served by CM0); its abilities are chosen with !asi.
// NWN's own bonus feats at its levels (Fighter, Wizard, Rogue, Ranger's
// favored enemies) are gone in faithful; class features are CM9.
// NWN mode: NWN's cadence, and the ASI rows are hidden (MinLevel 0xFF).

#include <stdint.h>
#include <stdio.h>

#include <string>

#include "config.h"
#include "creature.h"
#include "hooks.h"
#include "log.h"
#include "nwmain_1_69.h"
#include "rules/class_features.h"
#include "rules/nwn_conversion.h"
#include "rules/progression.h"
#include "rules_loaded.h"

namespace {

constexpr int kMaxBonusFeatLevel = 60;  // GetBonusFeats 0x53B6E0 reads levels 1..60
constexpr int kLastBaseClass = 10;      // classes.2da rows 0..10: Barbarian .. Wizard

void SetFaithfulBonusFeatSlots(void* rules_object, bool reload) {
  auto* classes = Field<uint8_t*>(rules_object, nwmain::kRulesClasses);
  const int count = Field<uint8_t>(rules_object, nwmain::kRulesClassCount);
  if (!classes) return;
  int changed = 0;
  for (int id = 0; id < count && id <= kLastBaseClass; ++id) {
    const auto character_class = rules::ClassFromNwnId(id);
    if (!character_class) continue;
    uint8_t* slots = classes + id * nwmain::kClassStride + nwmain::kClassBonusFeats;
    for (int level = 1; level <= kMaxBonusFeatLevel; ++level) {
      const bool style = g_config.class_features == rules::RuleVariant::kFaithful &&
                         rules::IsFightingStyleLevel2024(*character_class, level);  // Fighter F1
      slots[level] = rules::IsFeatLevel2024(*character_class, level) || style ? 1 : 0;
    }
    ++changed;
  }
  Log("feat cadence: 2024 feat slots%s for %d base classes (4, 8, 12, 16, 19; Fighter 6, 14; Rogue 10)",
      reload ? " again after a rules reload" : "", changed);
}

void HideAsiRows(void* rules_object, bool reload) {
  auto* feats = Field<uint8_t*>(rules_object, nwmain::kRulesFeats);
  const int count = Field<uint16_t>(rules_object, nwmain::kRulesFeatCount);
  if (!feats) return;
  int hidden = 0;
  for (int id = rules::kAsiFeatFirst; id < rules::kAsiFeatFirst + rules::kAsiFeatCount && id < count; ++id) {
    SetField<uint8_t>(feats + id * nwmain::kFeatStride, nwmain::kFeatMinLevel, 0xFF);
    ++hidden;
  }
  if (!reload) Log("feat cadence: NWN's - %d Ability Score Improvement rows hidden", hidden);
}

}  // namespace

void InstallFeatCadenceRules() {
  if (g_config.feat_cadence != rules::RuleVariant::kFaithful) {
    OnRulesLoaded(&HideAsiRows);
    return;
  }
  int applied = 0;
  for (const nwmain::MediumPatch& patch : nwmain::kFaithfulFeatCadencePatches) {
    char name[112];
    sprintf_s(name, "feat cadence: %s (faithful)", patch.what);
    applied += PatchBytes(name, patch.address, patch.original, patch.patched, patch.size) ? 1 : 0;
  }
  applied += OnRulesLoaded(&SetFaithfulBonusFeatSlots) ? 1 : 0;
  Log("feat cadence: %d of %u faithful changes installed", applied,
      static_cast<unsigned>(sizeof(nwmain::kFaithfulFeatCadencePatches) /
                            sizeof(nwmain::kFaithfulFeatCadencePatches[0])) + 1);
}
