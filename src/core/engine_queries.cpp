#include "engine_queries.h"

#include <math.h>
#include <string.h>

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

namespace {

struct VarName {
  const char* text;
  uint32_t length;  // including the null
};
using GetIntFn = int(__fastcall*)(void* table, void* edx, VarName* name);
using SetIntFn = void(__fastcall*)(void* table, void* edx, VarName* name, int value, int flag);
const auto g_var_get_int = reinterpret_cast<GetIntFn>(nwmain::kVarTableGetInt);
const auto g_var_set_int = reinterpret_cast<SetIntFn>(nwmain::kVarTableSetInt);

void* VarTable(void* object) { return static_cast<uint8_t*>(object) + nwmain::kObjectVarTable; }

using GetAttackFn = void*(__fastcall*)(void* round, void* edx, int index);
using GetItemFn = void*(__fastcall*)(void* owner, void* edx, int which);
using GetBaseItemFn = void*(__fastcall*)(void* base_items, void* edx, int row);
const auto g_get_attack_q = reinterpret_cast<GetAttackFn>(nwmain::kGetAttack);
const auto g_get_item_in_slot_q = reinterpret_cast<GetItemFn>(nwmain::kGetItemInSlot);
const auto g_get_base_item_q = reinterpret_cast<GetBaseItemFn>(nwmain::kGetBaseItem);

}  // namespace

int GetLocalInt(void* object, const char* name) {
  VarName text = {name, static_cast<uint32_t>(strlen(name) + 1)};
  return object ? g_var_get_int(VarTable(object), nullptr, &text) : 0;
}

void SetLocalInt(void* object, const char* name, int value) {
  VarName text = {name, static_cast<uint32_t>(strlen(name) + 1)};
  if (object) g_var_set_int(VarTable(object), nullptr, &text, value, 0);
}

int EffectInteger(const void* effect, int index) {
  if (!effect || index < 0 || index >= Field<int32_t>(effect, nwmain::kEffectIntegerCount)) return 0;
  const int32_t* integers = Field<const int32_t*>(effect, nwmain::kEffectIntegers);
  return integers ? integers[index] : 0;
}

bool HasEffectType(const void* object, uint16_t type) {
  bool found = false;
  ForEachEffectOfType(object, type, [&](const void*) { found = true; });
  return found;
}

bool HasState(const void* object, int state) {
  bool found = false;
  ForEachEffectOfType(object, nwmain::kEffectTypeSetState,
                      [&](const void* effect) { found = found || EffectInteger(effect, 0) == state; });
  return found;
}

bool CurrentAttackGetsTwoHandedStrength(void* creature) {
  if (!creature) return false;
  void* round = Field<void*>(creature, nwmain::kCreatureCombatRound);
  const void* attack =
      round ? g_get_attack_q(round, nullptr, Field<uint8_t>(round, nwmain::kRoundCurrentAttack)) : nullptr;
  if (!attack || Field<int32_t>(attack, nwmain::kAttackDataRanged) != 0) return false;
  void* inventory = Field<void*>(creature, nwmain::kCreatureInventory);
  void* item = inventory ? g_get_item_in_slot_q(inventory, nullptr, nwmain::kSlotRightHand) : nullptr;
  if (!item) return false;
  void* rules_object = *reinterpret_cast<void**>(nwmain::kRulesObject);
  void* base_items = rules_object ? Field<void*>(rules_object, nwmain::kRulesBaseItems) : nullptr;
  void* base_item =
      base_items ? g_get_base_item_q(base_items, nullptr, Field<int32_t>(item, nwmain::kItemBaseItem)) : nullptr;
  if (!base_item || Field<uint8_t>(base_item, nwmain::kBaseItemWield) == nwmain::kBaseItemWieldDouble) return false;
  return static_cast<int>(Field<uint8_t>(base_item, nwmain::kBaseItemSize)) -
             static_cast<int>(Field<uint32_t>(creature, nwmain::kCreatureSize)) == 1;
}

}  // namespace engine
