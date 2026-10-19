// Conditions (PLAN.md "Conditions — decided by the owner", 2026-10-19). Each
// item has a faithful (2024) and an NWN-mode version, switched by the
// `conditions` key.
//
// CN8 Deafened: NWN's deafness also adds 20% spell failure (RE-NOTES
// "Condition effects"), which only ever hit Bard / Sorcerer / Wizard spells
// and which Still Spell avoided; 2024 deafness has none. Faithful applies
// that part at 0%, so a deafened creature still can't hear but casts
// normally. NWN mode keeps it.

#include <stdio.h>

#include "config.h"
#include "hooks.h"
#include "log.h"
#include "nwmain_1_69.h"

void InstallConditionRules() {
  if (g_config.conditions != rules::RuleVariant::kFaithful) return;
  int applied = 0;
  for (const nwmain::SmallPatch& patch : nwmain::kFaithfulConditionPatches) {
    char name[96];
    sprintf_s(name, "conditions: %s (faithful)", patch.what);
    applied += PatchBytes(name, patch.address, patch.original, patch.patched, patch.size) ? 1 : 0;
  }
  Log("conditions: %d of %u faithful patches applied", applied,
      static_cast<unsigned>(sizeof(nwmain::kFaithfulConditionPatches) /
                            sizeof(nwmain::kFaithfulConditionPatches[0])));
}
