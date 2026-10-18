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

}  // namespace engine
