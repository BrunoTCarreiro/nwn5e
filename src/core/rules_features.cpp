// Decisions CM9 / CM10 (PLAN.md): the frame for the 2024 class features —
// `!features` lists a character's features by class level with what nwn5e
// does with each (rules::ClassFeatures2024), and its SRD subclass, granted at
// class level 3 (the prestige classes are off in faithful: rules_progression).
// The features themselves are built class by class, each from its brief.

#include <windows.h>

#include <algorithm>
#include <string>
#include <unordered_map>
#include <vector>

#include "config.h"
#include "creature.h"
#include "engine_queries.h"
#include "log.h"
#include "nwmain_1_69.h"
#include "rules/class_features.h"
#include "rules/nwn_conversion.h"

namespace {

const char* ClassName(rules::CharacterClass c) {
  static const char* const kNames[] = {"Barbarian", "Bard",   "Cleric", "Druid",    "Fighter", "Monk",
                                       "Paladin",   "Ranger", "Rogue",  "Sorcerer", "Warlock", "Wizard"};
  return kNames[static_cast<int>(c)];
}

}  // namespace

// !features
std::vector<std::string> FeaturesCommand(void* creature, const std::string& /*args*/) {
  if (g_config.class_features != rules::RuleVariant::kFaithful) {
    return {"nwn5e: !features shows the 2024 class features (class_features=faithful)."};
  }
  void* stats = Field<void*>(creature, nwmain::kCreatureStats);
  std::vector<std::string> lines;
  for (const rules::ClassLevels& c : rules::GroupClassLevels(ReadClassSlots(stats))) {
    const int level = c.levels < 20 ? c.levels : 20;
    std::string head = "nwn5e: " + std::string(ClassName(c.character_class)) + " " + std::to_string(level);
    head += level >= rules::kSubclassLevel ? std::string(" - ") + rules::Subclass2024(c.character_class)
                                           : std::string(" - ") + rules::Subclass2024(c.character_class) + " at 3";
    lines.push_back(head);
    if (!rules::HasFeatureList(c.character_class)) {
      lines.push_back("  its 2024 features come with its own brief (Phase 3, one class at a time).");
      continue;
    }
    for (const rules::ClassFeature& f : rules::ClassFeatures2024(c.character_class, level)) {
      lines.push_back("  " + std::to_string(f.level) + " " + f.name + (f.subclass ? " (subclass)" : "") + " - " +
                      f.status);
    }
  }
  if (lines.empty()) lines.push_back("nwn5e: no 2024 class.");
  return lines;
}

// --- Granted features (Fighter F2 Second Wind, ...) ---------------------------
// The feats a character's 2024 class levels give (rules::GrantedFeats2024)
// are added by nwn5e — so a character made before gets them too — with full
// uses, and recorded on the character (nwn5e_granted_feats: a bit per
// rules::GrantableFeats2024 entry); NWN mode removes what was recorded.

namespace {

using FeatFn = int(__fastcall*)(void* stats, void* edx, uint32_t feat);
using SetUsesFn = void(__fastcall*)(void* stats, void* edx, uint32_t feat, uint32_t uses);

const auto g_has_feat = reinterpret_cast<FeatFn>(nwmain::kHasFeat);
const auto g_add_feat = reinterpret_cast<FeatFn>(nwmain::kAddFeat);
const auto g_remove_feat = reinterpret_cast<FeatFn>(nwmain::kRemoveFeat);
const auto g_feat_total_uses = reinterpret_cast<FeatFn>(nwmain::kGetFeatTotalUses);
const auto g_set_feat_uses = reinterpret_cast<SetUsesFn>(nwmain::kSetFeatRemainingUses);
constexpr const char* kVarGranted = "nwn5e_granted_feats";
constexpr DWORD kSyncEveryMs = 2000;
std::unordered_map<uint32_t, DWORD> g_last_sync;

}  // namespace

void FeaturesCreatureUpdate(void* creature) {
  void* stats = Field<void*>(creature, nwmain::kCreatureStats);
  if (!stats || !engine::GetArea(creature)) return;
  const uint32_t id = Field<uint32_t>(creature, nwmain::kCreatureObjectId);
  if (id == nwmain::kObjectInvalid) return;
  const DWORD now = GetTickCount();
  auto last = g_last_sync.find(id);
  if (last != g_last_sync.end() && now - last->second < kSyncEveryMs) return;
  g_last_sync[id] = now;
  const CreatureFacts facts = ReadCreatureFacts(stats);
  if (!facts.character || !facts.plausible) return;
  const std::vector<int> wanted = g_config.class_features == rules::RuleVariant::kFaithful
                                      ? rules::GrantedFeats2024(rules::GroupClassLevels(ReadClassSlots(stats)))
                                      : std::vector<int>();
  const int recorded = engine::GetLocalInt(creature, kVarGranted);
  int now_recorded = recorded;
  const std::vector<int>& grantable = rules::GrantableFeats2024();
  for (size_t i = 0; i < grantable.size() && i < 31; ++i) {
    const int feat = grantable[i];
    const bool want = std::find(wanted.begin(), wanted.end(), feat) != wanted.end();
    const bool had = (recorded >> i) & 1;
    if (want && !had) {
      if (!g_has_feat(stats, nullptr, feat)) {
        g_add_feat(stats, nullptr, feat);
        g_set_feat_uses(stats, nullptr, feat, g_feat_total_uses(stats, nullptr, feat) & 0xFF);
      }
      now_recorded |= 1 << i;
      Log("features: %s 0x%08X granted feat %d", KindOf(facts), id, feat);
    } else if (!want && had) {
      if (g_has_feat(stats, nullptr, feat)) g_remove_feat(stats, nullptr, feat);
      now_recorded &= ~(1 << i);
      Log("features: %s 0x%08X removed feat %d", KindOf(facts), id, feat);
    }
  }
  if (now_recorded != recorded) engine::SetLocalInt(creature, kVarGranted, now_recorded);
}
