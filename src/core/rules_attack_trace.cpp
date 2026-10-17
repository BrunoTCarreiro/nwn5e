// [debug] attack_trace=1 — diagnostics, not a rule. For player characters it
// logs how each combat round is set up (main-hand, off-hand and extra attack
// counts) and every attack roll actually resolved, with its index in the
// round. Used to see whether the Extra Attack count reaches the round.
// Stops after kTraceLimit lines.

#include <stdint.h>

#include "config.h"
#include "hooks.h"
#include "log.h"
#include "nwmain_1_69.h"

namespace {

using InitializeNumberOfAttacksFn = void(__fastcall*)(void* round, void* edx);
using ResolveAttackRollFn = void(__fastcall*)(void* creature, void* edx, void* target);

InitializeNumberOfAttacksFn g_initialize_number_of_attacks = nullptr;
ResolveAttackRollFn g_resolve_attack_roll = nullptr;

constexpr int kTraceLimit = 300;
int g_trace_lines = 0;

template <typename T>
T Field(const void* object, uintptr_t offset) {
  return *reinterpret_cast<const T*>(static_cast<const uint8_t*>(object) + offset);
}

bool IsPlayerCreature(const void* creature) {
  const void* stats = Field<void*>(creature, nwmain::kCreatureStats);
  return stats && Field<uint32_t>(stats, nwmain::kStatsIsPlayer) != 0;
}

bool TraceSlotLeft() { return g_trace_lines < kTraceLimit && ++g_trace_lines; }

void __fastcall InitializeNumberOfAttacks(void* round, void* edx) {
  g_initialize_number_of_attacks(round, edx);
  const void* creature = Field<void*>(round, nwmain::kRoundOwner);
  if (!creature || !IsPlayerCreature(creature) || !TraceSlotLeft()) return;
  Log("trace: round set up for PC 0x%08X: main %d, off-hand %d, extra %d",
      Field<uint32_t>(creature, nwmain::kCreatureObjectId), Field<int32_t>(round, nwmain::kRoundMainHandAttacks),
      Field<int32_t>(round, nwmain::kRoundOffHandAttacks), Field<int32_t>(round, nwmain::kRoundExtraAttacks));
}

void __fastcall ResolveAttackRoll(void* creature, void* edx, void* target) {
  if (IsPlayerCreature(creature) && TraceSlotLeft()) {
    const void* round = Field<void*>(creature, nwmain::kCreatureCombatRound);
    if (round) {
      Log("trace: attack roll by PC 0x%08X: attack index %d (round: main %d, off-hand %d, extra %d)",
          Field<uint32_t>(creature, nwmain::kCreatureObjectId), Field<uint8_t>(round, nwmain::kRoundCurrentAttack),
          Field<int32_t>(round, nwmain::kRoundMainHandAttacks), Field<int32_t>(round, nwmain::kRoundOffHandAttacks),
          Field<int32_t>(round, nwmain::kRoundExtraAttacks));
    }
  }
  g_resolve_attack_roll(creature, edx, target);
}

}  // namespace

void InstallAttackTrace() {
  if (!g_config.debug_attack_trace) return;
  InstallHook({"CNWSCombatRound::InitializeNumberOfAttacks (trace)", nwmain::kInitializeNumberOfAttacks,
               nwmain::kInitializeNumberOfAttacksPrologue, sizeof(nwmain::kInitializeNumberOfAttacksPrologue),
               reinterpret_cast<void*>(&InitializeNumberOfAttacks),
               reinterpret_cast<void**>(&g_initialize_number_of_attacks)});
  InstallHook({"CNWSCreature::ResolveAttackRoll (trace)", nwmain::kResolveAttackRoll,
               nwmain::kResolveAttackRollPrologue, sizeof(nwmain::kResolveAttackRollPrologue),
               reinterpret_cast<void*>(&ResolveAttackRoll), reinterpret_cast<void**>(&g_resolve_attack_roll)});
}
