// Decision SK7 (PLAN.md), faithful: no Concentration checks on casting. 2024
// casting is not interrupted by damage (only keeping a concentration spell
// takes a Constitution save — Phase 4), restrained casters cast normally, and
// there is no defensive casting (casting draws no opportunity attacks, D2).
// NWN rolls d20 + Concentration three times (RE-NOTES "Concentration"):
// on damage while the caster is in a casting animation (OnApplyDamage), for
// defensive casting with an enemy within 4 m (DC 15 + spell level), and when
// casting while entangled (DC 15); a failure loses the spell. Faithful skips
// the first and makes the other two always pass. NWN mode: NWN's.

#include <stdio.h>

#include "config.h"
#include "hooks.h"
#include "log.h"
#include "nwmain_1_69.h"

void InstallCastingRules() {
  if (g_config.concentration != rules::RuleVariant::kFaithful) return;
  int applied = 0;
  for (const nwmain::MediumPatch& patch : nwmain::kFaithfulConcentrationPatches) {
    char name[112];
    sprintf_s(name, "concentration: %s (faithful)", patch.what);
    applied += PatchBytes(name, patch.address, patch.original, patch.patched, patch.size) ? 1 : 0;
  }
  Log("concentration: %d of %u faithful patches applied", applied,
      static_cast<unsigned>(sizeof(nwmain::kFaithfulConcentrationPatches) /
                            sizeof(nwmain::kFaithfulConcentrationPatches[0])));
}
