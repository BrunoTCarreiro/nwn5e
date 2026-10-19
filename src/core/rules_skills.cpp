// Phase 2 step 1 (PLAN.md "Phase 2 — skills and checks", decided by the owner
// 2026-10-19), with `skills` faithful:
// - SK2: CNWSCreatureStats::GetSkillRank (0x6A2C00) — the one function every
//   engine and script skill total comes from — returns the 2024 value: the
//   2024 skill's ability modifier + proficiency bonus if proficient (+ again
//   with Expertise) + item and spell effects. Ranks, Skill Focus, racial
//   bonuses, armour check penalty and NWN's blindness −4 are dropped (Stealth
//   disadvantage in armour and failing sight checks when blind come with the
//   check rolls). Rows without a 2024 skill (Concentration, Discipline, Parry,
//   Taunt, Appraise, Use Magic Device, Craft Trap) keep NWN's value until
//   SK6–SK8; "base ranks only" calls (level-up, the sheet message) are NWN's.
// - Characters (player characters, henchmen) keep their proficiencies and
//   Expertise in local variables (saved with the game), set with `!skills`
//   and checked against their 2024 choices; until set, they are taken from
//   the skills they have the most ranks in, within those choices. Other
//   creatures are proficient where they have ranks.
// - SK3 (a): a skill rank a script asks for (GetSkillRank, command 315 — the
//   official campaigns' AutoDC conversation checks) is the 2024 value scaled
//   to the 3e range (2 x), since those DCs are set for 3e totals.
// - SK1: the 2024 skill names are shown through the label hook (rules_labels).

#include <stdint.h>
#include <string.h>

#include <algorithm>
#include <string>
#include <unordered_map>
#include <vector>

#include "config.h"
#include "creature.h"
#include "engine_queries.h"
#include "hooks.h"
#include "log.h"
#include "log_once.h"
#include "nwmain_1_69.h"
#include "rules/nwn_conversion.h"
#include "rules/skills.h"

void TellPlayer(uint32_t object_id, const std::string& line);  // rules_chat.cpp
bool IsPlayerControlled(uint32_t object_id);                   // rules_chat.cpp

