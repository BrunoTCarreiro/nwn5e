#pragma once

// Rule switches from nwn5e.ini [rules]; every rule can be turned off to fall
// back to the game's 3e behaviour.
struct Config {
  bool crit_nat20_only = true;     // threat only on a natural 20
  bool crit_no_confirmation = true;  // a threat is a critical hit, no second roll
  bool crit_double_damage = true;  // x2 multiplier for every weapon
  bool crit_dice_only = true;      // crits roll damage dice twice, flat bonuses once
  bool attack_proficiency = true;  // attack rolls use the proficiency bonus instead of the 3e BAB
  bool creature_ac_by_cr = true;   // non-player creatures use the expected AC for their CR
  bool extra_attack = true;        // PCs attack per 2024 Extra Attack; no 3e -5 per extra attack
  bool retarget_remaining_attacks = true;  // after a kill, the next attack goes to the nearest enemy
  bool log_attacks = true;         // write each rule decision to nwn5e.log

  // [debug] — test switches, not rules.
  bool debug_every_hit_crits = false;  // every hit is a critical, crit immunity ignored
  int debug_attacks_per_round = 0;     // >0: player characters make this many main-hand attacks
  bool debug_attack_trace = false;     // log round setup and every PC attack roll
  int debug_creature_ac = 0;           // >0: every non-player creature has this AC (forces misses)
};

extern Config g_config;
void ConfigLoad(const wchar_t* directory);
