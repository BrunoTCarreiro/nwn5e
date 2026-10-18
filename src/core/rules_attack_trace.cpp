// [debug] attack_trace=1 — diagnostics, not a rule. For player characters it
// logs how each combat round is set up (main-hand, off-hand and extra attack
// counts) and every attack roll actually resolved, with its index in the
// round. Used to see whether the Extra Attack count reaches the round.
// Stops after kTraceLimit lines. The attack-roll line is called from the
// ResolveAttackRoll hook in rules_advantage.cpp (one hook per function).

#include <stdint.h>

#include "config.h"
#include "hooks.h"
#include "log.h"
#include "nwmain_1_69.h"

namespace {

using InitializeNumberOfAttacksFn = void(__fastcall*)(void* round, void* edx);

InitializeNumberOfAttacksFn g_initialize_number_of_attacks = nullptr;

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

}  // namespace

void TraceAttackRoll(const void* creature) {
  if (!IsPlayerCreature(creature) || !TraceSlotLeft()) return;
  const void* round = Field<void*>(creature, nwmain::kCreatureCombatRound);
  if (!round) return;
  Log("trace: attack roll by PC 0x%08X: attack index %d (round: main %d, off-hand %d, extra %d)",
      Field<uint32_t>(creature, nwmain::kCreatureObjectId), Field<uint8_t>(round, nwmain::kRoundCurrentAttack),
      Field<int32_t>(round, nwmain::kRoundMainHandAttacks), Field<int32_t>(round, nwmain::kRoundOffHandAttacks),
      Field<int32_t>(round, nwmain::kRoundExtraAttacks));
}

// [debug] combat_breakdown: the engine's own per-term attack breakdown in the
// combat log (nwmain_1_69.h). Called at each attack roll, after the client's
// flag message can no longer reset it.
void KeepCombatBreakdownOn() {
  if (!g_config.debug_combat_breakdown) return;
  *reinterpret_cast<volatile uint32_t*>(nwmain::kCombatDebugFlag) = 1;
  *reinterpret_cast<volatile uint32_t*>(nwmain::kCombatDebugFlag2) = 1;
}

void InstallAttackTrace() {
  if (g_config.debug_combat_breakdown) {
    static const uint8_t kNops[6] = {0x90, 0x90, 0x90, 0x90, 0x90, 0x90};
    const bool kept = PatchBytes("combat breakdown: client flag message ignored (1/2)", nwmain::kCombatDebugStore1,
                                 nwmain::kCombatDebugStore1Bytes, kNops, sizeof(kNops)) &&
                      PatchBytes("combat breakdown: client flag message ignored (2/2)", nwmain::kCombatDebugStore2,
                                 nwmain::kCombatDebugStore2Bytes, kNops, sizeof(kNops));
    if (!kept) g_config.debug_combat_breakdown = false;
    if (kept) Log("config: [debug] combat_breakdown=1 — the engine's attack breakdown goes to the combat log");
  }
  if (!g_config.debug_attack_trace) return;
  InstallHook({"CNWSCombatRound::InitializeNumberOfAttacks (trace)", nwmain::kInitializeNumberOfAttacks,
               nwmain::kInitializeNumberOfAttacksPrologue, sizeof(nwmain::kInitializeNumberOfAttacksPrologue),
               reinterpret_cast<void*>(&InitializeNumberOfAttacks),
               reinterpret_cast<void**>(&g_initialize_number_of_attacks)});
}
