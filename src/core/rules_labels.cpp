// Decision C8 (PLAN.md): in faithful mode the three NWN saves are shown with
// their 2024 names — "Fortitude / Reflex / Will" become "Constitution /
// Dexterity / Wisdom" wherever the game shows those labels (character sheet,
// combat log "... Save", effect and spell texts), swapped in memory as the
// game looks them up; dialog.tlk is not changed. NWN mode keeps NWN's labels.

#include <stdint.h>

#include "config.h"
#include "creature.h"
#include "hooks.h"
#include "log.h"
#include "nwmain_1_69.h"
#include "rules/saves.h"
#include "rules/skills.h"

namespace {

using GetSimpleStringFn = int(__fastcall*)(void* table, void* edx, uint32_t strref, void* result, int gender);
using ExoStringAssignFn = void*(__fastcall*)(void* exo_string, void* edx, const char* text);

GetSimpleStringFn g_get_simple_string = nullptr;
const auto g_exo_string_assign = reinterpret_cast<ExoStringAssignFn>(nwmain::kExoStringAssign);
int g_logged = 0;
constexpr uint32_t kReflexSaveStrref = 519;

int __fastcall GetSimpleString(void* table, void* edx, uint32_t strref, void* result, int gender) {
  if (const char* ability_label = rules::SaveLabelForStrref(strref)) {
    // A private id from the client's save-kind patch (rules_save_roll.cpp):
    // filled as "Reflex Save" is, then named for the spell's own ability.
    const int found = g_get_simple_string(table, edx, kReflexSaveStrref, result, gender);
    if (found && result) g_exo_string_assign(result, nullptr, ability_label);
    if (g_config.log_attacks && g_logged < 30) {
      ++g_logged;
      Log("label: save on the spell's ability shown as \"%s\"", ability_label);
    }
    return found;
  }
  const int found = g_get_simple_string(table, edx, strref, result, gender);
  if (!found || !result) return found;
  const char* label = g_config.saves == rules::RuleVariant::kFaithful ? rules::Faithful2024SaveLabel(strref) : nullptr;
  if (!label && g_config.skills == rules::RuleVariant::kFaithful) label = rules::Faithful2024SkillLabelForStrref(strref);
  if (!label) return found;
  g_exo_string_assign(result, nullptr, label);  // the result's text (CExoString at +0)
  if (g_config.log_attacks && g_logged < 30) {
    ++g_logged;
    Log("label: string %u shown as \"%s\"", strref, label);
  }
  return found;
}

}  // namespace

void InstallLabelRules() {
  if (g_config.saves != rules::RuleVariant::kFaithful && g_config.skills != rules::RuleVariant::kFaithful) return;
  InstallHook({"CTlkTable::GetSimpleString (2024 save and skill labels)", nwmain::kTlkGetSimpleString,
               nwmain::kTlkGetSimpleStringPrologue, sizeof(nwmain::kTlkGetSimpleStringPrologue),
               reinterpret_cast<void*>(&GetSimpleString), reinterpret_cast<void**>(&g_get_simple_string)});
}
