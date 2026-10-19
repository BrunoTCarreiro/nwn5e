// Decisions CM1 / CM2 (PLAN.md), faithful (RE-NOTES "XP, level cap and
// multiclassing"):
// - CM1: the engine's XP table is 2024's (written once the rules object has
//   loaded it), the level cap is 20 (also with HotU), and a script's SetXP is
//   read in NWN's table (rules::ScriptSetXp2024): HotU's 105,000 = level 15
//   becomes 165,000, the respawn scripts' "never lose a level" clamp neither
//   gives XP nor takes levels, and no script XP loss drops a level.
// - CM2: no multiclass XP penalty; a new class needs 13 in its primary
//   ability and in every current class's (CanLevelUpInClass, which feeds both
//   the level-up screen's class list and the server's checks — henchmen too).
// NWN mode: NWN's.

#include <stdint.h>
#include <string.h>

#include <vector>

#include "config.h"
#include "creature.h"
#include "hooks.h"
#include "log.h"
#include "log_once.h"
#include "nwmain_1_69.h"
#include "rules/nwn_conversion.h"
#include "rules/progression.h"

namespace {

using RulesLoaderFn = void(__fastcall*)(void* rules_object, void* edx);
using SetExperienceFn = void(__fastcall*)(void* stats, void* edx, uint32_t xp, int level_changes);
using CanLevelUpInClassFn = int(__fastcall*)(void* stats, void* edx, uint32_t nwn_class);
using StatFn = int(__fastcall*)(void* stats, void* edx);
using LevelFn = int(__fastcall*)(void* stats, void* edx, int include_negative);

const auto g_rules_last_loader = reinterpret_cast<RulesLoaderFn>(nwmain::kRulesLastLoader);
const auto g_set_experience = reinterpret_cast<SetExperienceFn>(nwmain::kSetExperience);
const auto g_get_level = reinterpret_cast<LevelFn>(nwmain::kGetLevel);
CanLevelUpInClassFn g_can_level_up_in_class = nullptr;
LogOnce g_logged(200);
int g_set_xp_lines = 0;

uint32_t* XpTable(void* rules_object) {
  return reinterpret_cast<uint32_t*>(static_cast<uint8_t*>(rules_object) + nwmain::kRulesXpTable);
}

// In place of the last loader call of the rules constructor (0x47A6F8) and of
// ReloadAll (0x47AD4C): the XP table (filled from exptable.2da just before)
// becomes faithful's.
void __fastcall AfterRulesLoaded(void* rules_object, void* edx) {
  g_rules_last_loader(rules_object, edx);
  uint32_t* table = XpTable(rules_object);
  const uint32_t nwn_level_2 = table[1];
  for (int i = 0; i < rules::kXpTableSize; ++i) table[i] = rules::FaithfulXpForLevel(i + 1);
  Log("progression: XP table set to 2024 (level 2 at %u, 5 at %u, 20 at %u; NWN's level 2 was %u)", table[1],
      table[4], table[19], nwn_level_2);
}

// In place of the SetXP command's SetExperience call (0x6FF52D).
void __fastcall ScriptSetExperience(void* stats, void* edx, uint32_t xp, int level_changes) {
  if (Field<uint32_t>(stats, nwmain::kStatsIsPlayer) != 0) {
    const uint32_t current = Field<uint32_t>(stats, nwmain::kStatsExperience);
    const int level = g_get_level(stats, nullptr, 0) & 0xFF;
    const uint32_t converted = rules::ScriptSetXp2024(xp, current, level);
    if (converted != xp && g_set_xp_lines < 100) {
      ++g_set_xp_lines;
      Log("progression: script SetXP %u -> %u for 0x%08X (level %d, XP %u)", xp, converted, ObjectIdOf(stats),
          level, current);
    }
    xp = converted;
  }
  g_set_experience(stats, edx, xp, level_changes);
}

const char* ClassName(rules::CharacterClass c) {
  static const char* const kNames[] = {"Barbarian", "Bard",   "Cleric", "Druid",    "Fighter", "Monk",
                                       "Paladin",   "Ranger", "Rogue",  "Sorcerer", "Warlock", "Wizard"};
  return kNames[static_cast<int>(c)];
}

void ReadScores(void* stats, int scores[6]) {
  static const uintptr_t kGetters[6] = {nwmain::kGetStrStat, nwmain::kGetDexStat, nwmain::kGetConStat,
                                        nwmain::kGetIntStat, nwmain::kGetWisStat, nwmain::kGetChaStat};
  for (int i = 0; i < 6; ++i) scores[i] = reinterpret_cast<StatFn>(kGetters[i])(stats, nullptr) & 0xFF;
}

int __fastcall CanLevelUpInClass(void* stats, void* edx, uint32_t nwn_class) {
  const int allowed = g_can_level_up_in_class(stats, edx, nwn_class);
  if (!allowed) return allowed;
  const auto next = rules::ClassFromNwnId(static_cast<int>(nwn_class & 0xFF));
  if (!next) return allowed;
  std::vector<rules::CharacterClass> current;
  for (const rules::NwnClassSlot& slot : ReadClassSlots(stats)) {
    if (const auto c = rules::ClassFromNwnId(slot.nwn_class_id)) current.push_back(*c);
  }
  int scores[6];
  ReadScores(stats, scores);
  rules::CharacterClass failing = *next;
  if (rules::CanMulticlassInto(*next, current, scores, &failing)) return allowed;
  if (g_logged.First(LogOnce::Key(ObjectIdOf(stats), nwn_class & 0xFF, static_cast<int>(failing)))) {
    Log("multiclass: 0x%08X can't take %s - %s needs %s (Str %d Dex %d Con %d Int %d Wis %d Cha %d)",
        ObjectIdOf(stats), ClassName(*next), ClassName(failing), rules::PrimaryAbilityText(failing), scores[0],
        scores[1], scores[2], scores[3], scores[4], scores[5]);
  }
  return 0;
}

bool RedirectCall(const char* name, uintptr_t site, uintptr_t expected_target, const void* new_target) {
  const auto* code = reinterpret_cast<const uint8_t*>(site);
  int32_t rel = 0;
  memcpy(&rel, code + 1, sizeof(rel));
  if (code[0] != 0xe8 || site + nwmain::kCallLength + rel != expected_target) {
    Log("SKIP %s: 0x%08X is not the mapped call", name, site);
    return false;
  }
  uint8_t expected[5];
  memcpy(expected, code, sizeof(expected));
  uint8_t call[5] = {0xe8};
  const int32_t new_rel = static_cast<int32_t>(reinterpret_cast<uintptr_t>(new_target) - (site + nwmain::kCallLength));
  memcpy(call + 1, &new_rel, sizeof(new_rel));
  return PatchBytes(name, site, expected, call, sizeof(call));
}

}  // namespace

