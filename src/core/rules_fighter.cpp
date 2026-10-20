// Fighter features built from its brief (PLAN.md "Phase 3 — Fighter"), in
// faithful (class_features):
// - C1 Champion's Improved / Superior Critical: weapon and unarmed attacks
//   crit on 19-20 from Fighter 3, 18-20 from 15 (GetCriticalHitRoll,
//   rules_crit.cpp); as decision 8 says, a 19 that misses still misses;
// - C2 Remarkable Athlete (Champion 3): advantage on initiative
//   (rules_initiative.cpp);
// - F6 Studied Attacks (13): a miss marks the target; the Fighter's next
//   attack at it before the end of its next turn has advantage
//   (rules_advantage.cpp, like Vex).
// Prestige-class levels count as Fighter levels (decision 10).

#include <windows.h>
#include <stdint.h>

#include <unordered_map>

#include "config.h"
#include "creature.h"
#include "engine_queries.h"
#include "log.h"
#include "log_once.h"
#include "mastery.h"
#include "nwmain_1_69.h"
#include "rules/class_features.h"
#include "rules/weapons.h"

namespace {

using GetAttackFn = void*(__fastcall*)(void* round, void* edx, int index);
const auto g_get_attack = reinterpret_cast<GetAttackFn>(nwmain::kGetAttack);

struct StudiedMark {
  uint32_t target;
  uint32_t rounds;  // the Fighter's ended rounds when it missed
  DWORD tick;
};
std::unordered_map<uint32_t, StudiedMark> g_studied;  // by attacker id
constexpr DWORD kMarkMaxAgeMs = 30000;
LogOnce g_logged(200);

bool Faithful() { return g_config.class_features == rules::RuleVariant::kFaithful; }

// Fighter levels of a character (player character or henchman), else 0.
int FighterLevelOf(void* stats) {
  if (!stats || !Faithful()) return 0;
  const CreatureFacts facts = ReadCreatureFacts(stats);
  if (!facts.character || !facts.plausible) return 0;
  return ClassLevel2024(stats, rules::CharacterClass::kFighter);
}

uint32_t IdOf(void* creature) { return Field<uint32_t>(creature, nwmain::kCreatureObjectId); }

}  // namespace

// C1: the lowest d20 that crits for this creature's weapon attacks (20 = no change).
int ChampionCriticalFloorFor(void* stats) {
  const int floor = rules::ChampionCriticalFloor(FighterLevelOf(stats));
  if (floor < 20 && g_logged.First(LogOnce::Key(ObjectIdOf(stats), floor, 1))) {
    Log("fighter: 0x%08X Champion crits on %d-20", ObjectIdOf(stats), floor);
  }
  return floor;
}

// C2: advantage on initiative.
bool HasRemarkableAthleteAdvantage(void* creature) {
  return creature && rules::HasRemarkableAthlete(FighterLevelOf(Field<void*>(creature, nwmain::kCreatureStats)));
}

// F6: advantage from an earlier miss at this target, used up by this attack.
MasteryRollSources TakeStudiedAttacks(void* attacker, uint32_t target_id) {
  MasteryRollSources sources;
  if (!attacker || !Faithful()) return sources;
  const auto mark = g_studied.find(IdOf(attacker));
  if (mark == g_studied.end()) return sources;
  const bool live = GetTickCount() - mark->second.tick <= kMarkMaxAgeMs &&
                    rules::VexActive(CombatRoundsEnded(IdOf(attacker)), mark->second.rounds);
  if (!live) {
    g_studied.erase(mark);
  } else if (mark->second.target == target_id) {
    ++sources.advantage;
    sources.note = "Studied Attacks advantage";
    g_studied.erase(mark);
  }
  return sources;
}

// F6: after an attack roll — a miss marks the target.
void StudiedAttacksAfterAttackRoll(void* attacker, void* target_object) {
  if (!attacker || !target_object || !Faithful()) return;
  if (!rules::HasStudiedAttacks(FighterLevelOf(Field<void*>(attacker, nwmain::kCreatureStats)))) return;
  void* round = Field<void*>(attacker, nwmain::kCreatureCombatRound);
  const void* attack = round ? g_get_attack(round, nullptr, Field<uint8_t>(round, nwmain::kRoundCurrentAttack)) : nullptr;
  if (!attack || !rules::MasteryAttackMiss(Field<uint8_t>(attack, nwmain::kAttackDataResult))) return;
  const uint32_t target_id = Field<uint32_t>(target_object, nwmain::kCreatureObjectId);
  if (!engine::GetCreature(target_id)) return;  // creatures only
  g_studied[IdOf(attacker)] = StudiedMark{target_id, CombatRoundsEnded(IdOf(attacker)), GetTickCount()};
  if (g_config.log_attacks && g_logged.First(LogOnce::Key(IdOf(attacker), static_cast<int>(target_id), 2))) {
    Log("fighter: 0x%08X missed 0x%08X - Studied Attacks: advantage on its next attack at it", IdOf(attacker),
        target_id);
  }
}
