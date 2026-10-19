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
  // D1: faithful = no off-hand penalties, one off-hand attack (Light weapons,
  // double weapon or Dual Wielder), no Strength on its damage unless negative
  // or with the Two-Weapon Fighting style. nwn/off = NWN's rules.
  rules::RuleVariant two_weapon_fighting = rules::RuleVariant::kFaithful;
  // D3: faithful = 2024 slots chosen with !mastery or filled by use, one swap
  // per long rest; nwn = mastery classes master every proficient weapon;
  // Weapon Focus adds its weapon in both. off = no mastery.
  rules::RuleVariant weapon_mastery = rules::RuleVariant::kFaithful;
  // E1: faithful = two-handed melee adds Strength once; nwn/off = NWN's 1.5 x.
  rules::RuleVariant two_handed_strength = rules::RuleVariant::kFaithful;
  // Phase 2 SK1–SK3: skills. faithful = the 2024 skills (proficiency and
  // Expertise instead of ranks, !skills to choose, 2024 names; scripts read
  // the values scaled to the 3e range); nwn / off = NWN's.
  rules::RuleVariant skills = rules::RuleVariant::kFaithful;
  // SK5: Stealth against Perception. faithful = Stealth rolled once per hide,
  // passive Perception (10 + modifier) unless searching, no size / in-combat /
  // Favored Enemy terms; nwn / off = NWN's.
  rules::RuleVariant perception = rules::RuleVariant::kFaithful;
  // SK10: spell attacks (touch attacks). faithful = PB + casting ability
  // against full AC, advantage / disadvantage, a natural 20 crits without
  // confirmation; nwn / off = NWN's.
  rules::RuleVariant spell_attacks = rules::RuleVariant::kFaithful;
  // CN1–CN8: conditions. faithful = 2024 conditions (restrained, helpless,
  // stunned, poisoned, concealment, deafened; PLAN.md); nwn = NWN's, plus the
  // both-mode parts (stunned / paralysed / asleep targets give advantage,
  // stunned fails Dexterity saves); off = NWN's. The save parts need saves on.
  rules::RuleVariant conditions = rules::RuleVariant::kFaithful;
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
  // Owner request: after drinking a potion mid-fight, a player character goes
  // back to attacking (NWN drops the attack order on item use).
  bool resume_attack_after_potion = true;
  // SK5 (with perception=faithful): hiding needs a Stealth total of 15 (the
  // 2024 Hide action); below it the creature is not hidden.
  bool hiding_needs_15 = true;
  // SK5 B (with perception=faithful): an enemy finds a hidden creature that is
  // in front of it within 2 m with a clear line between them (2024 hides you
  // only out of sight; NWN has no cover data).
  bool found_up_close = true;
  // SK5 C (with perception=faithful): a creature in combat searches (an active
  // d20 Perception each 6 s update) instead of using passive Perception.
  bool alert_creatures_search = true;

  // [debug] — test switches, not rules.
  bool debug_every_hit_crits = false;  // every hit is a critical, crit immunity ignored
  int debug_attacks_per_round = 0;     // >0: player characters make this many main-hand attacks
  bool debug_attack_trace = false;     // log round setup and every PC attack roll
  int debug_creature_ac = 0;           // >0: every non-player creature has this AC (forces misses)
  bool debug_combat_breakdown = false;  // the engine's own per-term attack breakdown in the combat log
  int debug_creature_hp_multiplier = 0;  // >1: a creature's hit points x this when a character first attacks it
  int debug_creature_min_hp = 0;         // >0: ... and at least this many
};

extern Config g_config;
void ConfigLoad(const wchar_t* directory);
