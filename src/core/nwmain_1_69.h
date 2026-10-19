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
// [0x92DC50] = the app manager; [+4] = the CServerExoApp, whose methods at
// 0x5FFxxx forward to the CServerExoAppInternal at [app + 4].
constexpr uintptr_t kAppManager = 0x0092DC50;
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

// Save values (Phase 1c, RE-NOTES "Saving throws"). int CNWSCreatureStats::
// GetFort/Reflex/WillSavingThrow(int bBaseOnly), ret 4: effects (type 3)
// unless base-only + Luck of Heroes (382, +1) + Dark Blessing (473, +Cha) +
// ability (Con byte +0x3FD / GetDexMod(0) 0x6A1430 / Wis byte +0x401) +
// blueprint bonus (int8 +0x4C4 / +0x4C6 / +0x4C5) + the base save
// (0x6A26D0 / 0x6A2960 / 0x6A2810): class tables (CNWClass at [rules +0xE4]
// + id * 0x268, GetFort/Ref/WillSaveBonus(level) 0x53B700 / 0x53B720 /
// 0x53B740, ret 4, char) + epic bonus + Divine Grace (217, +Cha if > 0) +
// Great Fortitude 14 / Lightning Reflexes 24 / Iron Will 22 (+2) + Lucky
// (248, +1) + Fort: 388 (+1), 583 (+4); Reflex: 386 (+1), 585 (+4); Will:
// 380 (+1), 388 (+1), 695 (+4). The sheet (0x62BD30) and the save roll
// (0x749230) read these three.
constexpr uintptr_t kGetFortSavingThrow = 0x006AD4F0;
constexpr uintptr_t kGetReflexSavingThrow = 0x006AD630;
constexpr uintptr_t kGetWillSavingThrow = 0x006AD590;
constexpr uint8_t kGetFortSavingThrowPrologue[] = {0x51, 0x8b, 0x44, 0x24, 0x08, 0x53, 0x56, 0x57};
constexpr uint8_t kGetReflexSavingThrowPrologue[] = {0x51, 0x8b, 0x44, 0x24, 0x08, 0x53, 0x56, 0x57};
constexpr uint8_t kGetWillSavingThrowPrologue[] = {0x51, 0x8b, 0x44, 0x24, 0x08, 0x53, 0x56, 0x57};
constexpr uintptr_t kGetDexMod = 0x006A1430;  // char (ecx = stats, push bArmorDexCap 0), ret 4
constexpr uintptr_t kStatsConModifier = 0x3FD;   // int8
constexpr uintptr_t kStatsWisModifier = 0x401;   // int8
constexpr uintptr_t kStatsChaModifier = 0x403;   // int8
constexpr uintptr_t kStatsFortBlueprintBonus = 0x4C4;  // int8
constexpr uintptr_t kStatsWillBlueprintBonus = 0x4C5;  // int8
constexpr uintptr_t kStatsReflexBlueprintBonus = 0x4C6;  // int8
constexpr uintptr_t kRulesClasses = 0xE4;      // CNWClass array in the rules object
constexpr uintptr_t kClassStride = 0x268;
constexpr uintptr_t kGetFortSaveBonus = 0x0053B700;
constexpr uintptr_t kGetReflexSaveBonus = 0x0053B720;
constexpr uintptr_t kGetWillSaveBonus = 0x0053B740;
constexpr int kFeatGreatFortitude = 14;
constexpr int kFeatLightningReflexes = 24;
constexpr int kFeatIronWill = 22;
constexpr int kFeatDivineGrace = 217;
constexpr int kFeatLucky = 248;
constexpr int kFeatLuckOfHeroes = 382;
constexpr int kFeatDarkBlessing = 473;

