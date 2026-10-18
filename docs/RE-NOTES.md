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
| Target Blind | +2 | GetBlind(target) 0x6621D0 (target +0x9DC bits: none of 0x24, and 0x10, or 0x08 without 0x02), unless melee and the target has Blind-Fight (0x198) |
| Attacker Invisible | +2 | attacker->GetInvisible(attacker->vtable[0x10](), 0) 0x6621F0 (ret 8), same Blind-Fight exception |
| Can't see my target | −4 | target not in the attacker's visible list with bit 0 (0x687BF0) |
| Moving Target | −2 | ranged only; target +0xAC (animation) 4, 0x56 or 0x57 |
| Target Prone | melee +4 / ranged −4 | target +0xAC 0x05, 0x07, 0x20, 0x21, 0x24 |
| Target Stunned | +2 | target +0x9DD == 6 |
| Target Flanked | +2 | 0x749E50(ecx attacker, target) == 1 — the target's attack-target fields (+0x4E0/+0x4E4/+0x4F8/+0x4FC) point at someone else; ranged only when centre distance² ≤ 100 |
| Ranged Attack Within Melee Distance | −4 | ranged, no Point Blank Shot (feat 27), centre distance² ≤ MaxAttackRange(target, attacker id, 0, 0)², and the target does not wield a ranged weapon (0x6499A0: right-hand base item +0x18) |
| Point Blank Shot Attack Bonus | +1 | ranged, has feat 27, within 5 m |
| Enemy Attack Bonus | target stats +0x413 (int8) | always |

"Ranged" is the current attack data's +0x34 (the function's `[esp+14h]`,
loaded at 0x695938); target = the argument (null for doors/placeables: the
whole block is skipped); attacker = stats +0x24. Creature +0x9DD == 5 makes
AddAttackActions refuse to act (feedback 7).

0.7.0 (Phase 1b, PLAN.md B1–B7): the ResolveAttackRoll hook evaluates these
checks before the d20 (rolled at 0x744401, before the modifier), the d20 call
is redirected to roll two dice and keep one, and the GetAttackModifierVersus
hook removes their 3e total. Close combat (B5) is the 2024 rule: an enemy
within 5 ft edge to edge that sees the attacker and is not dead, dying or in
action state 5/6.

## Saving throws (decompiled 2026-10-18)

- Save roll 0x749230 (CNWSCreature, `this` = the saving creature):
  (save 1 Fort / 2 Reflex / 3 Will, DC, save type, versus object, …).
  Value = GetFort/Ref/WillSavingThrow(0); bonus = GetTotalEffectBonus(3, …,
  save, type) + versus-specific effects + feat bonuses (+2s by spell school
  and save type, +1s), capped at +20; d20 = RollDice(1, 20). Types 1/2 can
  return 2 (immune, 0x6B0090 / feats). Feedback CC message: DC, bonus, roll,
  save, type. Natural 1 fails only when a server option ([[0x92DC50]+4]
  vtable +0x54 → +0x108) is set; natural 20 always succeeds; else success
  when DC ≤ d20 + bonus + value. Mind-spell failures may reroll once
  (feat, marker 0x103).
- GetFortSavingThrow(bBaseOnly) 0x6AD4F0: effects (type 3, save 1) unless
  base-only + Luck of Heroes (feat 0x17E = 382, +1) + Dark Blessing (feat 0x1D9 = 473, Blackguard,
  Cha mod) + Con mod (stats +0x3FD) + stats +0x4C4 (creature blueprint
  bonus) + GetBaseFortSavingThrow 0x6A26D0 (class tables). Reflex 0x6AD630
  and Will 0x6AD590 mirror it (Dex / Wis).
- Ability modifiers are bytes at stats +0x3F9 Str, +0x3FB Dex, +0x3FD Con,
  +0x3FF Int, +0x401 Wis, +0x403 Cha (Str, Wis and Cha confirmed in code;
  the rest by the 2-byte stride).
