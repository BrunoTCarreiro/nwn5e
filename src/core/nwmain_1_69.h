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

// Crits against players (decision B7). Before the immunity checks,
// ResolveAttackRoll asks a difficulty option:
//   0x74473E  call 0x600100(0)   ; -> 0x610D40 -> 0x47E260(0, server
//                                ;    options +0x170 = game difficulty)
//   0x744743  cmp eax, 1
//   0x744746  jnz 0x74477B       ; 75 33
// and when it is 1, the target is player-controlled (+0xAD8 or possessed
// familiar 0x68E510) and the attacker is not, the critical becomes a plain
// hit (result 1) — seen at Normal difficulty. jnz -> jmp keeps the crit.
constexpr uintptr_t kNoCritsOnPlayersBranch = 0x00744746;
constexpr uint8_t kNoCritsOnPlayersBranchBytes[] = {0x75, 0x33};
constexpr uint8_t kNoCritsOnPlayersBranchSkip[] = {0xeb, 0x33};

// The combat log's "Threat Roll": CNWSMessage::SendServerToPlayerCCMessage
// 0x62F850, attack-roll messages (cases 5 and 6), writes the attack data's
// critical-threat flag (+0x48) as a bool and, when it is 1, the threat roll
// (+0x0F, 5 bits); the client prints "Threat Roll" when the bool is set.
// Each case: `mov edx, [edi+48h]` (the bool) -> `xor edx, edx; nop`, and
// `jnz` over the threat-roll write -> `jmp`. Both together, or the client
// would read bits that were not written. The crit itself is the attack
// result (+0x50 = 3), sent separately.
constexpr uintptr_t kCcThreatFlag1 = 0x0062FB87;
constexpr uintptr_t kCcThreatRollSkip1 = 0x0062FB96;
constexpr uintptr_t kCcThreatFlag2 = 0x0062FCA8;
constexpr uintptr_t kCcThreatRollSkip2 = 0x0062FCB7;
constexpr uint8_t kCcThreatFlagBytes[] = {0x8b, 0x57, 0x48};  // mov edx, [edi+48h]
constexpr uint8_t kCcThreatFlagZero[] = {0x33, 0xd2, 0x90};   // xor edx, edx; nop
constexpr uint8_t kCcThreatRollSkipBytes[] = {0x75, 0x0f};    // jnz +0x0F
constexpr uint8_t kCcThreatRollSkipAlways[] = {0xeb, 0x0f};   // jmp +0x0F

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
// "Additional" attacks (GetWeaponAttackType 6/8: attacks of opportunity,
// Haste, Flurry, Rapid Shot, cleave) take -5 x combat round +0x2144 unless they
// are special attacks 0x363/0x364/0x187:
//   0x695AD0  mov ecx, eax / neg ecx / shl ecx, 2 / sub ecx, eax  ; -5 x count
//   0x695AD9  add ebx, ecx                                        ; 03 D9
constexpr uintptr_t kAdditionalAttackPenalty = 0x00695AD9;
constexpr uint8_t kAdditionalAttackPenaltyBytes[] = {0x03, 0xd9};

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

// Effect damage bonuses, CNWSCreature::GetTotalEffectBonus 0x65E7D0
// (decompiled 2026-10-18): each entry is (dice count, die size or constant)
// from a 2DA; on a critical (attack result 3 or 10) it is added
// GetCriticalHitMultiplier() times — the dice rolled again each time (2024:
// right), a constant added again (2024: wrong). Four blocks, three calls each
// (bForceMax product, loop start, loop test). Every call pushes one argument,
// and the entry's dice count is the caller's [esp+0x2C] at the call
// (0 = constant); inside the multiplier hook that is the return-address slot
// + 4 (return address) + 4 (argument) + 0x2C. Install checks each site is a
// call to GetCriticalHitMultiplier.
constexpr uintptr_t kEffectBonusMultiplierCalls[] = {0x0065F84D, 0x0065F8A2, 0x0065F8DD, 0x0065FA4A,
                                                     0x0065FA9F, 0x0065FADA, 0x0065FC3E, 0x0065FC9A,
                                                     0x0065FCDC, 0x0065FE68, 0x0065FEC4, 0x0065FF06};
constexpr uintptr_t kEffectBonusDiceCountFromReturnSlot = 4 + 4 + 0x2C;

