#include "engine_queries.h"

#include <math.h>

namespace engine {
namespace {

using GetCreatureFn = void*(__fastcall*)(void* server_internal, void* edx, uint32_t id);
using GetAreaFn = void*(__fastcall*)(void* object, void* edx);
using GetReactionToFn = uint8_t(__fastcall*)(void* creature, void* edx, uint32_t id);
using CreatureCheckFn = int(__fastcall*)(void* creature, void* edx);
using GetVisibleListElementFn = void*(__fastcall*)(void* creature, void* edx, uint32_t id);
using MaxAttackRangeFn = float(__fastcall*)(void* creature, void* edx, uint32_t target, int a, int b);

const auto g_get_creature = reinterpret_cast<GetCreatureFn>(nwmain::kGetCreatureByGameObjectId);
const auto g_get_area = reinterpret_cast<GetAreaFn>(nwmain::kGetArea);
const auto g_get_reaction_to = reinterpret_cast<GetReactionToFn>(nwmain::kGetReactionTo);
const auto g_get_dead = reinterpret_cast<CreatureCheckFn>(nwmain::kGetDead);
const auto g_get_dying = reinterpret_cast<CreatureCheckFn>(nwmain::kGetDying);
const auto g_get_visible_list_element = reinterpret_cast<GetVisibleListElementFn>(nwmain::kGetVisibleListElement);
const auto g_max_attack_range = reinterpret_cast<MaxAttackRangeFn>(nwmain::kMaxAttackRange);

float PersonalSpace(const void* creature) {
  const void* info = Field<void*>(creature, nwmain::kCreaturePersonalSpaceInfo);
  return info ? Field<float>(info, nwmain::kPersonalSpace) : 0.0f;
}

}  // namespace

void* ServerExoApp() {
  return Field<void*>(*reinterpret_cast<void**>(nwmain::kAppManager), 4);
}

void* GetCreature(uint32_t id) {
  void* app = ServerExoApp();
  return app ? g_get_creature(app, nullptr, id) : nullptr;
}

void* GetArea(void* object) { return g_get_area(object, nullptr); }

bool IsEnemy(void* creature, uint32_t other_id) {
  return g_get_reaction_to(creature, nullptr, other_id) == nwmain::kReactionEnemy;
}

bool IsDeadOrDying(void* creature) { return g_get_dead(creature, nullptr) || g_get_dying(creature, nullptr); }

bool Sees(void* creature, uint32_t other_id) {
  const void* element = g_get_visible_list_element(creature, nullptr, other_id);
  return element && (Field<uint8_t>(element, nwmain::kVisibleElementFlags) & 1) != 0;
}

int FactionOf(const void* creature) {
  const void* stats = Field<void*>(creature, nwmain::kCreatureStats);
  return stats ? Field<int32_t>(stats, nwmain::kStatsFaction) : -1;
}

float MaxAttackRange(void* creature, uint32_t target, int ranged_arg) {
  return g_max_attack_range(creature, nullptr, target, 0, ranged_arg);
}

float CentreDistanceSquared(const void* a, const void* b) {
  const float dx = Field<float>(a, nwmain::kObjectPosition) - Field<float>(b, nwmain::kObjectPosition);
  const float dy = Field<float>(a, nwmain::kObjectPosition + 4) - Field<float>(b, nwmain::kObjectPosition + 4);
  const float dz = Field<float>(a, nwmain::kObjectPosition + 8) - Field<float>(b, nwmain::kObjectPosition + 8);
  return dx * dx + dy * dy + dz * dz;
}

float EdgeDistance(const void* a, const void* b) {
  return sqrtf(CentreDistanceSquared(a, b)) - PersonalSpace(a) - PersonalSpace(b);
}

}  // namespace engine
