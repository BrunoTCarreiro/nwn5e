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

// D4 Push.
struct Vector3 {
  float x, y, z;
};
// Where `target` can be pushed straight away from `attacker`: up to
// `max_metres` in 0.5 m steps down to 0.5 m, along a line (plus 0.5 m of
// body behind the spot) that crosses only walkable faces and no placeable
// (the area's static-object test). Not for the dead, creatures larger than
// Large, or another area.
struct PushPlan {
  Vector3 from = {};
  Vector3 to = {};
  Vector3 away = {};  // unit, on the ground plane
  float metres = 0.0f;
  uint32_t blocker = 0x7F000000;  // a placeable that cut the push short
  const char* refused = nullptr;  // why it can't be pushed at all
};
PushPlan PlanPush(void* target, void* attacker, float max_metres);
// The knockdown fall as a visual only: the round pauses `ms` (one paused by
// the same attacker, its hit flinch, is extended) and is shortened by as
// much, so no attack is lost; the AI update stands it up. False when its
// round can't take it or it is already lying down.
bool PlayPushFall(void* target, uint32_t attacker_id, const Vector3& away, int ms);
// Puts `creature` at (x, y) on the area's ground (SetPosition + trigger
// update, as the engine's jump does); false off the walkmesh.
bool PlaceOnGround(void* creature, float x, float y);

}  // namespace engine
