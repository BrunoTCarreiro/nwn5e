#pragma once

// Rule switches from nwn5e.ini [rules]; every rule can be turned off to fall
// back to the game's 3e behaviour.
struct Config {
  bool crit_nat20_only = true;     // threat only on a natural 20
  bool crit_no_confirmation = true;  // a threat is a critical hit, no second roll
  bool crit_double_damage = true;  // x2 multiplier for every weapon
  bool attack_proficiency = true;  // attack rolls use the proficiency bonus instead of the 3e BAB
  bool creature_ac_by_cr = true;   // non-player creatures use the expected AC for their CR
  bool extra_attack = true;        // PCs attack per 2024 Extra Attack; no 3e -5 per extra attack
  bool log_attacks = true;         // write each rule decision to nwn5e.log

  // [debug] — test switches, not rules.
  bool debug_every_hit_crits = false;  // every hit is a critical, crit immunity ignored
  int debug_attacks_per_round = 0;     // >0: player characters make this many main-hand attacks
  bool debug_attack_trace = false;     // log round setup and every PC attack roll
};

extern Config g_config;
void ConfigLoad(const wchar_t* directory);