// Cleave, in CNWSCreature::ResolvePostMeleeDamage 0x746070 (ret 4): when an
// attack killed its target (attack data +0x40 == 1, not special attacks
// 0x363/0x364) and the round has < 50 attacks (GetTotalAttacks), it asks
//   0x746731  mov ecx, [edi+0C64h]   ; attacker's stats
//   0x746737  push 187h              ; feat 391 Great Cleave
//   0x74673C  call HasFeat           ; E8 EF CE F5 FF
// and on yes takes MaxAttackRange (0x64B5D0) + GetNearestEnemy (0x658C00) and
// AddCleaveAttack (0x73D4B0, target, bGreatCleave=1) — an attack against the
// nearest enemy in reach. Plain Cleave (feat 6) does the same once per round
// (combat round +0x212C). Redirecting this one call moves a round's next
// attack to the nearest enemy when the attacker has attacks left (decision 14).
constexpr uintptr_t kGreatCleaveCheckCall = 0x0074673C;
constexpr uint8_t kGreatCleaveCheckCallBytes[] = {0xe8, 0xef, 0xce, 0xf5, 0xff};
// int CNWSCreatureStats::HasFeat(uint16 feat), ret 4 (community: CNWSCreature__HasFeat).
constexpr uintptr_t kHasFeat = 0x006A3630;
// The Great Cleave branch then does (0x746745..0x746797):
//   range = MaxAttackRange(attacker, 0x7F000000, 0, 1)           ; 0x64B5D0, ret 0xC, float in ST0
//   id    = GetNearestEnemy(attacker, range, killedId, 1, 0)     ; 0x658C00, ret 0x10
//   if (GetCreatureByGameObjectID(id))                           ; 0x5FFAD0 on [[0x92DC50]+4], ret 4
//     round+0x2134 = id; AddCleaveAttack(id, 1); attacker+0x4C4 = 1
// Setting the round's target (+0x2134) is what sends the round's remaining
// attacks to the new enemy; AddCleaveAttack adds an extra one on top.
constexpr uintptr_t kMaxAttackRange = 0x0064B5D0;
constexpr uintptr_t kGetNearestEnemy = 0x00658C00;
constexpr uintptr_t kGetCreatureByGameObjectId = 0x005FFAD0;
constexpr uintptr_t kServerExoAppInternalHolder = 0x0092DC50;  // [[0x92DC50]+4] = CServerExoAppInternal*
constexpr uint32_t kObjectInvalid = 0x7F000000;
// CNWSCombatAttackData* CNWSCombatRound::GetAttack(int index) 0x73C0A0, ret 4;
// attack data +0x04 is the attack's target object id (ResolveMeleeAttack).
constexpr uintptr_t kGetAttack = 0x0073C0A0;
constexpr uintptr_t kAttackDataTarget = 0x04;
constexpr uintptr_t kRoundTarget = 0x2134;          // uint32 object id the round attacks
constexpr uintptr_t kCreatureTargetSwitched = 0x4C4;  // set to 1 by the cleave branch
// Attack data +0x40 = 1: this attack killed its target. Set by
// ResolvePostMeleeDamage and by CNWSCreature::ResolvePostRangedDamage
// (0x746980, (CNWSObject* target), ret 4) when the attack's total damage
// (0x73A850) reaches the target's current hit points and the target is not
// immortal/plot (+0xAF4 / +0xD0).
constexpr uintptr_t kAttackDataKilledTarget = 0x40;
constexpr uintptr_t kResolvePostRangedDamage = 0x00746980;
constexpr uint8_t kResolvePostRangedDamagePrologue[] = {0x6a, 0xff, 0x68, 0xfc, 0xed, 0x89, 0x00, 0x64};
// MaxAttackRange's third argument: with a ranged weapon 1 -> 20.0, 0 -> 40.0;
// melee -> reach + 1.5 either way. The engine's cleave passes 1, and so does
// melee retargeting. Ranged retargeting first used 1 as well (0.6.1: 40 m had
// picked a caged Hook Horror across the prelude arena); since it only picks
// enemies in the fight (0.6.2) it looks [rules] ranged_retarget_range metres
// (default 40), capped at the weapon's range (argument 0).
constexpr int kMaxAttackRangeRetarget = 1;
constexpr int kMaxAttackRangeWeapon = 0;

