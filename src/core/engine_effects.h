#pragma once

// Engine effects applied the way NWScript's Effect* + ApplyEffectToObject do
// it (nwmain_1_69.h "Engine effects"). `target` is a CNWSCreature (its
// CNWSObject base); `creator` is the object id credited (combat log). Each is
// queued as EVENT_APPLY_EFFECT `delay_ms` from now, like a hit's damage.

#include <stdint.h>

namespace engine {

// Instant damage of one type (`damage_type_flag`: 1 bludgeoning, 2 piercing,
// 4 slashing, ...), normal damage power.
void ApplyDamage(void* target, uint32_t creator, int amount, int damage_type_flag, uint32_t delay_ms);

// Knocked down (NWN's knockdown, the 2024 Prone stand-in) for `seconds`.
void ApplyKnockdown(void* target, uint32_t creator, float seconds, uint32_t delay_ms);

// Movement speed decreased by `percent` for `seconds`.
void ApplyMovementSpeedDecrease(void* target, uint32_t creator, int percent, float seconds, uint32_t delay_ms);

// Only an icon (effecticons.2da row) for `seconds` (CN5 fear).
void ApplyIcon(void* target, uint32_t creator, int icon, float seconds, uint32_t delay_ms);

// D4 Push: `target` moved straight away from `attacker` up to `max_metres`
// (in 0.5 m steps down to 0.5 m) onto walkable ground, its body (0.5 m) clear
// of walls; then, if its combat round can take it, the knockdown fall plays
// for `fall_ms` as a visual only (the round pauses and is shortened by as
// much, as the engine's hit flinch does) and the engine stands it back up.
// Not for the dead or creatures larger than Large. Immediate: call it on the
// server thread, from a hook.
struct PushOutcome {
  float metres = 0.0f;
  bool fell = false;
  const char* refused = nullptr;  // why it was not moved
};
PushOutcome PushAway(void* target, void* attacker, float max_metres, int fall_ms);

}  // namespace engine