// The save roll, CNWSCreature 0x749230 (this = the saving creature; save 1/2/3,
// DC (uint16), save type, versus object id, …; 7 stack args, ret 0x1C;
// returns 0 fail / 1 success / 2 immune). Its d20: call RollDice at 0x7499C9.
// Natural rolls (0x749CDE..): a 1 jumps to the fail path when the game option
// "Saving Throw Automatic Failure On 1" is set, a 20 succeeds:
//   0x749CE1  jnz 0x749CFD   ; 75 1A — not a 1: skip the option check
//   0x749D00  jz  0x749D47   ; 74 45 — a 20: success
// Faithful (C4): jnz -> jmp and the jz NOPed, so the total always decides.
constexpr uintptr_t kSavingThrowRoll = 0x00749230;
// C1: the script save commands FortitudeSave / ReflexSave / WillSave (108-110)
// share 0x6EE070 (CNWVirtualMachineCommands, (command, parameters), ret 8);
// it calls SavingThrowRoll on the target. The running script's OBJECT_SELF is
// commands +0x08. Its spell, as GetSpellId (0x6F6AA0) reads it: the object
// from CServerExoApp::GetObjectArray 0x5FFA90 ([[app+4]+0x10080]) and
// CGameObjectArray::GetGameObject(id, &object) 0x42E730 (0 = found), then
// vtable +0x10 (as CNWSObject) +0x188; for an area-of-effect object
// (vtable +0x54 non-null) the spell is AoE +0x1CC (its load code reads
// "SpellId" there, "AreaEffectId" at +0x1C4).
// C8: CTlkTable::GetSimpleString(strref, STR_RES* result, gender) 0x5C9C80
// (ret 0xC, returns 1; result text = CExoString at +0, sound resref at +8;
// strrefs with 0x1000000 come from the module's custom table). The character
// sheet asks for 317/318/319 here (0x5B2AA9 and others). CExoString::
// operator=(const char*) 0x5BA520 replaces a string's text.
constexpr uintptr_t kTlkGetSimpleString = 0x005C9C80;
constexpr uint8_t kTlkGetSimpleStringPrologue[] = {0x6a, 0xff, 0x68, 0xb8, 0xdd, 0x88, 0x00, 0x64};
constexpr uintptr_t kExoStringAssign = 0x005BA520;
constexpr uintptr_t kVmSaveCommand = 0x006EE070;
constexpr uint8_t kVmSaveCommandPrologue[] = {0x83, 0xec, 0x14, 0x8b, 0x41, 0x08, 0x56, 0x8d};
constexpr uintptr_t kVmCommandsObjectSelf = 0x08;
constexpr uintptr_t kGetObjectArray = 0x005FFA90;
constexpr uintptr_t kGetGameObject = 0x0042E730;
constexpr uintptr_t kVtableAsNwsObject = 0x10;
constexpr uintptr_t kVtableAsAreaOfEffect = 0x54;
constexpr uintptr_t kObjectLastSpellId = 0x188;
constexpr uintptr_t kAreaOfEffectSpellId = 0x1CC;
constexpr uint8_t kSavingThrowRollPrologue[] = {0x64, 0xa1, 0x00, 0x00, 0x00, 0x00, 0x6a, 0xff};
constexpr uintptr_t kSaveD20Call = 0x007499C9;
constexpr uintptr_t kSaveNatural1Check = 0x00749CE1;
constexpr uint8_t kSaveNatural1CheckBytes[] = {0x75, 0x1a};
constexpr uint8_t kSaveNatural1CheckSkip[] = {0xeb, 0x1a};
constexpr uintptr_t kSaveNatural20Check = 0x00749D00;
constexpr uint8_t kSaveNatural20CheckBytes[] = {0x74, 0x45};
// The save roll's feat bonuses (C7), all before its d20: HasFeat (0x6A3630)
// calls with the feat pushed just before — Hardiness vs spells 230 (twice),
// Arcane Defense 415–422, Still Mind 208, Lliira's Heart 439, Hardiness vs
// illusion 241 / enchantment 236, Deneir's Eye 437, Fearless 249, Resist
// Nature's Lure 202, Hardiness vs poisons 229, prestige poison saves 1070 /
// 467–463, Resist Poison 383, Snake Blood 386, Resist Disease 426, Strong
// Soul 388, Uncanny Dodge 255 / 254–251, Defensive Awareness 951. Effect
// (item / spell) bonuses are separate calls; the feat immunities come after
// the d20. Faithful: these calls answer "no feat".
constexpr uintptr_t kSaveRollFeatBonusCalls[] = {
    0x00749446, 0x00749467, 0x00749485, 0x007494A3, 0x007494C1, 0x007494DB, 0x007494F5, 0x0074950F, 0x00749529,
    0x0074954C, 0x00749567, 0x00749582, 0x0074959A, 0x007495BB, 0x007495DC, 0x007495F7, 0x0074961C, 0x00749637,
    0x0074967D, 0x00749696, 0x007496AF, 0x007496C8, 0x007496E1, 0x00749705, 0x00749720, 0x0074973A, 0x0074975B,
    0x00749776, 0x0074987F, 0x0074993E, 0x00749957, 0x00749970, 0x00749989, 0x007499A2};

