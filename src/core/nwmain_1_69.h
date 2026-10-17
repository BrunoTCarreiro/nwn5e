#pragma once

// Engine addresses in nwmain.exe 1.69 (GOG Diamond, SHA-256 4d8aa22b…2211c49).
// Provenance: re/maps/nwmain-1.69-bytematch.tsv (names from the community's
// nwserver.exe 1.69 map, located in nwmain.exe by masked byte signatures and
// confirmed by object-file spacing), decompiled with REA + Ghidra 12.1.4.
// Prologue bytes: re/prologue.mjs.

#include <stdint.h>

namespace nwmain {

// int CNWSCreatureStats::GetCriticalHitRoll(int bOffhand)
//   Lowest d20 roll that threatens a critical hit (21 - threat range).
constexpr uintptr_t kGetCriticalHitRoll = 0x0069C290;
constexpr uint8_t kGetCriticalHitRollPrologue[] = {0x83, 0xec, 0x08, 0x83, 0x7c, 0x24, 0x0c, 0x01};

// int CNWSCreatureStats::GetCriticalHitMultiplier(int bOffhand)
constexpr uintptr_t kGetCriticalHitMultiplier = 0x0069C400;
constexpr uint8_t kGetCriticalHitMultiplierPrologue[] = {0x83, 0x7c, 0x24, 0x04, 0x01, 0x53, 0x56, 0x57};

// void CNWSCreature::ResolveAttackRoll(CNWSObject* target) at 0x00744390
// (called per attack from ResolveMeleeAttack 0x742A80 / ResolveRangedAttack
// 0x740480). After a threat it rolls a second d20 into attack data +0x0F and:
//   0x744721  cmp eax, [esp+18h]    ; threat roll + attack modifier vs AC
//   0x744725  jl  0x744875          ; below AC -> ordinary hit (result 1)
// Replacing the jl with NOPs makes every threat a critical (result 3); the
// critical-immunity checks that follow still apply.
constexpr uintptr_t kCritConfirmBranch = 0x00744725;
constexpr uint8_t kCritConfirmBranchBytes[] = {0x0f, 0x8c, 0x4a, 0x01, 0x00, 0x00};

// Right after the confirmation, in the same function:
//   0x74472B  test edi, edi         ; target is not a creature ...
//   0x74472D  jz   0x74487B         ; ... -> critical without immunity checks
//   (falls through into the critical-immunity checks)
//   0x74487B  mov byte [esi+50h], 3 ; attack result = critical
// Debug only: an unconditional jmp 0x74487B skips the immunity checks.
constexpr uintptr_t kCritImmunityCheck = 0x0074472B;
constexpr uint8_t kCritImmunityCheckBytes[] = {0x85, 0xff, 0x0f, 0x84, 0x48, 0x01, 0x00, 0x00};
constexpr uint8_t kCritImmunityCheckSkip[] = {0xe9, 0x4b, 0x01, 0x00, 0x00, 0x90, 0x90, 0x90};

// int CNWSCreatureStats::GetAttackModifierVersus(CNWSCreature* target), ret 4.
// Starts from GetBaseAttackBonus(0) (every call site pushes 0), subtracts the
// 3e iterative penalty (5 x attack index; 3 x for monk tables; 5 x off-hand
// attacks made, combat round +0x2140), then adds everything else.
constexpr uintptr_t kGetAttackModifierVersus = 0x006958E0;
constexpr uint8_t kGetAttackModifierVersusPrologue[] = {0x6a, 0xff, 0x68, 0x28, 0x7e, 0x89, 0x00, 0x64};

// short CNWSCreatureStats::GetArmorClassVersus(CNWSCreature* attacker, int bTouch), ret 8.
// ResolveAttackRoll sign-extends the result from 16 bits.
constexpr uintptr_t kGetArmorClassVersus = 0x006925E0;
constexpr uint8_t kGetArmorClassVersusPrologue[] = {0x6a, 0xff, 0x68, 0x00, 0x7b, 0x89, 0x00, 0x64};

// Called, not hooked.
// int CNWSCreatureStats::GetBaseAttackBonus(int bPreEpicOnly), ret 4: class
// table BAB per class slot, plus the epic bonus when bPreEpicOnly == 0; a
// non-zero override byte at +0x411 replaces the total.
constexpr uintptr_t kGetBaseAttackBonus = 0x00693720;
// uint8 CNWSCreatureStats::GetLevel(int bCountNegativeLevels), ret 4 (result in AL).
constexpr uintptr_t kGetLevel = 0x00692520;

// CNWSCreatureStats fields (offsets verified in code unless noted).
constexpr uintptr_t kStatsCreature = 0x24;     // CNWSCreature* (GetLevel)
constexpr uintptr_t kStatsExperience = 0x70;   // uint32 XP (AddExperience, CanLevelUp)
constexpr uintptr_t kStatsIsPlayer = 0x74;     // uint32, non-zero for player characters (CanLevelUp
                                               // gates the XP check on it)
constexpr uintptr_t kStatsChallengeRating = 0x9C;  // float — NOT read in code yet: ten 4-byte fields
                                                   // after +0x74 in the community layout, ending
                                                   // right before the class count at +0xA1
                                                   // (verified). Guarded by IsPlausibleChallengeRating.
constexpr uintptr_t kStatsClassCount = 0xA1;   // uint8 (GetLevel, GetBaseAttackBonus)

// CNWSCreature fields.
constexpr uintptr_t kCreatureObjectId = 0x04;  // uint32 object id (passed as id in ResolveAttack)
constexpr uintptr_t kCreatureStats = 0xC64;    // CNWSCreatureStats* (ResolveAttackRoll, GetLevel)

}  // namespace nwmain
