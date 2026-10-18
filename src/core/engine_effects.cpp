// Engine effects, built like the NWScript commands build them (EffectDamage
// 0x6EC4B0, EffectKnockdown in 0x6F0610, EffectMovementSpeedDecrease
// 0x6F1C50) and applied like ApplyEffectToObject 0x6F50A0 does. Effects come
// from the engine's own operator new, since the engine deletes them.

#include "engine_effects.h"

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

}  // namespace engine
