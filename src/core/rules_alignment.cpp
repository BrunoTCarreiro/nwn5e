// Decision CM14 (PLAN.md), faithful: alignment is descriptive — no SRD 5.2
// rule reads it (RE-NOTES "Alignment limits"):
// - classes have no alignment limits: AlignRestrict, AlignRstrctType and
//   InvertRestrict are cleared in the loaded class table (after every load),
//   so the level-up class list, the server's checks and the alignment-shift
//   code (which marks a paladin or monk out of alignment) see none;
// - the creation screen's own hard-coded limits (Barbarian, Bard, Monk,
//   Paladin, Druid) are skipped: every alignment can be picked;
// - items' Use Limitation: Alignment Group / Specific Alignment is ignored.
// Spells and weapon properties keyed to alignment change with their phases
// (4, the Paladin brief, 8); the campaigns keep shifting and checking
// alignment as story. NWN mode: NWN's.

#include <stdint.h>
#include <stdio.h>

#include <string>

#include "config.h"
#include "creature.h"
#include "hooks.h"
#include "log.h"
#include "nwmain_1_69.h"
#include "rules_loaded.h"

namespace {

void LiftClassAlignmentLimits(void* rules_object, bool reload) {
  const int count = Field<uint8_t>(rules_object, nwmain::kRulesClassCount);
  auto* classes = Field<uint8_t*>(rules_object, nwmain::kRulesClasses);
  if (!classes) return;
  std::string lifted;
  int changed = 0;
  for (int i = 0; i < count; ++i) {
    uint8_t* c = classes + i * nwmain::kClassStride;
    const uint8_t mask = Field<uint8_t>(c, nwmain::kClassAlignRestrict);
    const uint8_t type = Field<uint8_t>(c, nwmain::kClassAlignRestrictType);
    const uint32_t invert = Field<uint32_t>(c, nwmain::kClassInvertRestrict);
    if (!mask && !type && !invert) continue;
    ++changed;
    char row[48];
    sprintf_s(row, "%s%d (0x%02X/%u/%u)", lifted.empty() ? "" : ", ", i, mask, type, invert);
    lifted += row;
    SetField<uint8_t>(c, nwmain::kClassAlignRestrict, 0);
    SetField<uint8_t>(c, nwmain::kClassAlignRestrictType, 0);
    SetField<uint32_t>(c, nwmain::kClassInvertRestrict, 0);
  }
  if (reload) {
    Log("alignment: class limits lifted again after a rules reload (%d of %d classes)", changed, count);
  } else {
    Log("alignment: class limits lifted for %d of %d classes - rows (mask/axes/invert): %s", changed, count,
        lifted.c_str());
  }
}

}  // namespace

void InstallAlignmentRules() {
  if (g_config.alignment != rules::RuleVariant::kFaithful) return;
  int applied = OnRulesLoaded(&LiftClassAlignmentLimits) ? 1 : 0;
  applied += PatchBytes("alignment: the creation screen offers every alignment (faithful)",
                        nwmain::kCreationAlignmentLimits, nwmain::kCreationAlignmentLimitsBytes,
                        nwmain::kCreationAlignmentLimitsSkip, sizeof(nwmain::kCreationAlignmentLimitsSkip))
                 ? 1
                 : 0;
  applied += PatchBytes("alignment: items' alignment limits ignored (faithful)", nwmain::kItemAlignmentLimit,
                        nwmain::kItemAlignmentLimitBytes, nwmain::kItemAlignmentLimitSkip,
                        sizeof(nwmain::kItemAlignmentLimitSkip))
                 ? 1
                 : 0;
  Log("alignment: %d of 3 faithful changes installed", applied);
}