void InstallProgressionRules() {
  if (g_config.progression == rules::RuleVariant::kFaithful) {
    int applied = 0;
    applied += RedirectCall("progression: 2024 XP table (faithful)", nwmain::kRulesLoadedCall,
                            nwmain::kRulesLastLoader, reinterpret_cast<void*>(&AfterRulesLoaded))
                   ? 1
                   : 0;
    applied += RedirectCall("progression: 2024 XP table again after ReloadAll (faithful)",
                            nwmain::kRulesReloadedCall, nwmain::kRulesLastLoader,
                            reinterpret_cast<void*>(&AfterRulesLoaded))
                   ? 1
                   : 0;
    const uint8_t cap_20 = 20;
    applied += PatchBytes("progression: level cap 20, also with HotU (faithful)", nwmain::kLevelCapHotU,
                          &nwmain::kLevelCapHotUByte, &cap_20, 1)
                   ? 1
                   : 0;
    applied += RedirectCall("progression: script SetXP read in NWN's table (faithful)", nwmain::kScriptSetXpCall,
                            nwmain::kSetExperience, reinterpret_cast<void*>(&ScriptSetExperience))
                   ? 1
                   : 0;
    Log("progression: %d of 4 faithful patches applied", applied);
  }
  if (g_config.multiclass == rules::RuleVariant::kFaithful) {
    int applied = PatchBytes("multiclass: no XP penalty (faithful)", nwmain::kMulticlassXpPenalty,
                             nwmain::kMulticlassXpPenaltyBytes, nwmain::kMulticlassXpPenaltySkip,
                             sizeof(nwmain::kMulticlassXpPenaltySkip))
                      ? 1
                      : 0;
    applied += InstallHook({"CNWSCreatureStats::CanLevelUpInClass (2024 multiclass prerequisites)",
                            nwmain::kCanLevelUpInClass, nwmain::kCanLevelUpInClassPrologue,
                            sizeof(nwmain::kCanLevelUpInClassPrologue), reinterpret_cast<void*>(&CanLevelUpInClass),
                            reinterpret_cast<void**>(&g_can_level_up_in_class)})
                   ? 1
                   : 0;
    Log("multiclass: %d of 2 faithful changes installed", applied);
  }
}
