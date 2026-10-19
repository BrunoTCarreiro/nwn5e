#pragma once

// Phase 2 skills (PLAN.md "Phase 2 — skills and checks", decided 2026-10-19):
// NWN's skill rows as the 2024 (SRD 5.2) skills and tools, proficiency instead
// of ranks, and what a character can choose. The engine side reads the
// creature; these functions only decide.

#include <stdint.h>

#include <optional>
#include <string>
#include <vector>

#include "rules/combat.h"
#include "rules/saves.h"

namespace rules {

// The 2024 skills, then the tools nwn5e maps NWN skills to.
enum class Skill2024 {
  kAcrobatics, kAnimalHandling, kArcana, kAthletics, kDeception, kHistory, kInsight, kIntimidation,
  kInvestigation, kMedicine, kNature, kPerception, kPerformance, kPersuasion, kReligion, kSleightOfHand,
  kStealth, kSurvival, kThievesTools, kSmithsTools,
  kCount
};
constexpr int kSkill2024Count = static_cast<int>(Skill2024::kCount);
bool IsTool(Skill2024 skill);
const char* Skill2024Name(Skill2024 skill);
Ability AbilityFor(Skill2024 skill);  // 2024 key ability (Smith's Tools: Strength)

// NWN 1.69 skills.2da rows.
enum NwnSkill {
  kNwnAnimalEmpathy = 0, kNwnConcentration, kNwnDisableTrap, kNwnDiscipline, kNwnHeal, kNwnHide, kNwnListen,
  kNwnLore, kNwnMoveSilently, kNwnOpenLock, kNwnParry, kNwnPerform, kNwnPersuade, kNwnPickPocket, kNwnSearch,
  kNwnSetTrap, kNwnSpellcraft, kNwnSpot, kNwnTaunt, kNwnUseMagicDevice, kNwnAppraise, kNwnTumble, kNwnCraftTrap,
  kNwnBluff, kNwnIntimidate, kNwnCraftArmor, kNwnCraftWeapon, kNwnRide,
  kNwnSkillCount
};

// SK1: the 2024 skill or tool an NWN skill row stands for in faithful mode;
// none for Concentration, Discipline, Parry, Taunt, Appraise, Use Magic Device
// and Craft Trap (SK6–SK8, Phase 3), which keep NWN's value until then.
std::optional<Skill2024> Skill2024ForNwnSkill(int nwn_skill);

// SK1: the faithful name of an NWN skill row as the game shows it, e.g. "Stealth
// (Hide)"; nullptr for rows without a 2024 skill.
const char* Faithful2024SkillLabel(int nwn_skill);
// The same, by the dialog.tlk string the game asks for (skills.2da Name).
const char* Faithful2024SkillLabelForStrref(uint32_t strref);

// "stealth", "sleight of hand", "thieves' tools", ... (any case; spaces,
// apostrophes and hyphens ignored); nullopt if unknown.
std::optional<Skill2024> ParseSkill2024(const std::string& text);
// Splits "stealth sleight of hand perception" into skills; on an unknown word
// returns false and puts it in `unknown`.
bool ParseSkill2024List(const std::string& text, std::vector<Skill2024>* skills, std::string* unknown);

// SK2: ability modifier + proficiency bonus if proficient + again with
// Expertise + item and spell effects.
int SkillBonus2024(int ability_modifier, int proficiency_bonus, bool proficient, bool expertise, int effect_bonus);

// SK3 (a): a skill value a *script* reads (the official campaigns' AutoDC
// checks, DCs set for 3e totals), scaled to the 3e range — about twice the 2024
// bonus — so conversation checks stay as passable as in NWN.
int ScriptSkillValue2024(int skill_bonus);

// A set of skills (bit k = Skill2024 k).
using SkillSet = uint32_t;
inline SkillSet SkillBit(Skill2024 skill) { return 1u << static_cast<int>(skill); }
inline bool Has(SkillSet set, Skill2024 skill) { return (set & SkillBit(skill)) != 0; }
int CountSkills(SkillSet set);

// What a character can choose (2024): class skills (first class: its count from
// its list; multiclassing into Bard, Ranger or Rogue: one more from that list),
// background (2 skills, 1 tool), species (Human: 1 skill; Elf: Insight,
// Perception or Survival), the Rogue's Thieves' Tools, and Expertise (Rogue 1 /
// 6, Bard 2 / 9, Ranger 2 / 9, Wizard 2).
enum class Species2024 { kOther, kHuman, kElf };
struct SkillAllowance {
  int class_skills = 0;
  SkillSet class_list = 0;      // the skills the class picks come from
  int multiclass_skills = 0;    // picks from multiclass_list
  SkillSet multiclass_list = 0;
  int background_skills = 2;
  int background_tools = 1;
  bool species_any_skill = false;   // Human: Skillful
  SkillSet species_list = 0;        // Elf: Keen Senses
  SkillSet granted = 0;             // always proficient (Rogue: Thieves' Tools)
  int expertise = 0;
  SkillSet expertise_list = 0;      // 0 = any proficient skill
  int TotalSkills() const;          // skill choices, tools excluded
};
SkillAllowance SkillAllowanceFor(const std::vector<ClassLevels>& classes, Species2024 species);

// Can `chosen` (skills and tools, `granted` excluded or included) be split
// into those choices? Returns an empty string when it can, else the reason.
std::string CheckSkillChoice(const SkillAllowance& allowance, SkillSet chosen);
std::string CheckExpertiseChoice(const SkillAllowance& allowance, SkillSet proficient, SkillSet expertise);

// SK5 (faithful): an observer's Perception die — the d20 it rolled when it
// searches (detect mode: the Search action), else 10 (passive Perception).
int PerceptionDie(bool searching, int d20);

}  // namespace rules