// Ranged retargeting keeps GetNearestEnemy's own checks and adds "in the
// fight", so it walks the area itself. GetNearestEnemy (0x658C00, ret 0x10;
// args range, exclude id, bMustBeSeen, bCheckPath), decompiled 2026-10-17:
// over the area's object ids (GetArea 0x63ECC0, ecx = creature, plain ret;
// area +0x150 id array, +0x154 count) it keeps the closest creature that is
// not the caller or `exclude`, that the caller's reaction (0x731520, ecx =
// caller, push id, ret 4, result in AL) marks 2 = enemy, that is neither dead
// (0x640750: hit points < 1, or < -9 for player-controlled) nor dying
// (0x6407E0) — both ecx = candidate, plain ret, result in EAX — and, with
// bMustBeSeen, whose entry in the caller's visible list (0x687BF0, ecx =
// caller, push id, ret 4; null when absent) has bit 0 of byte +4 set.
// Distance: centre to centre (floats x, y, z at +0x78) minus both creatures'
// personal space (float at [+0x304] + 8), strictly under the range.
// bCheckPath (a path test, 0x6DD2F0) is 0 in the engine's cleave and here.
constexpr uintptr_t kGetArea = 0x0063ECC0;
constexpr uintptr_t kAreaObjectIds = 0x150;    // uint32* object ids
constexpr uintptr_t kAreaObjectCount = 0x154;  // int32
constexpr uintptr_t kGetReactionTo = 0x00731520;
constexpr uint8_t kReactionEnemy = 2;
constexpr uintptr_t kGetDead = 0x00640750;
constexpr uintptr_t kGetDying = 0x006407E0;
constexpr uintptr_t kGetVisibleListElement = 0x00687BF0;
constexpr uintptr_t kVisibleElementFlags = 0x04;  // uint8, bit 0 = seen
constexpr uintptr_t kObjectPosition = 0x78;       // float x, y, z
constexpr uintptr_t kCreaturePersonalSpaceInfo = 0x304;  // pointer
constexpr uintptr_t kPersonalSpace = 0x08;               // float, in that block

// A creature's attack order, read the way 0x64B770 rebuilds the action status
// the client shows (+0xB0A = 2 "attacking", +0xB0C = target): the current
// action (int16 id at +0x68, -1 for none; its target at +0x6C), then the
// queued actions (+0xD4 -> CExoLinkedListInternal; +0 head node; node +4 next,
// +8 data; data +0 action id, +0x34 first parameter = an attack's target).
// Action 0xC = attack object (AddAttackActions 0x646D80 queues it; it stays
// the current action while the creature charges and fights).
constexpr uintptr_t kObjectCurrentAction = 0x68;        // int16
constexpr uintptr_t kObjectCurrentActionTarget = 0x6C;  // uint32
constexpr uintptr_t kObjectActionQueue = 0xD4;
constexpr uintptr_t kListHead = 0x00;
constexpr uintptr_t kListNodeNext = 0x04;
constexpr uintptr_t kListNodeData = 0x08;
constexpr uintptr_t kActionId = 0x00;          // int32
constexpr uintptr_t kActionFirstParam = 0x34;  // uint32
constexpr int kActionAttackObject = 0x0C;
// int CNWSCombatRound::GetTotalAttacks() 0x73E970, plain ret: main-hand +0x2138
// + off-hand +0x213C + extra +0x2148 + cleave +0x214C, at least 1.
constexpr uintptr_t kGetTotalAttacks = 0x0073E970;

// Character sheet. CNWSCreatureStats::UpdateCombatInformation() 0x693890
// (plain ret) rebuilds the 0x54-byte combat information block at stats +0x428
// that CNWSMessage::WriteGameObjUpdate_CharacterSheet (0x62BD30, flag 0x40)
// sends to the client: byte 0 main-hand attacks (its own BAB / 5 formula,
// 3 bits), byte 1 main-hand attack bonus (GetMeleeAttackBonus(0,0,0) or, for
// ranged weapons, GetRangedAttackBonus(0,0)), byte 3 off-hand attack bonus
// (GetMeleeAttackBonus(1,0,0)), +0x13/+0x14 and +0x1C/+0x1D crit range and
// multiplier. Both attack-bonus functions add GetBaseAttackBonus(0) directly
// (no iterative penalty). The sheet's separate "base attack bonus" (flag 0x20,
// call at 0x62C005) is left 3e: the client may use it for feat prerequisites.
constexpr uintptr_t kUpdateCombatInformation = 0x00693890;
constexpr uint8_t kUpdateCombatInformationPrologue[] = {0x6a, 0xff, 0x68, 0x64, 0x7b, 0x89, 0x00, 0x64};
constexpr uintptr_t kGetMeleeAttackBonus = 0x00694520;  // (int bOffHand, int, int), ret 0xC
constexpr uint8_t kGetMeleeAttackBonusPrologue[] = {0x51, 0x8b, 0x44, 0x24, 0x08, 0x53, 0x55, 0x56};
constexpr uintptr_t kGetRangedAttackBonus = 0x00694B00;  // (int, int), ret 8
constexpr uint8_t kGetRangedAttackBonusPrologue[] = {0x51, 0x55, 0x56, 0x8b, 0xf1, 0x8b, 0x46, 0x24};
constexpr uintptr_t kStatsCombatInformation = 0x428;     // uint8_t* block
constexpr uintptr_t kCombatInformationAttacks = 0x00;    // uint8 main-hand attacks shown

