#include "config.h"

#include <windows.h>
#include <stdio.h>

#include "log.h"

Config g_config;

namespace {
bool ReadBool(const wchar_t* ini, const wchar_t* key, bool fallback, const wchar_t* section = L"rules") {
  return GetPrivateProfileIntW(section, key, fallback ? 1 : 0, ini) != 0;
}
}  // namespace

void ConfigLoad(const wchar_t* directory) {
  wchar_t ini[MAX_PATH];
  swprintf_s(ini, L"%s\\nwn5e.ini", directory);
  g_config.crit_nat20_only = ReadBool(ini, L"crit_nat20_only", g_config.crit_nat20_only);
  g_config.crit_no_confirmation = ReadBool(ini, L"crit_no_confirmation", g_config.crit_no_confirmation);
  g_config.crit_double_damage = ReadBool(ini, L"crit_double_damage", g_config.crit_double_damage);
  g_config.attack_proficiency = ReadBool(ini, L"attack_proficiency", g_config.attack_proficiency);
  g_config.creature_ac_by_cr = ReadBool(ini, L"creature_ac_by_cr", g_config.creature_ac_by_cr);
  g_config.extra_attack = ReadBool(ini, L"extra_attack", g_config.extra_attack);
  g_config.log_attacks = ReadBool(ini, L"log_attacks", g_config.log_attacks);
  g_config.debug_every_hit_crits = ReadBool(ini, L"every_hit_crits", g_config.debug_every_hit_crits, L"debug");
  g_config.debug_attacks_per_round = GetPrivateProfileIntW(L"debug", L"attacks_per_round", 0, ini);
  g_config.debug_attack_trace = ReadBool(ini, L"attack_trace", g_config.debug_attack_trace, L"debug");
  if (g_config.debug_attack_trace) Log("config: [debug] attack_trace=1 — logging round setup and PC attack rolls");
  Log("config: crit_nat20_only=%d crit_no_confirmation=%d crit_double_damage=%d attack_proficiency=%d "
      "creature_ac_by_cr=%d extra_attack=%d log_attacks=%d",
      g_config.crit_nat20_only, g_config.crit_no_confirmation, g_config.crit_double_damage,
      g_config.attack_proficiency, g_config.creature_ac_by_cr, g_config.extra_attack, g_config.log_attacks);
  if (g_config.debug_every_hit_crits) Log("config: [debug] every_hit_crits=1 — every hit is a critical");
  if (g_config.debug_attacks_per_round > 0) {
    Log("config: [debug] attacks_per_round=%d — player characters make this many main-hand attacks",
        g_config.debug_attacks_per_round);
  }
}
