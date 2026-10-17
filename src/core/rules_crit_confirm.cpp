// 2024 rules have no critical confirmation roll: a natural 20 is a critical
// hit. The engine's confirmation is one branch inside ResolveAttackRoll
// (see nwmain_1_69.h); removing it confirms every threat.
//
// The threat roll is still made and stored, so the combat log keeps showing a
// "threat roll" until the combat feedback strings are reworked (Phase 7).
//
// [debug] every_hit_crits also needs the confirmation gone, and skips the
// critical-immunity checks so a test against undead/constructs still shows it.

#include "config.h"
#include "hooks.h"
#include "log.h"
#include "nwmain_1_69.h"

void InstallCritConfirmRules() {
  if (g_config.crit_no_confirmation || g_config.debug_every_hit_crits) {
    static const uint8_t kNops[sizeof(nwmain::kCritConfirmBranchBytes)] = {0x90, 0x90, 0x90, 0x90, 0x90, 0x90};
    PatchBytes("ResolveAttackRoll critical confirmation", nwmain::kCritConfirmBranch, nwmain::kCritConfirmBranchBytes,
               kNops, sizeof(kNops));
  }
  if (g_config.debug_every_hit_crits) {
    PatchBytes("ResolveAttackRoll critical immunity (debug)", nwmain::kCritImmunityCheck,
               nwmain::kCritImmunityCheckBytes, nwmain::kCritImmunityCheckSkip, sizeof(nwmain::kCritImmunityCheckSkip));
  }
}
