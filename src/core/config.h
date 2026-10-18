#pragma once

#include "rules/modes.h"

// Rule switches from nwn5e.ini [rules]; every rule can be turned off to fall
// back to the game's 3e behaviour. Rules decided since 2026-10-18 have a
// faithful (2024) and an NWN-mode version: `mode` picks the default, each
// rule's key overrides it (PLAN.md "Modes").
struct Config {
  rules::RuleVariant mode = rules::RuleVariant::kFaithful;
  // R1: faithful removes NWN's racial attack/AC bonuses vs specific enemies.
  rules::RuleVariant racial_combat_bonuses = rules::RuleVariant::kFaithful;
  // C1–C3: the three NWN saves as 2024 Con/Dex/Wis saves (off = NWN's 3e saves).
  rules::RuleVariant saves = rules::RuleVariant::kFaithful;
  // D2: faithful = only moving away provokes (no casting, potion, ranged,
  // unarmed or Disarm AoOs; Spring Attack no longer protects). nwn/off = NWN's AoOs.
  rules::RuleVariant opportunity_attacks = rules::RuleVariant::kFaithful;
  bool crit_nat20_only = true;     // threat only on a natural 20
  bool crit_no_confirmation = true;  // a threat is a critical hit, no second roll
  bool crit_double_damage = true;  // x2 multiplier for every weapon
  bool crit_dice_only = true;      // crits roll damage dice twice, flat bonuses once
  bool crits_against_players = true;  // no difficulty rule turning creatures' crits on players into hits
  bool attack_proficiency = true;  // attack rolls use the proficiency bonus instead of the 3e BAB
  bool creature_ac_by_cr = true;   // non-player creatures use the expected AC for their CR
  bool extra_attack = true;        // PCs attack per 2024 Extra Attack; no 3e -5 per extra attack
  bool retarget_remaining_attacks = true;  // after a kill, the next attack goes to the nearest enemy
  int ranged_retarget_range = 40;  // metres a ranged retarget looks (capped at the weapon's range)
  bool advantage_disadvantage = true;  // situational modifiers become 2024 advantage/disadvantage
  bool log_attacks = true;         // write each rule decision to nwn5e.log

  // [debug] — test switches, not rules.
  bool debug_every_hit_crits = false;  // every hit is a critical, crit immunity ignored
  int debug_attacks_per_round = 0;     // >0: player characters make this many main-hand attacks
  bool debug_attack_trace = false;     // log round setup and every PC attack roll
  int debug_creature_ac = 0;           // >0: every non-player creature has this AC (forces misses)
  bool debug_combat_breakdown = false;  // the engine's own per-term attack breakdown in the combat log
};

extern Config g_config;
void ConfigLoad(const wchar_t* directory);
