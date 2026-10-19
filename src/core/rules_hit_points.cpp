// Decision CM3 (PLAN.md), faithful: 2024 hit points, computed whenever the
// engine asks (RE-NOTES "Hit points"): the first level's die at its maximum,
// then the fixed average (die / 2 + 1) per level, with the 2024 dice
// (Bard / Rogue d8, Sorcerer / Wizard d6; prestige classes use their parent
// class's, decision 10), + the Constitution modifier per level, at least 1.
// NWN's stored rolls, Toughness, Epic Toughness and the Pale Master's bonus
// are not counted (SRD 5.2 has none of them, CM8). Player characters and
// henchmen (decision 11); creatures keep their own. A character above level
// 20 counts its first 20 (CM13). Nothing is written: the mode can switch back.
// NWN mode: NWN's.

#include <stdint.h>

#include <vector>

#include "config.h"
#include "creature.h"
#include "hooks.h"
#include "log.h"
#include "log_once.h"
#include "nwmain_1_69.h"
#include "rules/hit_points.h"
#include "rules/nwn_conversion.h"
#include "rules/progression.h"

namespace {

using GetMaxHitPointsFn = int(__fastcall*)(void* creature, void* edx, int include_toughness);
using GetLevelStatsFn = void*(__fastcall*)(void* stats, void* edx, uint32_t index);

GetMaxHitPointsFn g_get_max_hit_points = nullptr;
const auto g_get_level_stats = reinterpret_cast<GetLevelStatsFn>(nwmain::kGetLevelStats);
LogOnce g_logged(300);

int HitDie(int nwn_class_id) {
  if (const auto c = rules::ClassFromNwnId(nwn_class_id)) return rules::HitDie2024(*c);
  void* rules_object = *reinterpret_cast<void**>(nwmain::kRulesObject);
  auto* classes = rules_object ? Field<uint8_t*>(rules_object, nwmain::kRulesClasses) : nullptr;
  if (!classes || nwn_class_id < 0 || nwn_class_id >= Field<uint8_t>(rules_object, nwmain::kRulesClassCount)) return 8;
  return Field<uint8_t>(classes + nwn_class_id * nwmain::kClassStride, nwmain::kClassHitDie);
}

// The hit die of each level, in the order taken: the level history when it
// covers every level (player characters), else the class slots in order.
std::vector<int> LevelDice(void* stats, int level) {
  level = level < rules::kMaxLevel2024 ? level : rules::kMaxLevel2024;
  std::vector<int> dice;
  if (static_cast<int>(Field<uint32_t>(stats, nwmain::kStatsLevelHistoryCount)) >= level) {
    for (int i = 0; i < level; ++i) {
      void* level_stats = g_get_level_stats(stats, nullptr, static_cast<uint32_t>(i));
      if (!level_stats) {
        dice.clear();
        break;
      }
      dice.push_back(HitDie(Field<uint8_t>(level_stats, nwmain::kLevelStatsClass)));
    }
    if (!dice.empty()) return dice;
  }
  for (const rules::NwnClassSlot& slot : ReadClassSlots(stats)) {
    for (int i = 0; i < slot.levels && static_cast<int>(dice.size()) < level; ++i) dice.push_back(HitDie(slot.nwn_class_id));
  }
  return dice;
}

int __fastcall GetMaxHitPoints(void* creature, void* edx, int include_toughness) {
  const int nwn = g_get_max_hit_points(creature, edx, include_toughness);
  void* stats = Field<void*>(creature, nwmain::kCreatureStats);
  if (!stats) return nwn;
  const CreatureFacts facts = ReadCreatureFacts(stats);
  if (!facts.character || !facts.plausible) return nwn;
  const std::vector<int> dice = LevelDice(stats, facts.level);
  if (dice.empty()) return nwn;
  const int con = Field<int8_t>(stats, nwmain::kStatsConModifier);
  const int hp = rules::MaxHitPoints2024(dice, con);
  if (g_logged.First(LogOnce::Key(ObjectIdOf(stats), nwn, hp))) {
    Log("hit points: %s 0x%08X max %d (2024: d%d max at level 1, then average; Con %+d; %d levels) - NWN %d",
        KindOf(facts), ObjectIdOf(stats), hp, dice[0], con, static_cast<int>(dice.size()), nwn);
  }
  return hp;
}

}  // namespace

void InstallHitPointRules() {
  if (g_config.hit_points != rules::RuleVariant::kFaithful) return;
  InstallHook({"CNWSCreature::GetMaxHitPoints (2024 hit points)", nwmain::kGetMaxHitPoints,
               nwmain::kGetMaxHitPointsPrologue, sizeof(nwmain::kGetMaxHitPointsPrologue),
               reinterpret_cast<void*>(&GetMaxHitPoints), reinterpret_cast<void**>(&g_get_max_hit_points)});
}