- Callers of GetFortSavingThrow: 0x612430 (script getter, likely), 0x62BD30
  (character sheet), 0x749230.
- Spell DC 0x6659D0 (CNWSCreature, spell id): 10 + level + ability mod +
  Spell Focus (school = spells row +0x20; feats 0x23/0xA6.. +2, 0x189.. +4,
  0x262.. +6) − a class term (0x6995B0). Casting class slot = creature
  +0x19C (−1 none: item +0x1AC or innate level with modifier 3; −2 innate,
  Cha); level from the class column (0x4F8350) or innate (row +0x47);
  creature +0xA70 set → innate level, modifier 3 (items: NWN's 13 + level).
- Base saves: class tables + epic + Divine Grace (217) + Great Fortitude 14
  / Lightning Reflexes 24 / Iron Will 22 (+2) + Lucky 248 (+1) + per-save
  feats (Fort 388 +1, 583 +4; Reflex 386 +1, 585 +4; Will 380 +1, 388 +1,
  695 +4). The getters add the ability (Con byte / GetDexMod(0) / Wis
  byte), Luck of Heroes 382, Dark Blessing 473 and the blueprint byte.
- 0.8.0 (`saves`): the three getters return rules::SaveValue (effects =
  getter(0) − getter(1)); CalculateSpellSaveDC returns the 2024 DC and
  remembers (caster, DC); the save roll converts any DC not remembered for
  its versus object (C6), its d20 (0x7499C9) rolls species advantage and
  Halfling Luck, and in faithful mode the natural-1 / natural-20 branches
  are patched (0x749CE1 jnz→jmp, 0x749D00 jz NOPed). Quick launch: all
  installed. 0.8.1 (C7, faithful): the 34 feat-bonus HasFeat calls before
  the save d20 (nwmain_1_69.h kSaveRollFeatBonusCalls) call a stub that
  answers "no feat"; quick launch: 34 of 34 patched.

## Two-weapon fighting and attacks of opportunity (decompiled 2026-10-18)

- Off-hand attacks: CNWSCombatRound 0x73BF80 → round +0x213C. 1 when the
  right hand holds a non-ranged weapon no bigger than the creature (base
  item +0x19 ≤ creature size +0x4D4) and the left hand (slot 0x20) a weapon
  that is not a shield (base item +9 set, +8 ≠ 7), or the right hand a
  double weapon (+8 == 8); 2 with Improved Two-Weapon Fighting (feat 20).
- Penalty (GetAttackModifierVersus "Offhand Attack Penalty", attack type 1
  main / 2 off-hand, only when dual-wielding): main hand −6 (−4 when the
  off-hand weapon is light — GetRelativeWeaponSize 0x65A390 < 0 — or a
  double weapon), +2 with a feat; off-hand −10 (−8 light/double), +4 and +2
  with feats (Ambidexterity 1, Two-Weapon Fighting 41). Off-hand damage adds
  half Strength ("Offhand Strength Bonus", 0x697D10).
- Attacks of opportunity: combat round +0x2128 = AoOs left this round
  ("NumAOOs" in the save, 0x73E9B0), reset to 1 when combat ends (0x73B7A0,
  with the cleave allowance +0x212C). Broadcast 0x655E70 (`this` = the
  provoker; target or invalid = everyone near): each creature that sees the
  provoker, is hostile, has an AoO left, is not busy/flat-footed/dead,
  wields no ranged weapon and is within its melee reach (+1.5 m) gets
  AddAttackActions(provoker) + 0x73DD30 (the AoO attack) and +0x2128 − 1;
  with the second argument 1 the provoker first makes a Tumble check
  (skill 21) — feedback 0xA87. Provoked by moving (0x6475B0, the move
  action; Spring Attack 392 skips it) and by casting (0x67B550 /
  0x67C070; Epic Improved Combat Casting 696 skips it). Ranged attacks and
  item use do not call it.

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
  x through the same calls. Decompiled 2026-10-18: each entry is (dice count
  [esp+0x2C], die size or constant [esp+0x1C]) from two 2DA lookups
  (0x5CEDC0); four blocks, each with three GetCriticalHitMultiplier calls
  (bForceMax product, loop start, loop test), every one after a single push.
  0.6.4 answers 1 at those 12 sites when the entry's dice count is 0, read
  from the caller's stack (nwmain_1_69.h); sites verified in game at load.

## In-game observations (2026-10-18, 0.7.1, bow vs 3 goblins, then melee)

- Ranged retargeting at 40 m: a kill on shot 1 of 3 moved the round to the
  goblin 0.4 m away ("in the fight", attack order on the PC, faction 20 =
  ours); object 0xEB at 30.9 m (the caged Hook Horror) had no attack order
  and was skipped. After the last kill: "1 seen enemies within range, none in
  the fight". Round order: goblins charge at range, then three adjacent.
- Advantage/disadvantage: a charging goblin's −2 (moving) removed → normal
  roll at +4; with goblins adjacent every shot had disadvantage
  ("close-combat enemies 1–3"), the combat log showing the kept die (dice
  10 / 5 → "(5 + 4 = 9)", 1 / 10 → a natural 1). Goblins' flanking +2
  removed. One goblin swing had "target unseen" (the engine's own −4: its
  visible list lacked the PC for a moment) → disadvantage.
- A goblin's natural 20 on the PC: "*critical hit* : (20 + 0 = 20)", 3
  damage, no Threat Roll in the line; "crit damage: flat bonuses x2 -> x1".
- The PC's normal ranged attack is +4 while the sheet shows +3 — still
  unexplained. The sheet (GetRangedAttackBonus 0x694B00: effect attack bonus
  with no target + BAB + Dex/Zen Archery Wis + epic ladder + Weapon Focus +
  Epic Weapon Focus + Good Aim (thrown) + halfling sling) has no target,
  while GetAttackModifierVersus adds target-dependent and other terms, e.g.
  "Offensive Training" (+1 when the attacker has Battle Training vs Orcs
  231 / Goblins 232 / Reptilians 242 against race 14 / 12 / 15,
  0x696B05..0x696B77) and size (creature +0x4D4: 1/2/4/5 → +2/+1/−1/−2).
  First blamed on Battle Training — withdrawn: Unen Amakiir is an elf (race
  1 in player.bic), and the 11:00 session shows melee at +6 against the
  sheet's +5 against a human mage too, so the extra +1 applies to every
  attack. Melee reads Str (stats +0x3F9) or, with Weapon Finesse (0x6A5B50),
  Dex; ranged reads Dex (0x6A1430(0)) or, with Zen Archery (0x19C), Wis
  (+0x401) — the same getter and argument as the sheet. Next step: one fight
  with [debug] combat_breakdown=1 (the engine's own per-term labels).

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
- The threat roll is still rolled; 0.6.5 stops sending it. The combat log's
  attack line comes from SendServerToPlayerCCMessage 0x62F850 cases 5/6
  (modifier +0x10 8 bits, to-hit roll +0x0E 5 bits, result +0x50, …,
  critical-threat bool +0x48, then the threat roll +0x0F 5 bits only when
  that bool is 1); the client formats "%s %s %d = %d" (0x903564). Found by
  scanning for 5-bit writes (`push 5` before the bit writer 0x4FBD80).
  Patches applied in game at load; the log itself awaits a crit.
- A natural 20 against the player came out as a plain hit (21:31 session):
  in ResolveAttackRoll's crit path, `0x600100(0) == 1` (→ 0x610D40 →
  0x47E260(option 0, game difficulty at server options +0x170)) with a
  player-controlled target and the attacker not player-controlled sets
  result 1 (hit). 0.7.1 skips it (jnz → jmp at 0x744746, decision B7).
