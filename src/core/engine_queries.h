#pragma once

// Engine reads shared by the rules: object lookups, the area's objects,
// reactions, visibility and distances. Addresses, calling conventions and
// the evidence for each are in nwmain_1_69.h.

#include <stdint.h>

#include "creature.h"
#include "nwmain_1_69.h"

namespace engine {

void* ServerExoApp();                            // [[0x92DC50]+4], the CServerExoApp
void* GetCreature(uint32_t id);                  // CNWSCreature*, or null (not a creature)
void* GetArea(void* object);                     // 0x63ECC0
bool IsEnemy(void* creature, uint32_t other_id);  // reaction 0x731520 == 2
bool IsDeadOrDying(void* creature);              // 0x640750 / 0x6407E0
bool Sees(void* creature, uint32_t other_id);    // visible list 0x687BF0, bit 0
int FactionOf(const void* creature);             // stats +0x90, -1 if unknown
float MaxAttackRange(void* creature, uint32_t target, int ranged_arg);  // 0x64B5D0

// Script local variables on an object (saved with the game): CNWSScriptVar
// Table::GetInt 0x74E7B0 / SetInt 0x74E970 on object +0xD8. 0 when unset.
int GetLocalInt(void* object, const char* name);
void SetLocalInt(void* object, const char* name, int value);

// Does the engine add 1.5 x Strength to the creature's current attack (a
// melee attack with a right-hand weapon one size above the creature that is
// not a double weapon; damage bonus 0x697D10)?
bool CurrentAttackGetsTwoHandedStrength(void* creature);

// Edge-to-edge distance, as GetNearestEnemy measures it.
float EdgeDistance(const void* a, const void* b);
// Centre-to-centre distance squared, as GetAttackModifierVersus compares it.
float CentreDistanceSquared(const void* a, const void* b);

// Calls fn(id, creature) for each creature in `object`'s area other than
// `object` itself.
template <typename Fn>
void ForEachOtherCreatureInArea(void* object, Fn fn) {
  const void* area = GetArea(object);
  if (!area) return;
  const uint32_t* ids = Field<const uint32_t*>(area, nwmain::kAreaObjectIds);
  const int count = Field<int32_t>(area, nwmain::kAreaObjectCount);
  if (!ids) return;
  const uint32_t self = Field<uint32_t>(object, nwmain::kCreatureObjectId);
  for (int i = 0; i < count; ++i) {
    if (ids[i] == self) continue;
    if (void* creature = GetCreature(ids[i])) fn(ids[i], creature);
  }
}

}  // namespace engine