// NWN save types (nwscript SAVING_THROW_TYPE_*) used by the species traits.
constexpr int kSaveTypeMindSpells = 1;
constexpr int kSaveTypePoison = 2;
constexpr int kSaveTypeFear = 4;
constexpr int kRaceDwarf = 0;
constexpr int kRaceElf = 1;
constexpr int kRaceGnome = 2;

// Spell DC, CNWSCreature::CalculateSpellSaveDC(spell id) 0x6659D0, ret 4
// (RE-NOTES "Saving throws"): spell row = 0x4F85F0(id) with ecx = [rules
// +0xC0]; casting class slot = byte creature +0x19C (0xFF none, 0xFE innate
// Charisma); creature +0xA70 set = cast from an item (innate level + 3);
// slot 0xFF and short +0x1AC == 0xFFFF = a class-less ability (innate level
// + 3); casting modifier = 0x6AD1D0(slot) on the stats (byte, ret 4);
// innate level = row byte +0x47, school = row byte +0x20 (1–8); Spell Focus
// feats by school: +6 0x262 + (school − 1), +4 0x189 + (school − 1), +2
// 0x23 (school 1) / 0xA6 + (school − 2).
constexpr uintptr_t kCalculateSpellSaveDc = 0x006659D0;
constexpr uint8_t kCalculateSpellSaveDcPrologue[] = {0x83, 0xec, 0x0c, 0xa1, 0x64, 0xdc, 0x92, 0x00};
constexpr uintptr_t kRulesSpells = 0xC0;
constexpr uintptr_t kGetSpell = 0x004F85F0;     // ecx = spell array, push id, ret 4
constexpr uintptr_t kCreatureCastingSlot = 0x19C;  // uint8
constexpr uintptr_t kCreatureCastingFromItem = 0xA70;  // int32
constexpr uintptr_t kCreatureCastingClass = 0x1AC;  // int16, 0xFFFF none
constexpr uintptr_t kGetCastingAbilityModifier = 0x006AD1D0;  // ecx = stats, push slot, ret 4, byte
constexpr uintptr_t kSpellInnateLevel = 0x47;   // uint8
constexpr uintptr_t kSpellSchool = 0x20;        // uint8

// Ability modifiers (int8): confirmed by the casting-ability switch 0x6AD1D0
// (class +0x216: 0 Str +0x3F9, 1 Dex GetDexMod(0), 2 Con +0x3FD, 3 Int
// +0x3FF, 4 Wis +0x401, 5 Cha +0x403).
constexpr uintptr_t kStatsStrModifier = 0x3F9;
constexpr uintptr_t kStatsIntModifier = 0x3FF;

// Chat (C8 `!saves`). Every player chat line (talk, shout, whisper, tell,
// party) reaches CNWSMessage::SendServerToPlayerChatMessage 0x620C30 from
// the chat handler 0x6C4200 (after the module's chat script), with 6 stack
// slots (ret 0x18): channel, sender object id, the text (CExoString by
// value: char*, buffer length — the callee destroys it), two more. Replies:
// SendServerToPlayerChat_ServerTell 0x6220D0 (this = the same CNWSMessage,
// player id, CExoString by value — destroyed by the callee, so it must come
// from the engine's CExoString(const char*) 0x5BA260); player id from
// CServerExoApp::GetPlayerIDByGameObjectID 0x5FFB70 (0xFFFFFFFE for none; it
// forwards to the internal 0x6108B0 — 0.8.2 to 0.8.6 called that one with the
// app pointer, the wrong object, so `!saves` could not reply).
constexpr uintptr_t kSendServerToPlayerChatMessage = 0x00620C30;
constexpr uint8_t kSendServerToPlayerChatMessagePrologue[] = {0x6a, 0xff, 0x68, 0x48, 0x37, 0x89, 0x00, 0x64};
constexpr uintptr_t kSendServerToPlayerChatServerTell = 0x006220D0;
constexpr uintptr_t kExoStringFromText = 0x005BA260;
constexpr uintptr_t kGetPlayerIdByGameObjectId = 0x005FFB70;

