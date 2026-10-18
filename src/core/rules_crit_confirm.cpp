// 2024 rules have no critical confirmation roll: a natural 20 is a critical
// hit. The engine's confirmation is one branch inside ResolveAttackRoll
// (see nwmain_1_69.h); removing it confirms every threat.
//
// The threat roll is still made and stored, but the attack-roll message no
// longer sends it, so the combat log shows no "Threat Roll" (nwmain_1_69.h).
//
// crits_against_players (decision B7) removes NWN's difficulty rule that turns
// a creature's critical hit on a player-controlled character into a hit.
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
  if (g_config.crits_against_players) {
    PatchBytes("ResolveAttackRoll difficulty rule: creatures' crits on players stay crits",
               nwmain::kNoCritsOnPlayersBranch, nwmain::kNoCritsOnPlayersBranchBytes,
               nwmain::kNoCritsOnPlayersBranchSkip, sizeof(nwmain::kNoCritsOnPlayersBranchSkip));
  }
  if (g_config.crit_no_confirmation) {
    PatchBytes("combat log attack message: no threat flag (1/2)", nwmain::kCcThreatFlag1, nwmain::kCcThreatFlagBytes,
               nwmain::kCcThreatFlagZero, sizeof(nwmain::kCcThreatFlagZero));
    PatchBytes("combat log attack message: no threat roll (1/2)", nwmain::kCcThreatRollSkip1,
               nwmain::kCcThreatRollSkipBytes, nwmain::kCcThreatRollSkipAlways,
               sizeof(nwmain::kCcThreatRollSkipAlways));
    PatchBytes("combat log attack message: no threat flag (2/2)", nwmain::kCcThreatFlag2, nwmain::kCcThreatFlagBytes,
               nwmain::kCcThreatFlagZero, sizeof(nwmain::kCcThreatFlagZero));
    PatchBytes("combat log attack message: no threat roll (2/2)", nwmain::kCcThreatRollSkip2,
               nwmain::kCcThreatRollSkipBytes, nwmain::kCcThreatRollSkipAlways,
               sizeof(nwmain::kCcThreatRollSkipAlways));
  }
  if (g_config.debug_every_hit_crits) {
    PatchBytes("ResolveAttackRoll critical immunity (debug)", nwmain::kCritImmunityCheck,
               nwmain::kCritImmunityCheckBytes, nwmain::kCritImmunityCheckSkip, sizeof(nwmain::kCritImmunityCheckSkip));
  }
}
