// Decision D2 (PLAN.md): opportunity attacks. In 2024 only leaving a
// creature's reach provokes, once per round (one reaction). NWN already gives
// each creature one AoO per round (nwmain_1_69.h) and has no Combat Reflexes,
// but it also provokes on casting, ranged shots, unarmed attacks without
// Improved Unarmed Strike and Disarm, and Spring Attack shields movement.
// Faithful turns those off; moving away still provokes and Tumble still
// avoids it (until Phase 6 adds Disengage). NWN mode leaves NWN's rules.

#include <stdint.h>
#include <stdio.h>

#include "config.h"
#include "hooks.h"
#include "log.h"
#include "nwmain_1_69.h"

void InstallOpportunityRules() {
  if (g_config.opportunity_attacks != rules::RuleVariant::kFaithful) return;
  int applied = 0;
  for (const nwmain::BranchPatch& patch : nwmain::kFaithfulOpportunityPatches) {
    char name[96];
    sprintf_s(name, "opportunity attacks: %s (faithful)", patch.what);
    applied += PatchBytes(name, patch.address, patch.original, patch.patched, sizeof(patch.patched)) ? 1 : 0;
  }
  Log("opportunity attacks: %d of %u faithful patches applied", applied,
      static_cast<unsigned>(sizeof(nwmain::kFaithfulOpportunityPatches) /
                            sizeof(nwmain::kFaithfulOpportunityPatches[0])));
}
