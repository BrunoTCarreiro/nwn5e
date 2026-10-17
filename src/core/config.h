#pragma once

// Rule switches from nwn5e.ini [rules]; every rule can be turned off to fall
// back to the game's 3e behaviour.
struct Config {
  bool crit_nat20_only = true;     // threat only on a natural 20
  bool crit_no_confirmation = true;  // a threat is a critical hit, no second roll
  bool crit_double_damage = true;  // x2 multiplier for every weapon
  bool log_attacks = true;         // write each crit decision to nwn5e.log
};

extern Config g_config;
void ConfigLoad(const wchar_t* directory);