// Diagnostics ([debug] attack_trace).
// void CNWSCombatRound::InitializeNumberOfAttacks(), no stack args (the
// community plugin hooks it as (CNWSCombatRound*, void*) on 1.69).
constexpr uintptr_t kInitializeNumberOfAttacks = 0x0073C5A0;
constexpr uint8_t kInitializeNumberOfAttacksPrologue[] = {0x53, 0x56, 0x8b, 0xf1, 0x8b, 0x86, 0x5c, 0x21};
// void CNWSCreature::ResolveAttackRoll(CNWSObject* target), one stack arg.
constexpr uintptr_t kResolveAttackRoll = 0x00744390;
constexpr uint8_t kResolveAttackRollPrologue[] = {0x6a, 0xff, 0x68, 0xbb, 0xec, 0x89, 0x00, 0x64};

// Advantage/disadvantage (Phase 1b). ResolveAttackRoll rolls its d20 at
// 0x744401 (E8 rel32 -> RollDice) before it asks GetAttackModifierVersus for
// the modifier; the parry roll (0x74465E) and threat roll (0x744707) use the
// same RollDice. RollDice(int count, int sides) 0x47E120: this = the rules
// object [0x92DC64], two stack args (ret 8), result in AX.
constexpr uintptr_t kRollDice = 0x0047E120;
constexpr uintptr_t kAttackD20Call = 0x00744401;
// GetAttackModifierVersus(target creature) situational terms, as decompiled
// 2026-10-18 (asm 0x697165..0x697a13; RE-NOTES table). Target = the argument
// (null for a non-creature: the whole block is skipped); attacker = stats
// +0x24; "ranged" = the current attack data's +0x34.
//   blind      GetBlind(target) 0x6621D0 (ecx, plain ret, EAX), +2 if ranged
//              or the target lacks Blind-Fight (HasFeat(target stats, 0x198))
//   invisible  attacker->GetInvisible(attacker->vtable[0x10](), 0) 0x6621F0
//              (ecx = attacker, ret 8, EAX tested), same Blind-Fight rule
//   unseen     -4 unless the attacker's visible list has the target, bit 0
//   animation  target +0xAC: ranged 4/0x56/0x57 -2 (moving); 5/7/0x20/0x21/
//              0x24 prone: melee +4, ranged -4
//   stunned    target +0x9DD == 6: +2
//   flanked    0x749E50(ecx = attacker, push target) == 1: +2; ranged only
//              when centre distance^2 <= 100
//   in reach   ranged, no Point Blank Shot (HasFeat(attacker stats, 0x1B)),
//              centre distance^2 <= MaxAttackRange(target, attacker id, 0,
//              0)^2, and the target does not wield a ranged weapon
//              (0x6499A0, ecx = target: right-hand base item +0x18): -4
constexpr uintptr_t kGetBlind = 0x006621D0;
constexpr uintptr_t kGetInvisible = 0x006621F0;
constexpr uintptr_t kVtableAsObject = 0x10;
constexpr uintptr_t kIsFlankedBy = 0x00749E50;
constexpr uintptr_t kWieldsRangedWeapon = 0x006499A0;
constexpr uintptr_t kCreatureAnimation = 0xAC;    // int32
constexpr uintptr_t kCreatureActionState = 0x9DD;  // uint8: 6 stunned; 5 AddAttackActions refuses to act
constexpr uintptr_t kAttackDataRanged = 0x34;     // int32 bool
constexpr int kFeatBlindFight = 0x198;
constexpr int kFeatPointBlankShot = 0x1B;

