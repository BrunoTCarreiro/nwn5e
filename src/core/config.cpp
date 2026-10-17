#include "config.h"

#include <windows.h>
#include <stdio.h>

#include "log.h"

Config g_config;

namespace {
bool ReadBool(const wchar_t* ini, const wchar_t* key, bool fallback) {
  return GetPrivateProfileIntW(L"rules", key, fallback ? 1 : 0, ini) != 0;
}
}  // namespace

void ConfigLoad(const wchar_t* directory) {
  wchar_t ini[MAX_PATH];
  swprintf_s(ini, L"%s\\nwn5e.ini", directory);
  g_config.crit_nat20_only = ReadBool(ini, L"crit_nat20_only", g_config.crit_nat20_only);
  g_config.crit_no_confirmation = ReadBool(ini, L"crit_no_confirmation", g_config.crit_no_confirmation);
  g_config.crit_double_damage = ReadBool(ini, L"crit_double_damage", g_config.crit_double_damage);
  g_config.log_attacks = ReadBool(ini, L"log_attacks", g_config.log_attacks);
  Log("config: crit_nat20_only=%d crit_no_confirmation=%d crit_double_damage=%d log_attacks=%d",
      g_config.crit_nat20_only, g_config.crit_no_confirmation, g_config.crit_double_damage, g_config.log_attacks);
}
