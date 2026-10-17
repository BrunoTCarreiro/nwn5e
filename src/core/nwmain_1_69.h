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

// int CNWSCreatureStats::GetAttacksPerRound() (unnamed in the community map),
// plain ret. Main-hand attacks from the pre-epic BAB: (BAB + 4) / 5, or
// (BAB + 2) / 3 with monk attack tables, clamped to 1..6. Its only caller is
// CNWSCombatRound::InitializeNumberOfAttacks (0x73C5A0), which stores the count
// at combat round +0x2138 — after the script override (stats +0x412), and not
// for crossbows (base items 6/7) unless the creature has Rapid Reload (feat 411).
// Flurry/Rapid Shot/Haste extra attacks are counted separately (+0x2148).
constexpr uintptr_t kGetAttacksPerRound = 0x00693810;
constexpr uint8_t kGetAttacksPerRoundPrologue[] = {0x56, 0x8b, 0xf1, 0x6a, 0x00, 0xe8, 0xf6, 0x18};

// The 3e iterative penalty inside GetAttackModifierVersus (edi = the attack's
// index in the round, the argument it passes to GetAttack 0x73C0A0):
//   0x6959F1  lea ecx, [edi+edi*2] / 0x6959F4  sub ebx, ecx   ; monk tables: -3 x index
//   0x695A02  lea edx, [edi+edi*4] / 0x695A05  sub ebx, edx   ; otherwise:   -5 x index
// NOPing the subs removes the penalty. The off-hand penalty (0x695A3A) stays.
constexpr uintptr_t kIterativePenaltyMonk = 0x006959F4;
constexpr uint8_t kIterativePenaltyMonkBytes[] = {0x2b, 0xd9};
constexpr uintptr_t kIterativePenalty = 0x00695A05;
constexpr uint8_t kIterativePenaltyBytes[] = {0x2b, 0xda};

// int CNWSCreatureStats::GetDamageRoll(CNWSObject* defender, int bOffHand,
// int bCritical, int bSneakAttack, int bDeathAttack, int bForceMax) 0x69AEB0,
// ret 0x18. On a critical it rolls the weapon dice `multiplier` times
// (GetCriticalHitMultiplier calls inside its dice loops), then adds the flat
// bonuses from 0x697D10 (Strength etc.; returned unmultiplied, kept in ebp)
// in this loop:
//   0x69BEA7  call GetCriticalHitMultiplier   ; returns to 0x69BEAC
//   0x69BEB0  add  [esp+10h], ebp             ; flat bonuses, once per x
//   0x69BEBC  call GetCriticalHitMultiplier   ; returns to 0x69BEC1
// Answering 1 at these two call sites adds the flat bonuses once (2024).
constexpr uintptr_t kFlatBonusLoopCall1 = 0x0069BEA7;
constexpr uint8_t kFlatBonusLoopCall1Bytes[] = {0xe8, 0x54, 0x05, 0x00, 0x00};
constexpr uintptr_t kFlatBonusLoopCall2 = 0x0069BEBC;
constexpr uint8_t kFlatBonusLoopCall2Bytes[] = {0xe8, 0x3f, 0x05, 0x00, 0x00};
constexpr uintptr_t kCallLength = 5;

// Diagnostics ([debug] attack_trace).
// void CNWSCombatRound::InitializeNumberOfAttacks(), no stack args (the
// community plugin hooks it as (CNWSCombatRound*, void*) on 1.69).
constexpr uintptr_t kInitializeNumberOfAttacks = 0x0073C5A0;
constexpr uint8_t kInitializeNumberOfAttacksPrologue[] = {0x53, 0x56, 0x8b, 0xf1, 0x8b, 0x86, 0x5c, 0x21};
// void CNWSCreature::ResolveAttackRoll(CNWSObject* target), one stack arg.
constexpr uintptr_t kResolveAttackRoll = 0x00744390;
constexpr uint8_t kResolveAttackRollPrologue[] = {0x6a, 0xff, 0x68, 0xbb, 0xec, 0x89, 0x00, 0x64};

// CNWSCombatRound fields (InitializeNumberOfAttacks, ResolveMeleeAttack,
// GetAttackModifierVersus).
constexpr uintptr_t kRoundCurrentAttack = 0x2114;    // uint8, index of the attack being resolved
constexpr uintptr_t kRoundMainHandAttacks = 0x2138;  // int32, set from GetAttacksPerRound
constexpr uintptr_t kRoundOffHandAttacks = 0x213C;   // int32 (set by 0x73BF80 — unverified)
constexpr uintptr_t kRoundExtraAttacks = 0x2148;     // int32, Flurry/Rapid Shot/Haste
constexpr uintptr_t kRoundOwner = 0x215C;            // CNWSCreature*
// CNWSCreature -> its combat round (ResolveAttackRoll reads +0xAC8, then +0x2114).
constexpr uintptr_t kCreatureCombatRound = 0xAC8;

// CNWSCreatureStats class slots: count at +0xA1; slot i at i * 0x118 holds the
// class id (classes.2da row) at +0x1B2 and its levels at +0x1B3
// (GetBaseAttackBonus, GetLevel).
constexpr uintptr_t kStatsClassSlotStride = 0x118;
constexpr uintptr_t kStatsClassSlotId = 0x1B2;
constexpr uintptr_t kStatsClassSlotLevels = 0x1B3;

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
// uint16 associate type (0 none, 1 henchman, 2 animal companion, 3 familiar,
// 4 summoned, 5 dominated). GetAssociateId(type, nTh) 0x68D970 walks the
// associate list at +0xB34 and compares each associate's +0xB38 with `type`;
// ResolveAttack reads it too. Master object id at +0xB30.
constexpr uintptr_t kCreatureAssociateType = 0xB38;

}  // namespace nwmain
