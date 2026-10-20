#include "config.h"

#include <windows.h>
#include <stdio.h>

#include "log.h"

Config g_config;

namespace {
bool ReadBool(const wchar_t* ini, const wchar_t* key, bool fallback, const wchar_t* section = L"rules") {
  return GetPrivateProfileIntW(section, key, fallback ? 1 : 0, ini) != 0;
}

// A faithful / nwn / off key (PLAN.md "Modes"); empty or missing -> fallback.
rules::RuleVariant ReadVariant(const wchar_t* ini, const wchar_t* key, rules::RuleVariant fallback) {
  wchar_t wide[32] = {};
  GetPrivateProfileStringW(L"rules", key, L"", wide, 32, ini);
  char text[32] = {};
  for (int i = 0; i < 31 && wide[i]; ++i) text[i] = wide[i] < 128 ? static_cast<char>(wide[i]) : '?';
  return rules::ParseRuleVariant(text, fallback);
}

const char* VariantName(rules::RuleVariant variant) {
  switch (variant) {
    case rules::RuleVariant::kFaithful:
      return "faithful";
    case rules::RuleVariant::kNwn:
      return "nwn";
    default:
      return "off";
  }
}
}  // namespace

void ConfigLoad(const wchar_t* directory) {
  wchar_t ini[MAX_PATH];
  swprintf_s(ini, L"%s\\nwn5e.ini", directory);
  g_config.mode = ReadVariant(ini, L"mode", g_config.mode);
  g_config.racial_combat_bonuses = ReadVariant(ini, L"racial_combat_bonuses", g_config.mode);
  g_config.saves = ReadVariant(ini, L"saves", g_config.mode);
  g_config.opportunity_attacks = ReadVariant(ini, L"opportunity_attacks", g_config.mode);
  g_config.two_weapon_fighting = ReadVariant(ini, L"two_weapon_fighting", g_config.mode);
  g_config.weapon_mastery = ReadVariant(ini, L"weapon_mastery", g_config.mode);
  g_config.two_handed_strength = ReadVariant(ini, L"two_handed_strength", g_config.mode);
  g_config.conditions = ReadVariant(ini, L"conditions", g_config.mode);
  g_config.skills = ReadVariant(ini, L"skills", g_config.mode);
  g_config.perception = ReadVariant(ini, L"perception", g_config.mode);
  g_config.spell_attacks = ReadVariant(ini, L"spell_attacks", g_config.mode);
  g_config.concentration = ReadVariant(ini, L"concentration", g_config.mode);
  g_config.knockdown = ReadVariant(ini, L"knockdown", g_config.mode);
  g_config.initiative = ReadVariant(ini, L"initiative", g_config.mode);
  g_config.nwn_only_skills = ReadVariant(ini, L"nwn_only_skills", g_config.mode);
  g_config.progression = ReadVariant(ini, L"progression", g_config.mode);
  g_config.multiclass = ReadVariant(ini, L"multiclass", g_config.mode);
  g_config.alignment = ReadVariant(ini, L"alignment", g_config.mode);
  g_config.hit_points = ReadVariant(ini, L"hit_points", g_config.mode);
  g_config.feat_cadence = ReadVariant(ini, L"feat_cadence", g_config.mode);
  Log("config: feat_cadence=%s", VariantName(g_config.feat_cadence));
  g_config.ability_scores = ReadVariant(ini, L"ability_scores", g_config.mode);
  Log("config: ability_scores=%s", VariantName(g_config.ability_scores));
  g_config.served_data = ReadBool(ini, L"served_data", g_config.served_data);
  g_config.debug_served_data_probe = ReadBool(ini, L"served_data_probe", g_config.debug_served_data_probe, L"debug");
  Log("config: served_data=%d (probe %d)", g_config.served_data ? 1 : 0, g_config.debug_served_data_probe ? 1 : 0);
  Log("config: hit_points=%s", VariantName(g_config.hit_points));
  Log("config: alignment=%s", VariantName(g_config.alignment));
  Log("config: progression=%s multiclass=%s", VariantName(g_config.progression), VariantName(g_config.multiclass));
  Log("config: nwn_only_skills=%s", VariantName(g_config.nwn_only_skills));
  Log("config: knockdown=%s initiative=%s", VariantName(g_config.knockdown), VariantName(g_config.initiative));
  Log("config: spell_attacks=%s concentration=%s", VariantName(g_config.spell_attacks),
      VariantName(g_config.concentration));
  Log("config: skills=%s perception=%s", VariantName(g_config.skills), VariantName(g_config.perception));
  Log("config: mode=%s racial_combat_bonuses=%s saves=%s opportunity_attacks=%s two_weapon_fighting=%s "
      "weapon_mastery=%s two_handed_strength=%s conditions=%s",
      VariantName(g_config.mode), VariantName(g_config.racial_combat_bonuses), VariantName(g_config.saves),
      VariantName(g_config.opportunity_attacks), VariantName(g_config.two_weapon_fighting),
      VariantName(g_config.weapon_mastery), VariantName(g_config.two_handed_strength),
      VariantName(g_config.conditions));
  g_config.crit_nat20_only = ReadBool(ini, L"crit_nat20_only", g_config.crit_nat20_only);
  g_config.crit_no_confirmation = ReadBool(ini, L"crit_no_confirmation", g_config.crit_no_confirmation);
  g_config.crit_double_damage = ReadBool(ini, L"crit_double_damage", g_config.crit_double_damage);
  g_config.crit_dice_only = ReadBool(ini, L"crit_dice_only", g_config.crit_dice_only);
  Log("config: crit_dice_only=%d", g_config.crit_dice_only);
  g_config.attack_proficiency = ReadBool(ini, L"attack_proficiency", g_config.attack_proficiency);
  g_config.creature_ac_by_cr = ReadBool(ini, L"creature_ac_by_cr", g_config.creature_ac_by_cr);
  g_config.extra_attack = ReadBool(ini, L"extra_attack", g_config.extra_attack);
  g_config.retarget_remaining_attacks =
      ReadBool(ini, L"retarget_remaining_attacks", g_config.retarget_remaining_attacks);
  g_config.resume_attack_after_potion =
      ReadBool(ini, L"resume_attack_after_potion", g_config.resume_attack_after_potion);
  g_config.hiding_needs_15 = ReadBool(ini, L"hiding_needs_15", g_config.hiding_needs_15);
  g_config.found_up_close = ReadBool(ini, L"found_up_close", g_config.found_up_close);
  g_config.alert_creatures_search = ReadBool(ini, L"alert_creatures_search", g_config.alert_creatures_search);
  Log("config: resume_attack_after_potion=%d", g_config.resume_attack_after_potion);
  g_config.ranged_retarget_range = GetPrivateProfileIntW(L"rules", L"ranged_retarget_range", 40, ini);
  Log("config: retarget_remaining_attacks=%d ranged_retarget_range=%d m", g_config.retarget_remaining_attacks,
      g_config.ranged_retarget_range);
  g_config.crits_against_players = ReadBool(ini, L"crits_against_players", g_config.crits_against_players);
  Log("config: crits_against_players=%d", g_config.crits_against_players);
  g_config.advantage_disadvantage = ReadBool(ini, L"advantage_disadvantage", g_config.advantage_disadvantage);
  Log("config: advantage_disadvantage=%d", g_config.advantage_disadvantage);
  g_config.log_attacks = ReadBool(ini, L"log_attacks", g_config.log_attacks);
  g_config.debug_every_hit_crits = ReadBool(ini, L"every_hit_crits", g_config.debug_every_hit_crits, L"debug");
  g_config.debug_attacks_per_round = GetPrivateProfileIntW(L"debug", L"attacks_per_round", 0, ini);
  g_config.debug_attack_trace = ReadBool(ini, L"attack_trace", g_config.debug_attack_trace, L"debug");
  if (g_config.debug_attack_trace) Log("config: [debug] attack_trace=1 — logging round setup and PC attack rolls");
  g_config.debug_combat_breakdown =
      ReadBool(ini, L"combat_breakdown", g_config.debug_combat_breakdown, L"debug");
  g_config.debug_creature_ac = GetPrivateProfileIntW(L"debug", L"creature_ac", 0, ini);
  g_config.debug_creature_hp_multiplier = GetPrivateProfileIntW(L"debug", L"creature_hp_multiplier", 0, ini);
  g_config.debug_creature_min_hp = GetPrivateProfileIntW(L"debug", L"creature_min_hp", 0, ini);
  g_config.debug_players_poisoned = GetPrivateProfileIntW(L"debug", L"players_poisoned", 0, ini) != 0;
  if (g_config.debug_players_poisoned) Log("config: [debug] players_poisoned=1 - player characters count as poisoned for ability checks");
  if (g_config.debug_creature_min_hp > 0) {
    Log("config: [debug] creature_min_hp=%d - creatures have at least this many hit points when your party first "
        "attacks them", g_config.debug_creature_min_hp);
  }
  if (g_config.debug_creature_hp_multiplier > 1) {
    Log("config: [debug] creature_hp_multiplier=%d - creatures get this many times their hit points when your "
        "party first attacks them", g_config.debug_creature_hp_multiplier);
  }
  if (g_config.debug_creature_ac > 0) {
    Log("config: [debug] creature_ac=%d — every non-player creature has this AC", g_config.debug_creature_ac);
  }
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