// Opportunity attacks (D2; RE-NOTES "Two-weapon fighting and attacks of
// opportunity"). Broadcast 0x655E70, this = the provoker (target, or
// 0x7F000000 = everyone near; tumble flag; ret 8). One AoO per round per
// creature: CNWSCombatRound +0x2128 is set to 1 by the constructor and by
// EndCombatRound 0x73B7A0 (UpdateCombatRound 0x73B8F0 runs it when the
// round's time is up) and drops by 1 per AoO; NWN 1.69 has no Combat
// Reflexes. Provokers and the faithful patches (a scan found no other jump
// into any patched sequence, except the potion one noted below):
//   moving    0x64799E in the move action 0x6475B0; Spring Attack (392)
//             skips it: 0x647990 je provoke (74 08) → jmp
//   potions   0x67B9E5 in the item-use action 0x67B550, for base items 49
//             (potions), 54 (deleted row) and 104 (crafted potion). The
//             party is exempt when difficulty option 2 (0x600100(2)) is set
//             (dialog.tlk: "No attacks of opportunity on players using
//             potions"); everyone else jumps straight to the call
//             (0x67B9B0 / 0x67B9C6 → 0x67B9DC), so the call itself becomes
//             add esp, 8 (the callee's ret 8)
//   casting   0x67CA8F / 0x67CBC0 in the cast action 0x67C070, each behind
//             an Epic Improved Combat Casting (696) test: 0x67CA85 /
//             0x67CBB6 jne (75 0D) → jmp
//   event 20  EVENT_BROADCAST_AOO (id at 0x8B6484), queued with
//             AddEventDeltaTime 0x6BF770 and handled at 0x64A364 in the
//             creature event handler 0x64A220:
//     ranged  0x7426B0 (ranged attack events), once per shot; the party is
//             exempt when difficulty option 1 (0x600100(1)) is set:
//             0x7427C4 je queue (74 15) → jmp 0x7427F5 (EB 2F)
//     disarm  0x743F30 (melee attack events), special attack 9:
//             0x744031 jne (75 1A) → jmp
//     unarmed 0x743F30, no weapon, no Improved Unarmed Strike (21), not
//             special attack 65002: 0x74405C jne (75 5D) → jmp
constexpr uintptr_t kAttackOfOpportunityBroadcast = 0x00655E70;
struct SmallPatch {
  const char* what;
  uintptr_t address;
  uint8_t size;
  uint8_t original[5];
  uint8_t patched[5];
};
constexpr SmallPatch kFaithfulOpportunityPatches[] = {
    {"drinking potions", 0x0067B9E5, 5, {0xe8, 0x86, 0xa4, 0xfd, 0xff}, {0x83, 0xc4, 0x08, 0x90, 0x90}},
    {"casting (first)", 0x0067CA85, 2, {0x75, 0x0d}, {0xeb, 0x0d}},
    {"casting (second)", 0x0067CBB6, 2, {0x75, 0x0d}, {0xeb, 0x0d}},
    {"Spring Attack no longer protects", 0x00647990, 2, {0x74, 0x08}, {0xeb, 0x08}},
    {"ranged attacks", 0x007427C4, 2, {0x74, 0x15}, {0xeb, 0x2f}},
    {"disarm", 0x00744031, 2, {0x75, 0x1a}, {0xeb, 0x1a}},
    {"unarmed attacks without Improved Unarmed Strike", 0x0074405C, 2, {0x75, 0x5d}, {0xeb, 0x5d}},
};

// Conditions (RE-NOTES "Condition effects").
// - CNWSEffectListHandler::OnApplyDeaf 0x66D770 adds a spell failure effect
//   (type 25) of 20 — the `push 0x14` before SetInteger(0, 20) at 0x66D942.
// - OnApplyEntangle 0x66CB20 adds attack -2 (type 0x0B, `push 2` at
//   0x66CD00) and Dexterity -4 (type 0x25, `push 4` at 0x66CD56); its AI-state
//   effect (can't move) stays.
// Faithful makes each 0.
constexpr SmallPatch kFaithfulConditionPatches[] = {
    {"Deafened adds no spell failure", 0x0066D942, 2, {0x6a, 0x14}, {0x6a, 0x00}},
    {"Entangled: no attack -2", 0x0066CD00, 2, {0x6a, 0x02}, {0x6a, 0x00}},
    {"Entangled: no Dexterity -4", 0x0066CD56, 2, {0x6a, 0x04}, {0x6a, 0x00}},
    // Coup de grace (0x745560): `mov dword [eax+0x44], 1` at 0x745657 marks the
    // attack an automatic hit and kill against a helpless creature of level 4
    // or less; 2024 has none (a hit from within 5 ft is a critical instead).
    {"no coup de grace", 0x0074565A, 1, {0x01}, {0x00}},
};