namespace {

using GetSkillRankFn = char(__fastcall*)(void* stats, void* edx, uint8_t skill, void* vs, int base_only);
using TotalEffectBonusFn = int(__fastcall*)(void* creature, void* edx, int type, void* vs, int a3, int a4, int a5,
                                            int a6, int skill, int a8, int a9);
using DexModFn = int(__fastcall*)(void* stats, void* edx, int armor_dex_cap);

GetSkillRankFn g_get_skill_rank = nullptr;
const auto g_get_skill_rank_entry = reinterpret_cast<GetSkillRankFn>(nwmain::kGetSkillRank);  // through the hook
const auto g_total_effect_bonus = reinterpret_cast<TotalEffectBonusFn>(nwmain::kGetTotalEffectBonus);
const auto g_get_dex_mod = reinterpret_cast<DexModFn>(nwmain::kGetDexMod);

constexpr const char* kVarProficient = "nwn5e_skills";
constexpr const char* kVarExpertise = "nwn5e_skills_expertise";
constexpr const char* kVarSet = "nwn5e_skills_set";  // 1 chosen with !skills, 2 taken from ranks

struct Profile {
  rules::SkillSet proficient = 0;
  rules::SkillSet expertise = 0;
};
std::unordered_map<uint32_t, Profile> g_profiles;  // characters, by object id (the local variables, cached)
LogOnce g_logged(400);

bool Faithful() { return g_config.skills == rules::RuleVariant::kFaithful; }

bool SummonLike(int associate_type) { return associate_type >= 2 && associate_type <= 5; }

bool CountsAsCharacter(void* stats, const CreatureFacts& facts) {
  if (facts.character) return true;
  const std::vector<rules::NwnClassSlot> slots = ReadClassSlots(stats);
  return !slots.empty() && rules::ClassFromNwnId(slots[0].nwn_class_id) && !SummonLike(facts.associate_type);
}

int AbilityModifier(void* stats, rules::Ability ability) {
  switch (ability) {
    case rules::Ability::kStrength:
      return Field<int8_t>(stats, nwmain::kStatsStrModifier);
    case rules::Ability::kDexterity:
      return static_cast<int8_t>(g_get_dex_mod(stats, nullptr, 0) & 0xFF);
    case rules::Ability::kConstitution:
      return Field<int8_t>(stats, nwmain::kStatsConModifier);
    case rules::Ability::kIntelligence:
      return Field<int8_t>(stats, nwmain::kStatsIntModifier);
    case rules::Ability::kWisdom:
      return Field<int8_t>(stats, nwmain::kStatsWisModifier);
    case rules::Ability::kCharisma:
      return Field<int8_t>(stats, nwmain::kStatsChaModifier);
  }
  return 0;
}

std::vector<rules::ClassLevels> ClassLevelsOf(void* stats) {
  std::vector<rules::ClassLevels> classes;
  for (const rules::NwnClassSlot& slot : ReadClassSlots(stats)) {
    if (const auto c = rules::ClassFromNwnId(slot.nwn_class_id)) classes.push_back({*c, slot.levels});
  }
  return classes;
}

rules::Species2024 SpeciesOf(void* stats) {
  switch (Field<uint16_t>(stats, nwmain::kStatsRace)) {
    case nwmain::kRaceHuman:
      return rules::Species2024::kHuman;
    case nwmain::kRaceElf:
      return rules::Species2024::kElf;
    default:
      return rules::Species2024::kOther;  // other NWN species: Phase 3
  }
}

// The most NWN ranks among a 2024 skill's rows.
int RanksFor(void* stats, rules::Skill2024 skill) {
  const int8_t* ranks = Field<const int8_t*>(stats, nwmain::kStatsSkillRanks);
  if (!ranks) return 0;
  int most = 0;
  for (int row = 0; row < rules::kNwnSkillCount; ++row) {
    if (rules::Skill2024ForNwnSkill(row) == skill) most = std::max<int>(most, ranks[row]);
  }
  return most;
}

// From ranks: the most-ranked skills that fit the 2024 choices, then Expertise
// in the most-ranked of those.
Profile ProfileFromRanks(void* stats, const rules::SkillAllowance& allowance) {
  std::vector<std::pair<int, int>> by_rank;  // (ranks, skill)
  for (int i = 0; i < rules::kSkill2024Count; ++i) {
    const int ranks = RanksFor(stats, static_cast<rules::Skill2024>(i));
    if (ranks > 0) by_rank.push_back({ranks, i});
  }
  std::stable_sort(by_rank.begin(), by_rank.end(), [](const auto& a, const auto& b) { return a.first > b.first; });
  Profile profile;
  profile.proficient = allowance.granted;
  for (const auto& entry : by_rank) {
    const rules::SkillSet tried = profile.proficient | (1u << entry.second);
    if (rules::CheckSkillChoice(allowance, tried).empty()) profile.proficient = tried;
  }
  for (const auto& entry : by_rank) {
    const rules::SkillSet tried = profile.expertise | (1u << entry.second);
    if (rules::CheckExpertiseChoice(allowance, profile.proficient, tried).empty()) profile.expertise = tried;
  }
  return profile;
}

std::string Names(rules::SkillSet set) {
  std::string names;
  for (int i = 0; i < rules::kSkill2024Count; ++i) {
    if (!(set & (1u << i))) continue;
    if (!names.empty()) names += ", ";
    names += rules::Skill2024Name(static_cast<rules::Skill2024>(i));
  }
  return names.empty() ? "none" : names;
}

void StoreProfile(void* creature, uint32_t id, const Profile& profile, int how) {
  engine::SetLocalInt(creature, kVarProficient, static_cast<int>(profile.proficient));
  engine::SetLocalInt(creature, kVarExpertise, static_cast<int>(profile.expertise));
  engine::SetLocalInt(creature, kVarSet, how);
  g_profiles[id] = profile;
}

// A character's proficiencies: its local variables (cached), or from its
// ranks the first time (told to the player).
Profile CharacterProfile(void* creature, void* stats) {
  const uint32_t id = Field<uint32_t>(creature, nwmain::kCreatureObjectId);
  const auto cached = g_profiles.find(id);
  if (cached != g_profiles.end()) return cached->second;
  Profile profile;
  if (engine::GetLocalInt(creature, kVarSet) != 0) {
    profile.proficient = static_cast<rules::SkillSet>(engine::GetLocalInt(creature, kVarProficient));
    profile.expertise = static_cast<rules::SkillSet>(engine::GetLocalInt(creature, kVarExpertise));
    g_profiles[id] = profile;
    return profile;
  }
  profile = ProfileFromRanks(stats, rules::SkillAllowanceFor(ClassLevelsOf(stats), SpeciesOf(stats)));
  StoreProfile(creature, id, profile, 2);
  Log("skills: 0x%08X 2024 proficiencies taken from its ranks: %s; Expertise: %s", id, Names(profile.proficient).c_str(),
      Names(profile.expertise).c_str());
  if (IsPlayerControlled(id)) {
    TellPlayer(id, "nwn5e: your 2024 skills were taken from your ranks - proficient: " + Names(profile.proficient) +
                       "; Expertise: " + Names(profile.expertise) + ". Change them with !skills.");
  }
  return profile;
}

// The 2024 value of `skill` for `stats`, or false when NWN's stands.
bool SkillValue2024(void* stats, uint8_t skill, void* vs, int* value) {
  if (!stats || Field<int32_t>(stats, nwmain::kStatsDungeonMaster) != 0) return false;
  const auto mapped = rules::Skill2024ForNwnSkill(skill);
  void* creature = Field<void*>(stats, nwmain::kStatsCreature);
  if (!mapped || !creature) return false;
  const CreatureFacts facts = ReadCreatureFacts(stats);
  if (!facts.plausible) return false;
  const bool character = CountsAsCharacter(stats, facts);
  Profile profile;
  if (character) {
    profile = CharacterProfile(creature, stats);
  } else if (RanksFor(stats, *mapped) > 0) {
    profile.proficient = rules::SkillBit(*mapped);  // monsters: proficient where they have ranks
  }
  const int pb = rules::ProficiencyBonusForCreature(character, facts.level, facts.challenge_rating);
  const int effects = g_total_effect_bonus(creature, nullptr, nwmain::kEffectBonusTypeSkill, vs, 0, 0, 0, 0, skill,
                                           0xFF, 0);
  *value = std::clamp(rules::SkillBonus2024(AbilityModifier(stats, rules::AbilityFor(*mapped)), pb,
                                            rules::Has(profile.proficient, *mapped),
                                            rules::Has(profile.expertise, *mapped), effects),
                      -127, 127);
  return true;
}

char __fastcall GetSkillRank(void* stats, void* edx, uint8_t skill, void* vs, int base_only) {
  const char engine = g_get_skill_rank(stats, edx, skill, vs, base_only);
  int value = 0;
  if (base_only || !SkillValue2024(stats, skill, vs, &value)) return engine;
  if (g_config.log_attacks) {
    const uint32_t id = ObjectIdOf(stats);
    if (g_logged.First(LogOnce::Key(id, skill, value))) {
      Log("skills: 0x%08X %s: 3e %+d -> 2024 %+d", id, rules::Faithful2024SkillLabel(skill), engine, value);
    }
  }
  return static_cast<char>(value);
}

// SK3 (a): replaces the GetSkillRank call of the script command (command 315).
char __fastcall ScriptGetSkillRank(void* stats, void* edx, uint8_t skill, void* vs, int base_only) {
  const char value = g_get_skill_rank_entry(stats, edx, skill, vs, base_only);
  if (base_only || !rules::Skill2024ForNwnSkill(skill) || !stats ||
      Field<int32_t>(stats, nwmain::kStatsDungeonMaster) != 0) {
    return value;
  }
  return static_cast<char>(std::clamp(rules::ScriptSkillValue2024(value), -127, 127));
}

bool PatchScriptSkillRankCall() {
  const auto* code = reinterpret_cast<const uint8_t*>(nwmain::kScriptSkillRankCall);
  int32_t rel = 0;
  memcpy(&rel, code + 1, sizeof(rel));
  if (code[0] != 0xe8 || nwmain::kScriptSkillRankCall + nwmain::kCallLength + rel != nwmain::kGetSkillRank) {
    Log("SKIP skills: 0x%08X is not the mapped GetSkillRank call", nwmain::kScriptSkillRankCall);
    return false;
  }
  uint8_t expected[5];
  memcpy(expected, code, sizeof(expected));
  const int32_t new_rel = static_cast<int32_t>(reinterpret_cast<uintptr_t>(&ScriptGetSkillRank) -
                                               (nwmain::kScriptSkillRankCall + nwmain::kCallLength));
  uint8_t call[5] = {0xe8};
  memcpy(call + 1, &new_rel, sizeof(new_rel));
  return PatchBytes("skills: script skill ranks on the 3e scale (SK3 a)", nwmain::kScriptSkillRankCall, expected,
                    call, sizeof(call));
}

}  // namespace

