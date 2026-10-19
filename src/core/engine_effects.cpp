// Engine effects, built like the NWScript commands build them (EffectDamage
// 0x6EC4B0, EffectKnockdown in 0x6F0610, EffectMovementSpeedDecrease
// 0x6F1C50) and applied like ApplyEffectToObject 0x6F50A0 does. Effects come
// from the engine's own operator new, since the engine deletes them.

#include "engine_effects.h"

#include <math.h>

#include "creature.h"
#include "engine_queries.h"
#include "nwmain_1_69.h"

namespace {

using EngineNewFn = void*(__cdecl*)(size_t size);
using EffectCtorFn = void*(__fastcall*)(void* effect, void* edx, int create_new_id);
using SetNumIntegersFn = void(__fastcall*)(void* effect, void* edx, int count);
using SetIntegerFn = void(__fastcall*)(void* effect, void* edx, int index, int value);
using SetCreatorFn = void(__fastcall*)(void* effect, void* edx, uint32_t creator);
using ApplyEffectFn = void(__fastcall*)(void* object, void* edx, void* effect, int loading_game, int instant);
using GetAiMasterFn = void*(__fastcall*)(void* app, void* edx);
using AddEventFn = int(__fastcall*)(void* ai_master, void* edx, uint32_t days, uint32_t ms, uint32_t caller,
                                   uint32_t target, uint32_t event, void* data);

const auto g_engine_new = reinterpret_cast<EngineNewFn>(nwmain::kEngineOperatorNew);
const auto g_effect_ctor = reinterpret_cast<EffectCtorFn>(nwmain::kGameEffectCtor);
const auto g_set_num_integers = reinterpret_cast<SetNumIntegersFn>(nwmain::kGameEffectSetNumIntegers);
const auto g_set_integer = reinterpret_cast<SetIntegerFn>(nwmain::kGameEffectSetInteger);
const auto g_set_creator = reinterpret_cast<SetCreatorFn>(nwmain::kGameEffectSetCreator);
const auto g_apply_effect = reinterpret_cast<ApplyEffectFn>(nwmain::kObjectApplyEffect);
const auto g_get_ai_master = reinterpret_cast<GetAiMasterFn>(nwmain::kGetServerAIMaster);
const auto g_add_event = reinterpret_cast<AddEventFn>(nwmain::kAddEventDeltaTime);

using Vector = engine::Vector3;
using StaticLineFn = int(__fastcall*)(void* area, void* edx, float x1, float y1, float x2, float y2, float z1, float z2,
                                     float radius, uint32_t exclude, uint32_t* blocker, int flag, int a11, int a12);
const auto g_static_objects_on_line = reinterpret_cast<StaticLineFn>(nwmain::kAreaStaticObjectsOnLine);
using RoundCheckFn = int(__fastcall*)(void* round, void* edx, uint32_t attacker, int ms);
using RoundPausedFn = void(__fastcall*)(void* round, void* edx, int paused, uint32_t attacker);
using RoundTimeFn = void(__fastcall*)(void* round, void* edx, int ms, int flag);
using SetAnimationFn = void(__fastcall*)(void* creature, void* edx, int animation);
using ComputeHeightFn = float(__fastcall*)(void* area, void* edx, Vector position);
using WalkableLineFn = int(__fastcall*)(void* area, void* edx, float x1, float y1, float x2, float y2, float tolerance);
using SetPositionFn = void(__fastcall*)(void* object, void* edx, Vector position, int force);
using UpdateSubAreasFn = void(__fastcall*)(void* creature, void* edx, Vector position, uint32_t area_id);

const auto g_action_response_possible = reinterpret_cast<RoundCheckFn>(nwmain::kRoundGetActionResponsePossible);
const auto g_set_round_paused = reinterpret_cast<RoundPausedFn>(nwmain::kRoundSetRoundPaused);
const auto g_set_pause_timer = reinterpret_cast<RoundTimeFn>(nwmain::kRoundSetPauseTimer);
const auto g_decrement_round_length = reinterpret_cast<RoundTimeFn>(nwmain::kRoundDecrementRoundLength);
const auto g_compute_height = reinterpret_cast<ComputeHeightFn>(nwmain::kAreaComputeHeight);
const auto g_walkable_line = reinterpret_cast<WalkableLineFn>(nwmain::kAreaNoNonWalkPolysOnLine);
const auto g_set_position = reinterpret_cast<SetPositionFn>(nwmain::kObjectSetPosition);
const auto g_update_sub_areas = reinterpret_cast<UpdateSubAreasFn>(nwmain::kCreatureUpdateSubAreas);

bool LyingDown(int animation) {
  return animation == 0x05 || animation == 0x07 || animation == 0x20 || animation == 0x21 || animation == 0x24;
}

}  // namespace