// CN5 (faithful): CNWSEffectListHandler::OnApplySetState(object, effect,
// loading) 0x66DD30 (thiscall, ret 0xC; returns 1 when the effect is not
// kept). It refuses for no creature, creature +0xD0, dead (not loading) and
// creature +0xAF4 (all but states 20/21); fear (state 3) then checks
// GetIsImmune(4) / HasFeat(300) and the mind-spell immunity GetIsImmune(1) /
// HasFeat(0xD8) unless the effect is supernatural (flags & 0x18 == 0x10) with
// integer 1 == 1 — each sending NWN's "immune" feedback.
constexpr uintptr_t kOnApplySetState = 0x0066DD30;
constexpr uint8_t kOnApplySetStatePrologue[] = {0x6a, 0xff, 0x68, 0xce, 0x63, 0x89, 0x00, 0x64};
constexpr uintptr_t kCreatureDeadFlag = 0xD0;
constexpr uintptr_t kCreatureRefusesStates = 0xAF4;
constexpr int kImmunityMindSpells = 1;
constexpr int kImmunityFear = 4;
constexpr int kFeatMindImmunity = 0xD8;
constexpr int kFeatFearImmunity = 300;
constexpr uint16_t kEffectTypeIcon = 67;  // integer 0 = effecticons.2da row
constexpr int kIconFrightened = 14;

// Push (D4; RE-NOTES "Push: fall animation and short moves"):
// - CNWSCombatRound (creature +0xAC8), as the hit flinch in OnApplyDamage
//   (0x668C0C) uses it: GetActionResponsePossible(attacker id, ms) 0x73BC90,
//   SetRoundPaused(1, attacker id) 0x73B9E0, SetPauseTimer(ms, 0) 0x73BA60
//   (adds to a running timer), DecrementRoundLength(ms, 0) 0x73BAF0; paused
//   +0x2104, paused by +0x2108, timer +0x210C. The AI update ends the pause and
//   resets the animation (SetAnimation(1)).
// - CNWSCreature::SetAnimation(int) = vtable +0x80 (0x649600, ret 4). The
//   knockdown falls with 7 when dot(direction to the attacker, facing) >=
//   0.707, else 5 (0x66D190); facing is a vector at object +0x84.
// - CNWSArea::ComputeHeight(Vector) 0x6D9730 (ret 0xC; -6.0 off the walkmesh);
//   NoNonWalkPolysOnLine(x1, y1, x2, y2, -0.1f) 0x6DF020 (ret 0x14; 1 = the
//   line crosses only walkable faces).
// - As AIActionJumpToPoint (0x680FDA) moves within an area: CNWSObject::
//   SetPosition(Vector, 0) 0x63D2D0 (ret 0x10), then the creature's trigger
//   update 0x6833F0(Vector, area +0xC8) (ret 0x10).
constexpr uintptr_t kRoundGetActionResponsePossible = 0x0073BC90;
constexpr uintptr_t kRoundSetRoundPaused = 0x0073B9E0;
constexpr uintptr_t kRoundSetPauseTimer = 0x0073BA60;
constexpr uintptr_t kRoundDecrementRoundLength = 0x0073BAF0;
constexpr uintptr_t kRoundPaused = 0x2104;
constexpr uintptr_t kRoundPausedBy = 0x2108;
constexpr uintptr_t kRoundPauseTimer = 0x210C;
constexpr uintptr_t kVtableSetAnimation = 0x80;
constexpr int kAnimationFallFacing = 7;
constexpr int kAnimationFallTurned = 5;
constexpr float kFallFacingDot = 0.707f;
constexpr uintptr_t kObjectFacing = 0x84;
constexpr uintptr_t kAreaComputeHeight = 0x006D9730;
constexpr uintptr_t kAreaNoNonWalkPolysOnLine = 0x006DF020;
constexpr float kAreaOffWalkmeshHeight = -6.0f;
constexpr uintptr_t kObjectSetPosition = 0x0063D2D0;
constexpr uintptr_t kCreatureUpdateSubAreas = 0x006833F0;
constexpr uintptr_t kAreaId = 0xC8;
constexpr uint32_t kCreatureSizeLarge = 4;  // creature +0x4D4: 1 tiny .. 4 large, 5 huge
// The concealment / miss-chance roll (0x746EF0, called by ResolveAttackRoll
// before the hit decision) is entered only past this gate (eax = attacker miss
// chance, ebx = target concealment): `test eax,eax; jg 0x7472D8; test ebx,ebx;
// jle 0x7473E1`. A jmp 0x7473E1 (the hit-only tail: Deflect Arrows ...) skips
// the roll; nothing jumps into the replaced bytes.
constexpr SmallPatch kFaithfulConcealmentGate = {
    "skip the concealment roll", 0x007472CC, 5, {0x85, 0xc0, 0x7f, 0x08, 0x85}, {0xe9, 0x10, 0x01, 0x00, 0x00}};