// `!skills [expertise] [skill ...]` (rules_chat.cpp).
std::vector<std::string> SkillsCommand(void* creature, const std::string& args) {
  std::vector<std::string> lines;
  if (!Faithful()) {
    lines.push_back("nwn5e: skills follow NWN's rules (skills=nwn); nothing to choose.");
    return lines;
  }
  void* stats = creature ? Field<void*>(creature, nwmain::kCreatureStats) : nullptr;
  if (!stats) return lines;
  const uint32_t id = Field<uint32_t>(creature, nwmain::kCreatureObjectId);
  const rules::SkillAllowance allowance = rules::SkillAllowanceFor(ClassLevelsOf(stats), SpeciesOf(stats));
  Profile profile = CharacterProfile(creature, stats);
  std::string rest = args;
  const bool expertise = rest.size() >= 9 && _strnicmp(rest.c_str(), "expertise", 9) == 0;
  if (expertise) rest = rest.substr(9);
  if (rest.find_first_not_of(' ') == std::string::npos) {
    if (expertise) {
      lines.push_back("nwn5e: Expertise (" + std::to_string(rules::CountSkills(profile.expertise)) + " of " +
                      std::to_string(allowance.expertise) + "): " + Names(profile.expertise) +
                      ". Set with !skills expertise <skill> ...");
      return lines;
    }
    lines.push_back("nwn5e: 2024 skills - proficient: " + Names(profile.proficient) + "; Expertise: " +
                    Names(profile.expertise) + ".");
    lines.push_back("nwn5e: you choose " + std::to_string(allowance.class_skills) + " from your class (" +
                    Names(allowance.class_list) + ")" +
                    (allowance.multiclass_skills ? ", " + std::to_string(allowance.multiclass_skills) +
                                                       " from your other classes" : std::string()) +
                    ", " + std::to_string(allowance.background_skills) + " more and 1 tool (background)" +
                    (allowance.species_any_skill ? ", 1 more (Human)" : std::string()) +
                    (allowance.species_list ? ", 1 of Insight, Perception or Survival (Elf)" : std::string()) +
                    (allowance.granted ? "; always: " + Names(allowance.granted) : std::string()) +
                    (allowance.expertise ? "; Expertise in " + std::to_string(allowance.expertise) : std::string()) +
                    ".");
    lines.push_back("nwn5e: set them with !skills <skill> <skill> ... (replaces the list), Expertise with "
                    "!skills expertise <skill> ...");
    return lines;
  }
  std::vector<rules::Skill2024> chosen;
  std::string unknown;
  if (!rules::ParseSkill2024List(rest, &chosen, &unknown)) {
    lines.push_back("nwn5e: unknown skill \"" + unknown + "\" (2024 names: Acrobatics, Animal Handling, Arcana, "
                    "Athletics, Deception, History, Insight, Intimidation, Investigation, Medicine, Nature, "
                    "Perception, Performance, Persuasion, Religion, Sleight of Hand, Stealth, Survival, Thieves' "
                    "Tools, Smith's Tools).");
    return lines;
  }
  rules::SkillSet set = 0;
  for (rules::Skill2024 skill : chosen) set |= rules::SkillBit(skill);
  if (expertise) {
    const std::string problem = rules::CheckExpertiseChoice(allowance, profile.proficient, set);
    if (!problem.empty()) {
      lines.push_back("nwn5e: Expertise not changed - " + problem + ".");
      return lines;
    }
    profile.expertise = set;
  } else {
    set |= allowance.granted;
    const std::string problem = rules::CheckSkillChoice(allowance, set);
    if (!problem.empty()) {
      lines.push_back("nwn5e: skills not changed - " + problem + ".");
      return lines;
    }
    profile.proficient = set;
    profile.expertise &= set;
  }
  StoreProfile(creature, id, profile, 1);
  Log("skills: 0x%08X chose - proficient: %s; Expertise: %s", id, Names(profile.proficient).c_str(),
      Names(profile.expertise).c_str());
  lines.push_back("nwn5e: 2024 skills - proficient: " + Names(profile.proficient) + "; Expertise: " +
                  Names(profile.expertise) + ".");
  return lines;
}

void InstallSkillRules() {
  if (!Faithful()) return;
  InstallHook({"CNWSCreatureStats::GetSkillRank (2024 skills)", nwmain::kGetSkillRank, nwmain::kGetSkillRankPrologue,
               sizeof(nwmain::kGetSkillRankPrologue), reinterpret_cast<void*>(&GetSkillRank),
               reinterpret_cast<void**>(&g_get_skill_rank)});
  PatchScriptSkillRankCall();
}