namespace engine {

FallResult PlayPushFall(void* target, uint32_t attacker_id, const Vector3& away, int ms, uint32_t* paused_by) {
  *paused_by = nwmain::kObjectInvalid;
  if (LyingDown(Field<int32_t>(target, nwmain::kCreatureAnimation))) return FallResult::kLyingDown;
  void* round = Field<void*>(target, nwmain::kCreatureCombatRound);
  bool paused = false;
  if (round) {
    if (Field<int32_t>(round, nwmain::kRoundPaused) == 1) *paused_by = Field<uint32_t>(round, nwmain::kRoundPausedBy);
    if (g_action_response_possible(round, nullptr, attacker_id, ms) != 0) {
      if (*paused_by == attacker_id) {
        const int running = Field<int32_t>(round, nwmain::kRoundPauseTimer);
        if (running < ms) {
          g_decrement_round_length(round, nullptr, ms - running, 0);
          SetField<int32_t>(round, nwmain::kRoundPauseTimer, ms);
        }
      } else {
        g_set_round_paused(round, nullptr, 1, attacker_id);
        g_set_pause_timer(round, nullptr, ms, 0);
        g_decrement_round_length(round, nullptr, ms, 0);
      }
      paused = true;
    }
  }
  // As the knockdown picks it: facing the attacker (away from the push) -> 7.
  const Vector facing = Field<Vector>(target, nwmain::kObjectFacing);
  const float length = sqrtf(facing.x * facing.x + facing.y * facing.y);
  const float dot = length > 0.0f ? (-away.x * facing.x - away.y * facing.y) / length : 0.0f;
  const int animation = dot >= nwmain::kFallFacingDot ? nwmain::kAnimationFallFacing : nwmain::kAnimationFallTurned;
  const auto set_animation =
      (*reinterpret_cast<SetAnimationFn* const*>(target))[nwmain::kVtableSetAnimation / sizeof(void*)];
  set_animation(target, nullptr, animation);
  return paused ? FallResult::kFell : FallResult::kFellUnpaused;
}

}  // namespace engine

namespace {

// A new effect of `type`, extraordinary (not dispellable, like a weapon's
// own effect), with a duration type (0 instant, 1 temporary).
void* NewEffect(uint16_t type, uint16_t duration_type, float seconds, uint32_t creator) {
  void* memory = g_engine_new(nwmain::kGameEffectSize);
  if (!memory) return nullptr;
  void* effect = g_effect_ctor(memory, nullptr, 1);
  SetField<uint16_t>(effect, nwmain::kEffectType, type);
  const uint16_t flags = Field<uint16_t>(effect, nwmain::kEffectFlags);
  SetField<uint16_t>(effect, nwmain::kEffectFlags,
                     static_cast<uint16_t>((flags & ~0x1F) | nwmain::kEffectSubtypeExtraordinary | duration_type));
  if (duration_type == nwmain::kEffectDurationTemporary) SetField<float>(effect, nwmain::kEffectDuration, seconds);
  g_set_creator(effect, nullptr, creator);
  return effect;
}

// Queued for the target like a hit's damage (applied directly if there is no
// AI master yet).
void Apply(void* target, void* effect, uint32_t creator, uint32_t delay_ms) {
  if (!target || !effect) return;
  void* app = engine::ServerExoApp();
  void* ai_master = app ? g_get_ai_master(app, nullptr) : nullptr;
  if (!ai_master) {
    g_apply_effect(target, nullptr, effect, 0, 1);
    return;
  }
  g_add_event(ai_master, nullptr, 0, delay_ms, creator, Field<uint32_t>(target, nwmain::kCreatureObjectId),
              nwmain::kEventApplyEffect, effect);
}

int BitIndex(int flag) {
  int index = 0;
  while (flag > 1) {
    flag >>= 1;
    ++index;
  }
  return index;
}

}  // namespace

