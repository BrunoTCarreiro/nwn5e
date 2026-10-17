# Reverse-engineering notes

## Target

| | nwmain.exe | nwserver.exe |
|---|---|---|
| Path | `X:\GOG\NWN Diamond\` | same |
| Version | 1.69 (GOG Diamond) | 1.69 |
| Format | PE32 i386, MSVC 6 linker, base 0x400000, no `.reloc` | same |
| Linked | 2008-07-03 (`TimeDateStamp` 0x486CFADC) | 2008-07-03 |
| SizeOfImage | 0x05E6D000 | |
| SHA-256 | `4d8aa22b1bc9a48c108f3c5d4dc08f49dcddad98d863b89dc3684968d2211c49` | |
| Packing | none (normal section entropy) | none |

Single-player runs an embedded server inside `nwmain.exe`, so the server rule
classes (`CNWSCreature`, `CNWSCreatureStats`, `CNWSCombatRound`, …) are
compiled into it. Their addresses differ from `nwserver.exe`.

## Function map

Source of names: the 1.69 `nwserver.exe` addresses used by Shadooow's
Community Patch plugin (`github.com/Shad000w/nwnx_patch-win`): the
`CreateHook` calls in `patch.cpp` and the function-pointer addresses (with
signatures) in `nwn_internals/*.cpp`. The repository has no license, so only
addresses, names and signatures are used, no code. Spot-checked: every
`CreateHook` address starts a function (preceded by `0x90` padding) in the
GOG `nwserver.exe`. Seeds: `re/seeds/nwserver-1.69-names.tsv` (573 names).

`re/match-bytes.mjs` locates each in `nwmain.exe`:

1. Masked byte signature of 24–256 bytes from the server function (call/jmp
   rel32 and any 32-bit value inside the image are wildcards), searched in
   `nwmain.exe`'s `.text`, lengthened until unique.
2. Spacing check: the linker keeps each object file's functions in order, so
   two neighbouring functions sit the same distance apart in both executables.
   A unique match whose distance to a neighbouring match is preserved is
   **confirmed**; ambiguous or missing entries are resolved from a confirmed
   neighbour's distance when the predicted address starts a function.

Result (`re/maps/nwmain-1.69-bytematch.tsv`, 573 names): 446 confirmed,
33 unique, 31 by-spacing, 34 predicted, 25 ambiguous, 4 none.

Independent cross-check: the functions REA/Ghidra showed being called from
`ResolveAttackRoll` (0x744390) match the map's names —
`GetAttackModifierVersus` 0x6958E0, `GetArmorClassVersus` 0x6925E0,
`GetCriticalHitRoll` 0x69C290 — and the map places `ResolveAttackRoll`
itself at 0x744390. `GetDamageRoll` (0x69AEB0) is one of the callers REA
listed for `GetCriticalHitMultiplier`.

Rule functions used so far:

| Function | nwserver.exe | nwmain.exe | Map status |
|---|---|---|---|
| `CNWSCreatureStats::GetCriticalHitRoll(int bOffhand)` | 0x4778B0 | 0x69C290 | confirmed |
| `CNWSCreatureStats::GetCriticalHitMultiplier(int bOffhand)` | 0x477A20 | 0x69C400 | confirmed |
| `CNWSCreatureStats::GetBaseAttackBonus` | 0x46ED40 | 0x693720 | confirmed |
| `CNWSCreatureStats::GetBaseFortSavingThrow` | 0x47DD90 | 0x6A26D0 | confirmed |
| `CNWSCreatureStats::GetBaseWillSavingThrow` | 0x47DED0 | 0x6A2810 | confirmed |
| `CNWSCreatureStats::GetBaseReflexSavingThrow` | 0x47E020 | 0x6A2960 | confirmed |
| `CNWSCombatRound::InitializeNumberOfAttacks` | 0x530E40 | 0x73C5A0 | confirmed |
| `CNWSCombatRound::StartCombatRound` | 0x52FA00 | 0x73B160 | confirmed |
| `CNWSCreature::ResolveAttack` | 0x547580 | 0x740180 | confirmed |
| `CNWSCreatureStats::LevelUp` | 0x4847F0 | 0x6A9120 | confirmed |
| `CNWSCreatureStats::CanLevelUp` | 0x483B00 | 0x6A8440 | confirmed |

Semantics:

- `GetCriticalHitRoll` returns the lowest d20 roll that threatens a critical
  (21 − threat range): 20 for a ×2/20 weapon, 19 for 19–20, lower with
  Improved Critical / Keen.
- `GetCriticalHitMultiplier` returns the weapon's damage multiplier.
- Both are `__thiscall` on `CNWSCreatureStats`; hooks use `__fastcall` with an
  unused `edx`.

## Attack resolution (decompiled with REA + Ghidra 12.1.4)

Ghidra's full auto-analysis of `nwmain.exe` takes ~8.5 min (511,861 ms) on
this PC; after that, REA queries answer in well under a second.

```
CNWSCreature::ResolveAttack            0x740180  (community name, confirmed)
 ├─ ranged → ResolveRangedAttack        0x740480  (REA: branch on 0x6499A0)
 │           └─ ResolveRangedSpecialAttack 0x740650 (community name, confirmed)
 └─ melee  → ResolveMeleeAttack         0x742A80  (REA: branch on 0x6499A0)
             ├─ special (+0x52 != 0) → ResolveMeleeSpecialAttack 0x742BC0 (community name, confirmed)
             └─ normal → ResolveAttackRoll 0x744390, then damage if result ∈ {1,3,5,6,7,10}
```

The two special-attack functions were located independently by the byte map
and by REA's decompiled call structure — the two methods agree.

`ResolveAttackRoll(CNWSObject* target)` 0x744390, per attack:

1. `d20 = Rand(1,20)` (0x47E120); attack modifier = 0x6958E0(target); AC.
2. 0x746EF0 — defensive effects (concealment etc.); may end the attack.
3. Parry mode: `d20 + Parry skill (10)` vs the attack total → result 2
   (parried); beating it by 10 queues a riposte (0x73D120).
4. Natural 1, or total < AC without a natural 20 → result 4 (miss);
   `+0x14` = miss margin.
5. `GetCriticalHitRoll(offhand)` (0x69C290); on a threat: `+0x48 = 1`,
   threat roll `d20` into `+0x0F`, then

   ```
   0x744721  cmp eax, [esp+18h]   ; threat roll + attack modifier vs AC
   0x744725  jl  0x744875         ; 0F 8C 4A 01 00 00 → ordinary hit
   ```

   then critical-immunity checks (immunity 0x1F, feat 0x380) → result 3
   (critical) or result 1 with a feedback message.

Attack-data fields seen (`CNWSCombatAttackData`, from 0x73C0A0):
`+0x0E` to-hit roll, `+0x0F` threat roll, `+0x10` attack modifier,
`+0x14` miss margin, `+0x48` critical threat, `+0x50` attack result
(1 hit, 2 parried, 3 critical, 4 miss, 7 automatic hit when `+0x44 == 1`),
`+0x52` special attack.

Callers (REA `procedure_callers`):

- `GetCriticalHitRoll` 0x69C290 ← 0x744390 (ResolveAttackRoll), 0x693890
  (unverified: likely the combat information shown on the character sheet).
- `GetCriticalHitMultiplier` 0x69C400 ← 0x65E7D0, 0x693890, 0x697D10, 0x69AEB0.

## In-game observations (2026-10-17, debug every_hit_crits run)

- Every hit was a critical: hooks at 0x69C290 / 0x69C400 and patches at
  0x744725 / 0x74472B all applied on the first try.
- 50 `GetCriticalHitRoll` calls landed within 0.2 s right after loading —
  more than the attacks in that window, so most come from the other caller
  (0x693890), consistent with it computing displayed combat information.
- Vanilla `GetCriticalHitRoll` returned **21** once (no roll can threaten),
  and vanilla `GetCriticalHitMultiplier` returned **x0** for some attackers.
  Both look like "this attack cannot crit" (e.g. creature/natural weapons);
  the hooks currently force 20 / x2. 2024 rules let any attack roll crit, but
  confirm what 21 / x0 encode before relying on it.

## Attack modifier and AC (decompiled 2026-10-17)

`GetAttackModifierVersus(target)` 0x6958E0, `ret 4`. Every one of its five
`GetBaseAttackBonus` calls pushes 0 with `ecx` = the stats; the result goes to
`ebx`, then the 3e iterative penalty is subtracted before the other bonuses
are added:

| Branch (attack data +0x30) | ebx |
|---|---|
| normal | BAB − 5 × attack index (`edi`) |
| monk attack tables (0x695110) | BAB − 3 × attack index |
| 2 (off-hand) | BAB − 5 × off-hand attacks made (combat round +0x2140) |
| 6 / 8, special attacks 0x363/0x364 | BAB |

So `engine result − GetBaseAttackBonus(0) + PB` swaps only the 3e BAB.

`GetArmorClassVersus(attacker, bTouch)` 0x6925E0, `ret 8`; the attack roll
sign-extends its 16-bit result.

`GetBaseAttackBonus(bPreEpicOnly)` 0x693720, `ret 4`: sums each class slot's
table BAB (class id at slot +0x1B2, slots 0x118 apart), adds the epic bonus
when the argument is 0, and returns the override byte at +0x411 instead when
it is non-zero. `GetLevel(bCountNegativeLevels)` 0x692520, `ret 4`, result in
AL: sums slot +0x1B3 levels (minus +0x1B4 negative levels when asked).

`CNWSCreatureStats` fields used by the hooks:

| Offset | Field | Evidence |
|---|---|---|
| +0x24 | owning CNWSCreature* | GetLevel follows it to +0xC64 |
| +0x70 | XP | AddExperience adds to it; CanLevelUp compares it to the XP table |
| +0x74 | is player character | CanLevelUp runs the XP check only when it is non-zero |
| +0x9C | challenge rating (float) | Not read in code yet: the community layout puts ten 4-byte fields between +0x74 and the class count at +0xA1 (verified), ending at +0x9C. **Confirmed in game** (2026-10-17): the values read were 0.17, 0.50, 1.00, 2.00 — NWN's fractional CRs — with 0 implausible reads. Runtime guard stays: `rules::IsPlausibleChallengeRating`. |
| +0xA1 | class count | GetLevel, GetBaseAttackBonus, CanLevelUp |

`CNWSCreature` +0xC64 is its stats (ResolveAttackRoll, GetLevel); +0x04 is used
as the object id (logging only).

## Attacks per round (decompiled 2026-10-17)

`CNWSCombatRound::InitializeNumberOfAttacks()` 0x73C5A0 (this = combat round,
owner creature at +0x215C):

- Right-hand item (`GetItemInSlot(0x10)`, 0x722510) a crossbow (base item 6/7):
  1 attack, or `GetAttacksPerRound()` if that is above 1 and the creature has
  Rapid Reload (feat 0x19B = 411).
- Otherwise: creature +0x9D0 non-zero → 1 attack; else the stats override at
  +0x412 if non-zero, else `GetAttacksPerRound()`.
- The count goes to combat round **+0x2138** (main-hand attacks).
- 0x73BF80 then runs (likely the off-hand count, +0x213C — unverified).
- Combat mode (creature +0x4AB) 10 (Dirty Fighting): 1 attack, 0 off-hand.
  Modes 5/6 (Flurry of Blows / Rapid Shot) with a valid weapon: +0x2148 = 1
  extra attack, else the mode is cancelled (0x67B0D0). Hasted
  (creature +0x9CC == 1): +0x2148 += 1. Resets +0x2140 / +0x2144.

`GetAttacksPerRound()` 0x693810 (unnamed in the community map), plain `ret`,
only caller 0x73C5A0: monk attack tables (0x695110) → (pre-epic BAB + 2) / 3,
else (pre-epic BAB + 4) / 5, clamped to 1..6. Hooked for player characters.

Iterative penalty in `GetAttackModifierVersus`: `edi` is loaded once
(0x695917, a stack local) and passed to `GetAttack` (0x73C0A0) — the attack's
index in the round. `sub ebx, ecx` at 0x6959F4 (2B D9, −3 × index, monk
tables) and `sub ebx, edx` at 0x695A05 (2B DA, −5 × index) are NOPed.

Class ids (game's `2dasource.zip` classes.2da): 0 Barbarian, 1 Bard,
2 Cleric, 3 Druid, 4 Fighter, 5 Monk, 6 Paladin, 7 Ranger, 8 Rogue,
9 Sorcerer, 10 Wizard; 11–26 and 38 monster/racial classes; 27–37 and 41
prestige classes.

## Retargeting after a kill (decompiled and verified 2026-10-17)

- `CNWSCombatRound::GetWeaponAttackType()` 0x73E510 types each attack by its
  index: 1 main-hand weapon (7 unarmed); 2 off-hand when the round has off-hand
  attacks and the index is past main + extra + cleave; 6 "additional" weapon
  attack (8 unarmed) past the main-hand count once extra/cleave attacks exist;
  3/4/5 creature weapons (random).
- GetAttackModifierVersus: additional attacks take −5 × round +0x2144 (`add
  ebx, ecx` 0x695AD9) unless special 0x363/0x364/0x187; off-hand −5 × +0x2140.
- `CNWSCombatRound::GetTotalAttacks()` 0x73E970 = +0x2138 + +0x213C + +0x2148
  + +0x214C (main, off-hand, extra, cleave).
- ResolvePostMeleeDamage's Great Cleave branch (0x746745..0x746797): see
  nwmain_1_69.h. Setting round +0x2134 to the new enemy sends the round's
  remaining attacks there; AddCleaveAttack (0x73D4B0) adds one more.
- 0.5.0 (answered "has Great Cleave"): 6 attacks in a round, the added ones at
  −5/−10. 0.5.1 (switch the target only, NOP the additional-attack penalty):
  22:41 session — kill on attack 3 of 4, attack 4 went to the next goblin and
  killed it in the same round; 4 attacks total; every attack +6; with no enemy
  left: "no enemy in reach".
- Ranged (hook on ResolvePostRangedDamage): 0.6.0 searched 40 m and picked the
  caged Hook Horror across the arena. 0.6.1 (20 m), 23:17 session, shortbow,
  3 goblins, 3 shots a round: round 1 shot 1 killed one goblin, shots 2–3 went
  to the next and killed it; round 2 shot 2 killed the last one, "no enemy in
  range", and the round stopped there. The Hook Horror was never targeted. A kill
  on a round's last shot logs nothing (no attack left to move).
- 0.6.2 (owner: a bow must not pick idle enemies): GetNearestEnemy decompiled
  (nwmain_1_69.h) — with bMustBeSeen = 1 it already skips enemies the attacker
  hasn't seen, but anything seen counts (the caged Hook Horror was). Ranged
  retargeting now walks the area with the same checks and keeps only enemies
  whose attack order (current or queued action 0xC, the state 0x64B770 turns
  into the client's "attacking" status) targets the attacker, an associate of
  the attacker, or a creature of the attacker's faction. Melee still calls
  GetNearestEnemy. Also found: combat round +0x20EC = 1 while a round runs
  (StartCombatRound 0x73B160), +0x2154 = the round's defender.
- The per-shot ranged modifier varied in that session (5e +2, −2, +0, +0, +0;
  sheet +3). Explained by GetAttackModifierVersus's situational terms
  (decompiled 2026-10-17, each labelled by a debug string, see below): shot 1
  at a charging goblin −2 (moving target), shot 2 at a goblin arriving within
  its reach −6 (moving −2, ranged within melee distance −4), later shots at a
  goblin standing next to the archer −4. Base +4 — still one above the sheet's
  +3 (unexplained). The goblins' ±2 is likely the same family (flanking +2).

## GetAttackModifierVersus situational terms (decompiled 2026-10-17)

After the BAB part (above), each term has a debug label (`[%d] <label>`),
added in this order: off-hand penalty, size, combat-mode modifiers (Power
Attack, Improved Power Attack, Flurry, Rapid Shot, Expertise, Improved
Expertise), special attack, Nature Sense, Epic Prowess, Superior Weapon Focus,
Bane of Enemies, Good Aim, Halfling sling, Enchant Arrow, Offensive Training,
Dex/Str/Wis modifier, Weapon Focus, Epic Weapon Focus, effect attack bonus,
then the situational ones:

| Label | Value | When |
|---|---|---|
| Target Blind / Attacker Invisible | (not read yet) | |
| Can't see my target | −4 | target not in the attacker's visible list with bit 0 (0x687BF0) |
| Moving Target | −2 | ranged only; target +0xAC (animation) 4, 0x56 or 0x57 |
| Target Prone | melee +4 / ranged −4 | target +0xAC 0x05, 0x07, 0x20, 0x21, 0x24 |
| Target Stunned | +2 | target +0x9DD == 6 |
| Target Flanked | +2 | 0x749E50(target) == 1; ranged only within 10 m |
| Ranged Attack Within Melee Distance | −4 | ranged, no Point Blank Shot (feat 27), distance ≤ MaxAttackRange from the target's side, and 0x6499A0 == 0 |
| Point Blank Shot Attack Bonus | +1 | ranged, has feat 27, within 5 m |
| Enemy Attack Bonus | target stats +0x413 (int8) | always |

2024 equivalents (Phase 1b): ranged in close combat, unseen target and prone
at range → Disadvantage; prone within 5 ft, stunned and (optional) flanking →
Advantage; moving target has none.

## Damage roll and critical damage (decompiled 2026-10-17)

`CNWSCreatureStats::GetDamageRoll(defender, bOffHand, bCritical, bSneakAttack,
bDeathAttack, bForceMax)` 0x69AEB0, `ret 0x18`:

- Weapon dice come from the base item (`NumDice` +0x1A, `DieToRoll` +0x1B,
  via 0x4BC890); unarmed uses 0x6957D0 (monk/unarmed dice 0x695190/0x695320).
  Dice are rolled with `RollDice(count, sides)` 0x47E120 (`this` = the rules
  object at [0x92DC64]; the attack roll calls it as (1, 20)).
- On a critical each dice loop runs `GetCriticalHitMultiplier()` times,
  re-rolling the dice each time (the 3e "x2" is two separate rolls, already the
  2024 behaviour with x2).
- Flat bonuses: `0x697D10` returns the sum of Strength / two-handed /
  off-hand / special-attack bonuses unmultiplied (its multiplier calls only
  build debug strings like "Strength Bonus (Critical x%d)"); GetDamageRoll
  keeps it in ebp and adds it once per x in the loop 0x69BEA7–0x69BEC3
  (`add [esp+10h], ebp`). 0.4.0 answers multiplier 1 to those two calls.
- Effect damage bonuses: `CNWSCreature::GetTotalEffectBonus` 0x65E7D0 (type 2),
  on attack result 3/10: dice effects re-rolled per x, flat effects added per
  x through the same calls — so flat effect bonuses still double (known gap).

## In-game observations (2026-10-17, attack/AC hooks, 0.2.0)

Prelude fight, `nwn5e.log`: the PC (object 0x7FFFFFFF — NWN numbers player
objects down from there) level 1: +5 → +6 and +3 → +4 (BAB 1 → PB 2);
creatures level 1 CR 0.5: +3 → +4, +4 → +5; creature AC: CR 1 10 → 13,
CR 0.17 10 → 13, CR 2 14 → 13. Threat 21 / multiplier x0 came only in pairs
at the same instant, outside attacks — the empty off-hand during the combat
information update; 0.2.1 leaves them unchanged.

## In-game observations (2026-10-17, Extra Attack, 0.3.1 attack_trace)

`[debug] attacks_per_round=3`, `attack_trace=1`, plus the game's own chat
log (`ClientEntireChatWindowLogging=1` in **nwnplayer.ini** `[Game Options]`
— not nwn.ini — writes `logs\nwclientLog1.txt` with `[CHAT WINDOW TEXT]
[timestamp]` lines).

- Every round is set up with main 3 (`GetAttacksPerRound` hook reaches
  combat round +0x2138).
- Against a target that survives (21:20 session) three attack rolls ran in one
  round, indices 0, 1, 2.
- Against Weak Goblins that die to the first hit (21:24 session), each round
  had index 0 (the kill) and index 1, which the chat log labels "Attack Of
  Opportunity" against the next goblin; at that moment +0x2148 had become 1.
  So an attack of opportunity is resolved inside the round as an extra attack,
  and the round's remaining main-hand attacks are not used once the target
  dies.

**Confirmed (21:31 session, 0.3.2, `creature_ac=40` so attacks miss):** eight
rounds in a row, each set up with main 3, each with attack indices 0, 1, 2 —
and the game's chat log shows three "Unen Amakiir attacks Weak Goblin" lines per
round at the same seconds, every one at +6 (no iterative penalty). Rounds were
set up 6.0–7.0 s apart (mean ≈ 6.5 s); the three attacks ~2 s apart. A goblin's
natural 20 against the PC came out as a plain *hit* with a threat roll shown —
probably the game-difficulty check in ResolveAttackRoll's crit path (0x600100),
not confirmed.

## Open

- Character sheet shows the 3e attack bonus (+5) while attack rolls use the
  converted +6 (owner, 2026-10-17): the sheet's numbers come from another
  path — likely 0x693890 (combat information) — not GetAttackModifierVersus.
- Name 0x693890 (likely the combat information update) to confirm the
  empty-hand reading of threat 21 / x0. Multiplier callers now named:
  0x65E7D0 GetTotalEffectBonus, 0x697D10 flat damage bonuses, 0x69AEB0
  GetDamageRoll.
- Verify the predicted and ambiguous map entries with REA decompilation
  before using any of them.
- The threat roll is still rolled and shown in the combat log.
