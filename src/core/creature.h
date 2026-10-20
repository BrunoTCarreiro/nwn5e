#pragma once

// What the rule hooks need to know about a creature, read from engine memory
// (offsets in nwmain_1_69.h), and the attack-bonus conversion they share.

#include <stdint.h>

#include <vector>

#include "rules/nwn_conversion.h"

template <typename T>
T Field(const void* object, uintptr_t offset) {
  return *reinterpret_cast<const T*>(static_cast<const uint8_t*>(object) + offset);
}

template <typename T>
void SetField(void* object, uintptr_t offset, T value) {
  *reinterpret_cast<T*>(static_cast<uint8_t*>(object) + offset) = value;
}

struct CreatureFacts {
  bool is_player;
  int associate_type;
  bool character;  // player character or henchman: character rules (decision 11)
  int level;
  float challenge_rating;
  bool plausible;  // characters always; others when the CR read is plausible
};

// Object id of the creature owning these stats (0 when unknown).
uint32_t ObjectIdOf(void* stats);

// Reads the facts; logs (once per creature) a challenge rating that is not plausible.
CreatureFacts ReadCreatureFacts(void* stats);

// "PC", "henchman" or "creature", for the log.
const char* KindOf(const CreatureFacts& facts);

// Class slots (classes.2da id + levels), at most 3.
std::vector<rules::NwnClassSlot> ReadClassSlots(void* stats);

// Levels in a 2024 class, prestige classes counted as their parent (decision 10).
int ClassLevel2024(void* stats, rules::CharacterClass character_class);

// An engine attack bonus built on GetBaseAttackBonus(0), converted to 2024:
// minus the 3e BAB, plus the proficiency bonus. Returns the engine value when
// the creature's facts are not plausible.
int ConvertEngineAttackBonus(void* stats, int engine_value, const CreatureFacts& facts, int* base_attack_bonus,
                             int* proficiency_bonus);