namespace engine {

void ApplyDamage(void* target, uint32_t creator, int amount, int damage_type_flag, uint32_t delay_ms) {
  if (amount <= 0) return;
  void* effect = NewEffect(nwmain::kEffectTypeDamage, nwmain::kEffectDurationInstant, 0.0f, creator);
  if (!effect) return;
  g_set_num_integers(effect, nullptr, 20);
  for (int i = 0; i < 13; ++i) g_set_integer(effect, nullptr, i, -1);
  g_set_integer(effect, nullptr, BitIndex(damage_type_flag), amount);
  g_set_integer(effect, nullptr, 14, 1000);
  g_set_integer(effect, nullptr, 15, damage_type_flag);
  g_set_integer(effect, nullptr, 16, 0);  // DAMAGE_POWER_NORMAL
  Apply(target, effect, creator, delay_ms);
}

void ApplyKnockdown(void* target, uint32_t creator, float seconds, uint32_t delay_ms) {
  Apply(target, NewEffect(nwmain::kEffectTypeKnockdown, nwmain::kEffectDurationTemporary, seconds, creator), creator,
        delay_ms);
}

void ApplyMovementSpeedDecrease(void* target, uint32_t creator, int percent, float seconds, uint32_t delay_ms) {
  void* effect =
      NewEffect(nwmain::kEffectTypeMovementSpeedDecrease, nwmain::kEffectDurationTemporary, seconds, creator);
  if (!effect) return;
  g_set_integer(effect, nullptr, 0, percent);
  Apply(target, effect, creator, delay_ms);
}

void ApplyCantMove(void* target, uint32_t creator, float seconds, uint32_t delay_ms) {
  void* effect = NewEffect(nwmain::kEffectTypeSetAiState, nwmain::kEffectDurationTemporary, seconds, creator);
  if (!effect) return;
  g_set_integer(effect, nullptr, 0, static_cast<int>(~static_cast<uint32_t>(nwmain::kAiStateCanMove)));
  Apply(target, effect, creator, delay_ms);
}

void ApplyIcon(void* target, uint32_t creator, int icon, float seconds, uint32_t delay_ms) {
  void* effect = NewEffect(nwmain::kEffectTypeIcon, nwmain::kEffectDurationTemporary, seconds, creator);
  if (!effect) return;
  g_set_integer(effect, nullptr, 0, icon);
  Apply(target, effect, creator, delay_ms);
}

PushPlan PlanPush(void* target, void* attacker, float max_metres) {
  PushPlan plan;
  if (!target || !attacker) {
    plan.refused = "no creature";
    return plan;
  }
  if (IsDeadOrDying(target)) {
    plan.refused = "dead";
    return plan;
  }
  if (Field<uint32_t>(target, nwmain::kCreatureSize) > nwmain::kCreatureSizeLarge) {
    plan.refused = "larger than Large";
    return plan;
  }
  void* area = GetArea(target);
  if (!area || GetArea(attacker) != area) {
    plan.refused = "not in the attacker's area";
    return plan;
  }
  plan.from = Field<Vector>(target, nwmain::kObjectPosition);
  const Vector attacker_at = Field<Vector>(attacker, nwmain::kObjectPosition);
  Vector away = {plan.from.x - attacker_at.x, plan.from.y - attacker_at.y, 0.0f};
  float length = sqrtf(away.x * away.x + away.y * away.y);
  if (length < 0.01f) {  // on top of each other: along the attacker's facing
    away = Field<Vector>(attacker, nwmain::kObjectFacing);
    away.z = 0.0f;
    length = sqrtf(away.x * away.x + away.y * away.y);
    if (length < 0.01f) {
      plan.refused = "no direction";
      return plan;
    }
  }
  plan.away = {away.x / length, away.y / length, 0.0f};
  const void* pathfind = Field<void*>(target, nwmain::kCreaturePathfind);
  float radius = pathfind ? Field<float>(pathfind, nwmain::kPathfindRadius) : 0.5f;
  if (!(radius > 0.05f && radius < 3.0f)) radius = 0.5f;
  constexpr float kBody = 0.5f;  // walkable line kept behind the new spot
  for (float metres = max_metres; metres >= 0.49f; metres -= 0.5f) {
    const Vector to = {plan.from.x + plan.away.x * metres, plan.from.y + plan.away.y * metres, plan.from.z};
    const float end_x = to.x + plan.away.x * kBody;
    const float end_y = to.y + plan.away.y * kBody;
    if (g_compute_height(area, nullptr, to) <= nwmain::kAreaOffWalkmeshHeight + 0.01f) continue;
    if (g_walkable_line(area, nullptr, plan.from.x, plan.from.y, end_x, end_y, -0.1f) != 1) continue;
    uint32_t blocker = nwmain::kObjectInvalid;
    if (g_static_objects_on_line(area, nullptr, plan.from.x, plan.from.y, end_x, end_y, plan.from.z - 0.1f,
                                 plan.from.z + 1.8f, radius, nwmain::kObjectInvalid, &blocker, 1, 0, 0) != 1) {
      if (plan.blocker == nwmain::kObjectInvalid) plan.blocker = blocker;
      continue;
    }
    plan.to = to;
    plan.metres = metres;
    return plan;
  }
  plan.refused = plan.blocker != nwmain::kObjectInvalid ? "a placeable right behind it" : "no walkable ground behind it";
  return plan;
}

bool PlaceOnGround(void* creature, float x, float y) {
  void* area = creature ? GetArea(creature) : nullptr;
  if (!area) return false;
  Vector at = {x, y, Field<Vector>(creature, nwmain::kObjectPosition).z};
  const float height = g_compute_height(area, nullptr, at);
  if (height <= nwmain::kAreaOffWalkmeshHeight + 0.01f) return false;
  at.z = height;
  g_set_position(creature, nullptr, at, 0);
  g_update_sub_areas(creature, nullptr, at, Field<uint32_t>(area, nwmain::kAreaId));
  return true;
}

}  // namespace engine