// A creature's applied effects (CNWSObject): CGameEffect* array at +0xE8,
// count at +0xEC, sorted by effect type. Per effect: type +0x08 (uint16),
// creator +0x18, integer count +0x34 and int32 array +0x38 (GetInteger
// 0x666A80). SetState (type 8) keeps its state in integer 0; creature +0x9DD
// holds only the highest state number held.
constexpr uintptr_t kObjectEffects = 0xE8;
constexpr uintptr_t kObjectEffectCount = 0xEC;
constexpr uintptr_t kEffectIntegerCount = 0x34;
constexpr uintptr_t kEffectIntegers = 0x38;
constexpr uint16_t kEffectTypeSetState = 8;
constexpr uint16_t kEffectTypeEntangle = 18;
constexpr uint16_t kEffectTypePoison = 35;     // kept while the poison lasts (OnApplyPoison 0x670590)
constexpr uint16_t kEffectTypeConcealment = 76;  // ints: %, race (28 all), 2 alignment filters, 0 all/2 ranged/3 melee
constexpr int kStateFrightened = 3;
constexpr int kStateDazed = 5;
constexpr int kStateStunned = 6;
constexpr int kStateParalyzed = 8;
constexpr int kStateAsleep = 9;
constexpr int kStatePetrified = 15;
constexpr int kRacialTypeAll = 28;
constexpr int kFeatEpicSelfConcealmentFirst = 0x2EC;  // 10% .. 50%
constexpr int kFeatEpicSelfConcealmentLast = 0x2F0;
// CNWSCreatureStats::GetIsImmune(uint8 type, CNWSCreature* versus) 0x6B0090
// (thiscall, ret 8; nonzero = immune). Before a critical the engine checks it
// with type 31 and HasFeat(0x380) (0x74477B).
constexpr uintptr_t kGetIsImmune = 0x006B0090;
constexpr int kImmunityCriticalHit = 31;
constexpr int kFeatCriticalHitImmunity = 0x380;
// Attack data +0x50 (uint8): 1 hit, 2 parried, 3 critical, 4 miss, 7
// automatic hit (coup de grace), 8 concealed, 9 attacker miss chance.
constexpr uint8_t kAttackResultHit = 1;
constexpr uint8_t kAttackResultCritical = 3;

// Two-weapon fighting (D1; RE-NOTES "Two-weapon fighting and attacks of
// opportunity"):
//   off-hand attacks  CNWSCombatRound 0x73BF80 (plain ret; one caller,
//                     InitializeNumberOfAttacks at 0x73C651) stores and returns
//                     round +0x213C: 1 with a non-shield weapon in the left
//                     hand and a right-hand weapon no bigger than the creature,
//                     or a double weapon (WeaponWield 8); 2 with Improved
//                     Two-Weapon Fighting (20). Faithful: hooked, then
//                     rules::OffHandAttacks.
//   attack penalty    GetAttackModifierVersus 0x695C06–0x695CE8 builds it in a
//                     local that stays 0 when skipped (all outside jumps land on
//                     0x695C06); the sheet's GetMeleeAttackBonus repeats it at
//                     0x69485F–0x69491D in ebx (0 on every skip path). Faithful:
//                     each block's first 5 bytes become a jmp to its end.
//   off-hand damage   CNWSCreatureStats 0x697D10 (target or 0, bOffHand; ret 8;
//                     one caller, GetDamageRoll at 0x69BE8B) returns the damage
//                     bonus; with bOffHand the Strength term is Str mod × 0.5
//                     (float 0x8A6930, truncated) when positive, else Str mod.
//                     Faithful: hooked, NWN's term swapped for
//                     rules::OffHandDamageAbilityModifier.
// Also seen, not D1: two-handed weapons add Str × 1.5 ("Two Handed Strength
// Bonus", same function); Point Blank Shot adds +1 damage within 5 m.
constexpr uintptr_t kGetOffHandAttacks = 0x0073BF80;
constexpr uint8_t kGetOffHandAttacksPrologue[] = {0x56, 0x57, 0x8b, 0xf9, 0x8b, 0x87, 0x5c, 0x21};
constexpr uintptr_t kGetDamageBonus = 0x00697D10;
constexpr uint8_t kGetDamageBonusPrologue[] = {0x6a, 0xff, 0x68, 0x60, 0x7f, 0x89, 0x00, 0x64};
constexpr SmallPatch kFaithfulTwoWeaponPatches[] = {
    {"no off-hand attack penalty (attack roll)", 0x00695C06, 5, {0x8b, 0x44, 0x24, 0x28, 0x83},
     {0xe9, 0xdd, 0x00, 0x00, 0x00}},
    {"no off-hand attack penalty (character sheet)", 0x0069485F, 5, {0x8b, 0x44, 0x24, 0x18, 0x85},
     {0xe9, 0xb9, 0x00, 0x00, 0x00}},
};
constexpr int kSlotLeftHand = 0x20;
constexpr uint8_t kBaseItemWieldDouble = 8;
constexpr int kFeatImprovedTwoWeaponFighting = 20;
constexpr int kFeatTwoWeaponFighting = 41;

