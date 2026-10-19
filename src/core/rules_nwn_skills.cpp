// Decision SK8 (PLAN.md), faithful: the 3e-only skills do nothing (RE-NOTES
// "Parry, Taunt, Appraise, Use Magic Device"). 2024 has none of them:
// - parry mode is a cosmetic toggle: no parry rolls or ripostes, and the
//   creature attacks and makes opportunity attacks as usual (NWN's parry mode
//   gives up its own attacks for parry attempts);
// - Taunt does nothing (the animation plays; no roll, effect or hostility);
// - Appraise: stores keep their own prices — the markup / markdown scripts
//   pass to OpenStore (the campaigns' Appraise check, but also any other
//   script discount) is recorded as 0;
// - Use Magic Device never lifts an item's class, race or alignment limits
//   (so non-casters can't read class-restricted scrolls), and Holy Avenger
//   gives a non-paladin its plain +2. The SRD Thief's Use Magic Device
//   returns with classes (Phase 3).
// NWN mode: NWN's.

#include <stdio.h>

#include "config.h"
#include "hooks.h"
#include "log.h"
#include "nwmain_1_69.h"

void InstallNwnOnlySkillRules() {
  if (g_config.nwn_only_skills != rules::RuleVariant::kFaithful) return;
  int applied = 0;
  for (const nwmain::MediumPatch& patch : nwmain::kFaithfulNwnOnlySkillPatches) {
    char name[112];
    sprintf_s(name, "3e-only skills: %s (faithful)", patch.what);
    applied += PatchBytes(name, patch.address, patch.original, patch.patched, patch.size) ? 1 : 0;
  }
  Log("3e-only skills: %d of %u faithful patches applied", applied,
      static_cast<unsigned>(sizeof(nwmain::kFaithfulNwnOnlySkillPatches) /
                            sizeof(nwmain::kFaithfulNwnOnlySkillPatches[0])));
}
