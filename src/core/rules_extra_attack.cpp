// 2024 Extra Attack in place of NWN's iterative attacks:
//
// - Player characters and henchmen (decision 11) make the attacks of the 2024
//   Attack action, from their class levels (rules::MainHandAttacksPerRound);
//   prestige levels count as levels of their parent class (decision 10),
//   racial levels do not count.
// - Other creatures (monsters, summons, familiars, animal companions) keep
//   the engine's count, standing in for a Multiattack.
// - No main-hand attack takes the 3e iterative penalty (-5, or -3 with monk
//   attack tables). Off-hand attacks keep theirs until two-weapon fighting is
//   converted (Phase 1d).
//
// The engine's own exceptions still apply because they sit around the hooked
// count: script overrides of the number of attacks, crossbows without Rapid
// Reload (the 2024 Loading property; Rapid Reload stands in for Crossbow
// Expert), Dirty Fighting, and the extra attack of Flurry/Rapid Shot/Haste.

#include <stdint.h>

#include <algorithm>
#include <vector>

#include "config.h"
#include "hooks.h"
#include "log.h"
#include "log_once.h"
#include "nwmain_1_69.h"
#include "rules/nwn_conversion.h"

namespace {

using AttacksPerRoundFn = int(__fastcall*)(void* stats, void* edx);
AttacksPerRoundFn g_get_attacks_per_round = nullptr;

// The engine never computes more than 6 main-hand attacks.
constexpr int kEngineMaxAttacks = 6;

LogOnce g_attacks_logged(2000);

template <typename T>
T Field(const void* object, uintptr_t offset) {
  return *reinterpret_cast<const T*>(static_cast<const uint8_t*>(object) + offset);
}

uint32_t ObjectIdOf(void* stats) {
  const void* creature = Field<void*>(stats, nwmain::kStatsCreature);
  return creature ? Field<uint32_t>(creature, nwmain::kCreatureObjectId) : 0;
}

std::vector<rules::NwnClassSlot> ReadClassSlots(void* stats) {
  std::vector<rules::NwnClassSlot> slots;
  const int count = std::min<int>(Field<uint8_t>(stats, nwmain::kStatsClassCount), 3);
  for (int i = 0; i < count; ++i) {
    const uintptr_t slot = i * nwmain::kStatsClassSlotStride;
    slots.push_back({Field<uint8_t>(stats, slot + nwmain::kStatsClassSlotId),
                     Field<uint8_t>(stats, slot + nwmain::kStatsClassSlotLevels)});
  }
  return slots;
}

int __fastcall GetAttacksPerRound(void* stats, void* edx) {
  const int engine_attacks = g_get_attacks_per_round(stats, edx);
  const bool is_player = Field<uint32_t>(stats, nwmain::kStatsIsPlayer) != 0;
  const void* creature = Field<void*>(stats, nwmain::kStatsCreature);
  const int associate_type = creature ? Field<uint16_t>(creature, nwmain::kCreatureAssociateType) : 0;
  // Player characters and henchmen follow Extra Attack (decision 11).
  if (!rules::UsesCharacterRules(is_player, associate_type)) return engine_attacks;

  int attacks = engine_attacks;
  const char* reason = nullptr;
  if (is_player && g_config.debug_attacks_per_round > 0) {
    attacks = std::min(g_config.debug_attacks_per_round, kEngineMaxAttacks);
    reason = "debug attacks_per_round";
  } else if (g_config.extra_attack) {
    attacks = std::min(rules::MainHandAttacksPerRound(true, ReadClassSlots(stats), engine_attacks), kEngineMaxAttacks);
    reason = "Extra Attack";
  }
  if (reason && g_config.log_attacks &&
      g_attacks_logged.First(LogOnce::Key(ObjectIdOf(stats), engine_attacks, attacks))) {
    Log("attacks per round: object 0x%08X %s: 3e %d -> 5e %d (%s)", ObjectIdOf(stats),
        is_player ? "PC" : "henchman", engine_attacks, attacks, reason);
  }
  return attacks;
}

}  // namespace

void InstallExtraAttackRules() {
  if (!g_config.extra_attack && g_config.debug_attacks_per_round <= 0) return;
  InstallHook({"CNWSCreatureStats::GetAttacksPerRound", nwmain::kGetAttacksPerRound,
               nwmain::kGetAttacksPerRoundPrologue, sizeof(nwmain::kGetAttacksPerRoundPrologue),
               reinterpret_cast<void*>(&GetAttacksPerRound), reinterpret_cast<void**>(&g_get_attacks_per_round)});
  static const uint8_t kNops[] = {0x90, 0x90};
  PatchBytes("GetAttackModifierVersus iterative penalty (monk tables)", nwmain::kIterativePenaltyMonk,
             nwmain::kIterativePenaltyMonkBytes, kNops, sizeof(kNops));
  PatchBytes("GetAttackModifierVersus iterative penalty", nwmain::kIterativePenalty, nwmain::kIterativePenaltyBytes,
             kNops, sizeof(kNops));
}