// Weapon mastery (D3). Every object keeps its local variables (the ones
// scripts read with GetLocalInt) at object +0xD8, saved with the game:
// CNWSScriptVarTable::GetInt(CExoString*) 0x74E7B0 (ret 4; 0 when missing)
// and SetInt(CExoString*, value, flag) 0x74E970 (ret 0xC; creates the entry,
// copying the name). Names compare with strcmp; a CExoString is {char*,
// length including the null}.
constexpr uintptr_t kObjectVarTable = 0xD8;
constexpr uintptr_t kVarTableGetInt = 0x0074E7B0;
constexpr uintptr_t kVarTableSetInt = 0x0074E970;
// A finished rest: CNWSCreatureStats::ResetFeatRemainingUses() 0x6A4B10 (plain
// ret) runs once a rest completes, from AIActionRest (0x688ADA, the rest
// action 0x6883E0) and from the script command ForceRest (0x710467). Rest
// 0x659C50 is only the start (it refuses with enemies within 30 m).
constexpr uintptr_t kResetFeatRemainingUses = 0x006A4B10;
constexpr uint8_t kResetFeatRemainingUsesPrologue[] = {0x8b, 0x41, 0x10, 0x33, 0xd2, 0x85, 0xc0, 0x7e};
// Messages outside a chat: CServerExoApp::GetNWSMessage() 0x5FFBD0, plain ret.
constexpr uintptr_t kGetNwsMessage = 0x005FFBD0;

// Weapon mastery effects (D4). Attack data +0x50 is the result (1 hit, 2
// parried, 3 critical, 4 miss, 7 automatic hit, 8/9 concealment, 10
// devastating critical). Rounds: CNWSCombatRound::EndCombatRound() 0x73B7A0
// (plain ret; this = the round, owner at +0x215C) runs at the end of each of
// a creature's combat rounds (UpdateCombatRound 0x73B8F0); D4 counts them as
// the 2024 turns Vex and Sap last for.
constexpr uintptr_t kAttackDataResult = 0x50;
// CNWSObject::GetCurrentHitPoints(bExcludeTemporary) 0x63AA50: current hit
// points int16 at +0xB4, temporary hit points int32 at +0xBC.
constexpr uintptr_t kObjectCurrentHitPoints = 0xB4;
constexpr uintptr_t kObjectTemporaryHitPoints = 0xBC;  // int32
// CNWSEffectListHandler::OnApplyDamage(object, effect, bLoadingGame) 0x6686C0
// (ret 0xC): the apply handlers are a table by effect type x 4 filled at
// 0x667B90 (death 19 at +0x4C = 0x669A10, damage 38 at +0x98). Immunity and
// resistance are applied in it, so the hit points it takes off are the damage
// dealt. Effect +0x18 = creator.
constexpr uintptr_t kOnApplyDamage = 0x006686C0;
constexpr uint8_t kOnApplyDamagePrologue[] = {0x6a, 0xff, 0x68, 0xb8, 0x5e, 0x89, 0x00, 0x64};
constexpr uintptr_t kEffectCreator = 0x18;
constexpr uintptr_t kEndCombatRound = 0x0073B7A0;
constexpr uint8_t kEndCombatRoundPrologue[] = {0x53, 0x33, 0xdb, 0x56, 0x8b, 0xf1, 0x8b, 0x8e};