// Racial combat bonuses (decision R1), as the engine adds them (decompiled
// 2026-10-18; race ids read from 0x8ACF30..0x8ACF4C):
//   Battle Training, +1 to hit once (GetAttackModifierVersus 0x696B05):
//     feat 232 vs target race 12 (goblinoid), 231 vs 14 (orc), 242 vs 15
//     (reptilian); target race = target stats +0x3EC
//   Good Aim, +1 (0x6965E8): ranged, feat 250, and the current attack weapon
//     (0x73E3F0, round, push attack data +0x30, ret 4) is thrown (base item
//     +8 WeaponWield == 11)
//   Halfling sling, +1 (0x696701): ranged, attacker race 3, right-hand item
//     (0x722510 on creature +0xB6C, push 0x10, ret 4) base item 61
//   Defensive Training, +4 AC (GetArmorClassVersus 0x692E35): defender feat
//     233, attacker race 18 (giant)
// The sheet's ranged bonus (0x694B00) adds Good Aim and the sling bonus too,
// from the right-hand item.
constexpr uintptr_t kStatsRace = 0x3EC;           // uint16 racial type
constexpr uintptr_t kCreatureInventory = 0xB6C;   // inventory (GetItemInSlot's this)
constexpr uintptr_t kGetItemInSlot = 0x00722510;  // slot 0x10 right hand, 0x20 left
constexpr uintptr_t kItemBaseItem = 0x0C;         // uint32 baseitems.2da row
constexpr uintptr_t kGetCurrentAttackWeapon = 0x0073E3F0;
constexpr uintptr_t kRulesObject = 0x0092DC64;    // [ ] = rules object (RollDice's this)
constexpr uintptr_t kRulesBaseItems = 0x18;       // base item array in it
constexpr uintptr_t kGetBaseItem = 0x004BC890;    // ecx = base item array, push row, ret 4
constexpr uintptr_t kBaseItemWield = 0x08;        // uint8 WeaponWield: 7 shield, 8 double, 11 thrown
constexpr uintptr_t kAttackDataType = 0x30;       // uint8 weapon attack type
constexpr int kSlotRightHand = 0x10;
constexpr int kFeatBattleTrainingOrcs = 231;
constexpr int kFeatBattleTrainingGoblins = 232;
constexpr int kFeatBattleTrainingReptilians = 242;
constexpr int kFeatDefensiveTrainingGiants = 233;
constexpr int kFeatGoodAim = 250;
constexpr int kRaceHalfling = 3;
constexpr int kRaceGoblinoid = 12;
constexpr int kRaceOrc = 14;
constexpr int kRaceReptilian = 15;
constexpr int kRaceGiant = 18;
constexpr int kBaseItemSling = 61;
constexpr uint8_t kWieldThrown = 11;

// Engine combat breakdown ([debug] combat_breakdown): with 0x93233C set,
// GetAttackModifierVersus, ResolveAttackRoll, GetDamageRoll and the save roll
// write a "[%d] <label>" line per term into the attack data (+0x68), and the
// attack-roll message (0x62F850) sends it to the client. A client → server
// message (0x6C45D0, subtype 9) stores both flags from the client:
//   0x6C5238  mov [0x93233C], edi   ; 89 3D 3C 23 93 00
//   0x6C523E  mov [0x932340], edi   ; 89 3D 40 23 93 00
// The switch blanks those stores and sets both flags to 1.
constexpr uintptr_t kCombatDebugFlag = 0x0093233C;
constexpr uintptr_t kCombatDebugFlag2 = 0x00932340;
constexpr uintptr_t kCombatDebugStore1 = 0x006C5238;
constexpr uintptr_t kCombatDebugStore2 = 0x006C523E;
constexpr uint8_t kCombatDebugStore1Bytes[] = {0x89, 0x3d, 0x3c, 0x23, 0x93, 0x00};
constexpr uint8_t kCombatDebugStore2Bytes[] = {0x89, 0x3d, 0x40, 0x23, 0x93, 0x00};

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
constexpr uintptr_t kStatsFaction = 0x90;      // int32 faction id (CNWSCreature::GetFaction
                                               // 0x687370 passes it to CFactionManager::GetFaction)

// CNWSCreature fields.
constexpr uintptr_t kCreatureObjectId = 0x04;  // uint32 object id (passed as id in ResolveAttack)
constexpr uintptr_t kCreatureStats = 0xC64;    // CNWSCreatureStats* (ResolveAttackRoll, GetLevel)
// uint16 associate type (0 none, 1 henchman, 2 animal companion, 3 familiar,
// 4 summoned, 5 dominated). GetAssociateId(type, nTh) 0x68D970 walks the
// associate list at +0xB34 and compares each associate's +0xB38 with `type`;
// ResolveAttack reads it too. Master object id at +0xB30.
constexpr uintptr_t kCreatureAssociateType = 0xB38;
constexpr uintptr_t kCreatureMaster = 0xB30;  // uint32 master object id (community layout)

}  // namespace nwmain
