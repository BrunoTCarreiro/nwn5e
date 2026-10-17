// 2024 rules have no critical confirmation roll. The 3e confirmation lives
// inside the engine's attack resolution; this module patches it once that code
// is mapped (see docs/RE-NOTES.md).

#include "config.h"
#include "log.h"

void InstallCritConfirmRules() {
  if (!g_config.crit_no_confirmation) return;
  Log("crit_no_confirmation: attack-roll site not mapped yet; confirmation roll unchanged");
}