// Engine effects (D4 Graze / Slow / Topple), as NWScript builds them:
// EffectDamage 0x6EC4B0, EffectKnockdown (command 134, shared handler
// 0x6F0610), EffectMovementSpeedDecrease 0x6F1C50; the script command table
// is filled at runtime (command index x 4: ApplyEffectToObject 220 at +0x370).
// A CGameEffect is 0x90 bytes from the engine's operator new 0x84AC1C
// (cdecl), constructed with 0x666680(1). +0x08 word true type (38 damage,
// 20 knockdown, 29 movement speed decrease); +0x0A word: bits 0-2 duration
// type (0 instant, 1 temporary, 2 permanent), bits 3-4 subtype (8 magical,
// 0x18 extraordinary); +0x0C float duration; +0x18 creator. SetNumIntegers
// 0x6669C0, SetInteger(index, value) 0x666A90, SetCreator 0x667550.
// Damage: 20 integers, 0-12 the amount per damage type (bit index of the
// DAMAGE_TYPE flag; -1 unused), 14 = 1000, 15 = the type flag, 16 = damage
// power. Slow: integer 0 = percent. Applied with CNWSObject::ApplyEffect
// (effect, 0, 1) 0x63D4A0, as ApplyEffectToObject does.
constexpr uintptr_t kEngineOperatorNew = 0x0084AC1C;
constexpr uintptr_t kGameEffectCtor = 0x00666680;
constexpr uintptr_t kGameEffectSetNumIntegers = 0x006669C0;
constexpr uintptr_t kGameEffectSetInteger = 0x00666A90;
constexpr uintptr_t kGameEffectSetCreator = 0x00667550;
constexpr uintptr_t kObjectApplyEffect = 0x0063D4A0;
// The engine applies a hit's damage effect later, as EVENT_APPLY_EFFECT (id 5,
// [0x8B6448]) queued with CServerAIMaster::AddEventDeltaTime(days, ms, caller,
// target, event, data) 0x6BF770 (ret 0x18; AI master = CServerExoApp::
// GetServerAIMaster 0x5FFBA0 = [[app+4]+0x10064]) at half the attack's
// animation length (attack data +0x02, word); the target's event handler
// 0x64A220 then calls ApplyEffect. D4 does the same.
constexpr uintptr_t kAddEventDeltaTime = 0x006BF770;
constexpr uintptr_t kGetServerAIMaster = 0x005FFBA0;
constexpr uint32_t kEventApplyEffect = 5;
constexpr uintptr_t kAttackDataAnimationLength = 0x02;
// D4 Cleave: CNWSCombatRound::AddCleaveAttack(target, bGreatCleave) 0x73D4B0
// (ret 8) queues a round action (type 9) at the target, counts it as an extra
// attack (+0x2148) and puts special attack 6 (Cleave; 391 for Great Cleave)
// first in the special-attack list. ResolveMeleeSpecialAttack 0x742BC0 clears
// a special attack the attacker has no uses of (GetFeatRemainingUses 0x6A3850
// == 0), so without the feat it is an ordinary attack. The attack goes at the
// round's target (+0x2134), not the action's, which is why the Great Cleave
// branch sets it first (seen in game 0.9.6: without it the extra attack went
// at the first creature).
// Main-hand Strength in the damage bonus 0x697D10: Str x 1.5 (float
// 0x8ADA34, truncated) when the right-hand weapon is one size above the
// creature (base item +0x19, creature size +0x4D4), the attack is melee and
// the weapon is not double; Str otherwise.
constexpr uintptr_t kAddCleaveAttack = 0x0073D4B0;
constexpr uintptr_t kBaseItemSize = 0x19;
constexpr uintptr_t kCreatureSize = 0x4D4;
constexpr float kCleaveSplashMetres = 1.524f;  // 5 ft
constexpr size_t kGameEffectSize = 0x90;
constexpr uintptr_t kEffectType = 0x08;
constexpr uintptr_t kEffectFlags = 0x0A;
constexpr uintptr_t kEffectDuration = 0x0C;
constexpr uint16_t kEffectTypeDamage = 38;
constexpr uint16_t kEffectTypeKnockdown = 20;
constexpr uint16_t kEffectTypeMovementSpeedDecrease = 29;
constexpr uint16_t kEffectDurationInstant = 0;
constexpr uint16_t kEffectDurationTemporary = 1;
constexpr uint16_t kEffectSubtypeExtraordinary = 0x18;
constexpr int kDamageTypeSlashing = 4;
constexpr int kFeatWeaponFinesse = 42;
constexpr int kSaveFortitude = 1;

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
