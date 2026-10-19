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
- Details for D1 (2026-10-18): the off-hand count function 0x73BF80 has one
  caller, InitializeNumberOfAttacks (0x73C651), which reads +0x213C. The
  penalty is built twice: in GetAttackModifierVersus (0x695C06–0x695CE8, a
  local that stays 0 when skipped) and again in the sheet's
  GetMeleeAttackBonus (0x69485F–0x69491D, in ebx). 0x697D10 is the damage
  bonus (target or 0, bOffHand; ret 8), called once, by GetDamageRoll
  (0x69BE8B); the off-hand Strength term is trunc(Str mod × 0.5) when
  positive (float 0x8A6930), Str mod otherwise. The same function gives
  two-handed weapons (weapon size = creature size + 1, not double) Str mod ×
  1.5 ("Two Handed Strength Bonus") and Point Blank Shot (27) +1 damage
  within 5 m. Faithful D1 patches and hooks: nwmain_1_69.h.
- Attacks of opportunity: combat round +0x2128 = AoOs left this round
  ("NumAOOs" in the save, 0x73E9B0), reset to 1 at the end of **every
  round**: 0x73B7A0 is EndCombatRound (clears the 50 attack slots, resets
  the round state, sets +0x2128 and the cleave allowance +0x212C to 1), and
  its one caller, UpdateCombatRound 0x73B8F0, runs it whenever the round
  timer (+0x20F4) passes the round length (+0x20F8). So NWN already allows
  one AoO per round per creature (corrected 2026-10-18; first read as "per
  fight"). NWN 1.69 has no Combat Reflexes feat (not in feat.2da), and
  nothing else writes +0x2128. Broadcast 0x655E70 (`this` = the
  provoker; target or invalid = everyone near): each creature that sees the
  provoker, is hostile, has an AoO left, is not busy/flat-footed/dead,
  wields no ranged weapon and is within its melee reach (+1.5 m) gets
  AddAttackActions(provoker) + 0x73DD30 (the AoO attack) and +0x2128 − 1;
  with the second argument 1 the provoker first makes a Tumble check
  (skill 21) — feedback 0xA87. Provoked by moving (0x6475B0, the move
  action: only while the mover's animation +0xAC is 4, 0x56 or 0x57 —
  apparently running, as walking away drew none in tests while running in
  circles drew four (2026-10-18) — at most once per 1.5 s (+0x508), not
  while the mover's own AoO cooldown +0x50C is set; the taker must also
  roughly face the mover; Spring Attack 392 skips it) by drinking a potion (0x67B550, see
  below) and by casting (0x67C070; Epic Improved Combat Casting 696 skips
  it). The fifth caller,
  0x64A364, is the creature event handler 0x64A220 (vtable 0x8B5F88) for
  event 20, EVENT_BROADCAST_AOO (id at 0x8B6484; data = the AoO target, or
  none = everyone). It is queued with AddEventDeltaTime 0x6BF770 by:
  - 0x7426B0, which schedules a ranged round's events: once per shot,
    `this` provokes everyone near. Skipped only when difficulty option 1
    (0x600100(1) → byte [rules + 0xF8 + difficulty·7 + 1], the same table
    as the crits-on-players option 0) is set and the shooter is
    player-controlled (+0xAD8) or a possessed familiar (0x68E510). So
    ranged attacks do provoke (earlier note "ranged attacks do not call
    it" was wrong).
  - 0x743F30, the melee counterpart: special attack 9 (Disarm) provokes
    the defender; an attack with no weapon (GetCurrentAttackWeapon 0) by a
    creature without Improved Unarmed Strike (21) provokes unless the
    special attack is 65002.
  The second broadcast caller, 0x67B550, is the item-use action, not a cast:
  it provokes for potions (base items 49 and 104; 54 is a deleted row).
  Everyone provokes except the party when difficulty option 2 is set
  (dialog.tlk: "No attacks of opportunity on players using potions"); the
  others jump straight to the call (0x67B9B0 / 0x67B9C6 → 0x67B9DC). The
  faithful patches (D2) are listed in nwmain_1_69.h
  (`kFaithfulOpportunityPatches`); all seven apply (0.8.3, potions as a
  call-site patch in 0.8.4).
- Difficulty table options (0x600100(n) → byte [rules + 0xF8 + difficulty·7
  + n]): 0 = no critical hits on PCs (B7), 1 = no AoO on players using
  ranged weapons, 2 = no AoO on players using potions. dialog.tlk 67577–67581
  (the difficulty descriptions) list all three for Very Easy, Easy and
  Normal and none for D&D Hardcore and Very Difficult.

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

## Local variables, rests and server objects (decompiled 2026-10-18, D3)

- Every object keeps its script local variables at object +0xD8
  (LoadVarTable 0x639BD0 adds 0xD8); they are saved with the game.
  CNWSScriptVarTable::GetInt(CExoString*) 0x74E7B0, ret 4, 0 when missing;
  SetInt(CExoString*, value, flag) 0x74E970, ret 0xC — MatchIndex 0x74E640
  (name, type 1 = int, create) copies the name into a new entry. Names
  compare with strcmp (0x5BA5E0, case-sensitive). A CExoString is {char*,
  buffer length including the null} (constructor 0x5BA260).
- Resting: CNWSCreature::Rest 0x659C50 only starts a rest (it refuses with
  an enemy within 30 m); the rest action AIActionRest 0x6883E0 readies
  spells and calls CNWSCreatureStats::ResetFeatRemainingUses 0x6A4B10 (plain
  ret) when the rest completes, as does the script command ForceRest
  (0x7103D0, call at 0x710467).
- [0x92DC50] is the app manager and [+4] the CServerExoApp; its methods at
  0x5FFxxx forward to the CServerExoAppInternal at [app + 4]
  (GetCreatureByGameObjectID 0x5FFAD0 → 0x609C50, GetPlayerIDByGameObjectID
  0x5FFB70 → 0x6108B0). GetNWSMessage 0x5FFBD0 = [[app + 4] + 0x10018]. Until
  0.8.7 the chat code called the internal 0x6108B0 with the app pointer.
- GetWeaponFocus 0x6A5C20 maps an item's base item to its Weapon Focus feat
  (no item or gloves 36/78 → 100 unarmed; creature weapons 69–72 → 291) and
  returns HasFeat; the table is in src/rules/weapons.cpp.

## Engine effects (decompiled 2026-10-18, D4)

- The NWScript command table is filled at runtime by CNWVirtualMachineCommands
  (writes `mov [reg + index*4], handler`, e.g. ApplyEffectToObject = command
  220 at +0x370 -> 0x6F50A0). EffectDamage (79) 0x6EC4B0, EffectMovement
  SpeedDecrease (451) 0x6F1C50; EffectKnockdown (134), EffectEntangle (130)
  and EffectDeath (133) share 0x6F0610 (switch on the command). Ghidra did not
  define these as functions (no static callers); read them with llvm-objdump.
- Recipes, the CGameEffect layout and the apply path (CNWSObject::ApplyEffect
  0x63D4A0, or EVENT_APPLY_EFFECT 5 via AddEventDeltaTime as the engine does
  for a hit) are in nwmain_1_69.h "Engine effects"; src/core/engine_effects.cpp
  builds damage, knockdown and movement-speed-decrease effects that way.
- A melee hit's damage effect (0x7440DA in the melee event scheduler 0x743F30):
  type 38, 20 integers, 0-12 copied from attack data +0x16 (int16 per damage
  type), 13 = 1, 14 = attack data +2 / 2, 17 = 1, applied as EVENT_APPLY_EFFECT
  at the hit time.

## The spell behind a save (decompiled 2026-10-18, C1)

- FortitudeSave / ReflexSave / WillSave (script commands 108-110) share
  0x6EE070: (command, parameters), ret 8; versus defaults to OBJECT_SELF
  (commands +0x08); it calls SavingThrowRoll(1/2/3, DC, type, versus, 1, 0, 0)
  on the target.
- GetSpellId (248) 0x6F6AA0: OBJECT_SELF via CServerExoApp::GetObjectArray
  0x5FFA90 + CGameObjectArray::GetGameObject 0x42E730 (0 = found), vtable +0x10
  (as CNWSObject), field +0x188. An area-of-effect object (vtable +0x54, as
  in GetAreaOfEffectCreator 0x6F83D0) keeps its spell at +0x1CC ("SpellId" in
  its load code at 0x756406; "AreaEffectId" at +0x1C4).
- Persistent areas of effect pass themselves as the save's opponent, so the
  C6 "is this a remembered spell DC for the caster" check missed their DCs and
  converted them again; from 0.9.8 any save asked for by a spell script keeps
  its DC.
- spells.2da: the expansion's copy (xp2patch.bif, 840 rows) overrides the
  original (417 rows); Sunburst 427, Banishment 430, the Inflict spells
  431-435 and Bane 449 are only in the former.

## Condition effects (decoded 2026-10-19, for CN8)

- Effect apply/remove tables are filled at 0x667B90 (apply at handler +4,
  remove at +8, by effect type x 4). SetState 8 -> 0x66DD30, SetState
  internal 9 -> 0x66EA00, Deaf 21 -> 0x66D770, spell failure 25 -> 0x66DB70.
- SetState states (from their immunity checks, IMMUNITY_TYPE_*): 1 charmed
  (14), 2 confused (16), 3 frightened (4), 4 turned (no check), 5 dazed (18),
  6 stunned (12), 7 dominated (15; 21 shares its case), 8 paralysed (6),
  9 sleep (13), 15 petrified (no check), 20 paralysis without icon.
  Every state but dazed also clears AI-state bit 0x100; the highest state
  number held is kept at creature +0x9DD and re-applied as SetState
  internal (type 9). Sub-effects per state (0x66EA00, jump table 0x66F3D4,
  byte index 0x66F404): icon (type 0x43, effecticons.2da row) plus
  - charmed: icon 12; personal reputation toward the creator set to 100
    (0x687C30), current target cleared (0x6427E0).
  - dominated: icon 15; becomes the creator's associate, type 5
    (0x68B030); target cleared.
  - confused: icon 13; actions cleared (0x63A650); the behaviour is AI.
  - dazed: icon 17; actions cleared; an extra type-0x3B effect whose
    handler (0x673990) sets creature +0x9D8 = 1 (not decoded further).
  - stunned: icon 18; AI mask clears 0x2; visual 208.
  - paralysed: icon 16; AI mask clears 0x0E.
  - petrified: icon 109; AI mask clears 0x0E (same as paralysis); visuals
    351 VFX_DUR_PETRIFY + 352 FREEZE_ANIMATIONS. No damage resistance.
  - sleep: icon 19; AI mask clears 0x15E.
  - frightened / turned: saving throw -2 (type 0x1B) and icon 14 / 24.
- Deaf: AI mask clears 0x10 (hearing), spell failure +20 (type 25) and
  icon 7; nothing else. The type-25 amount goes to stats +0x42C, which is
  summed with armour (+0x42D) and shield (+0x42E) failure (0x656740) and
  rolled at casting (0x67D664) only for Bard (1), Sorcerer (9) and Wizard
  (10) spells, and not with metamagic 0x20 (Still Spell). So NWN's deafness
  never fails a divine spell, and Still Spell (not Silent) avoids it.
- Difficulty texts (dialog.tlk 67577-67581): charm and domination on a
  player daze them at every difficulty; confusion dazes a player only on
  DM Easy. Flesh to Stone (3045): 1 round / level on Normal or easier,
  permanent above; the caster resting restores.

## Push: fall animation and short moves (decoded 2026-10-19, D4 stage 4 recon)

- Knockdown (OnApplyKnockdown 0x66CE80; remove 0x66D220): immunity type 28,
  pauses the target's combat round with no limit (SetRoundPaused 0x73B9E0(1,
  attacker), SetPauseTimer 0x73BA60(0, infinite=1)), an AI-state effect
  clearing 0x100 for duration + 1.5 s, ClearAllActions (0x63A650, cancels a
  spell), then posture byte +0x480 = 5 / animation 7 when the target faces the
  attacker (dot >= 0.707), else posture 6 / animation 5. Removal unpauses and
  0x655C80 resets posture from state (0 standing, 1 seated, 2 dazed/stunned,
  4 dead/asleep) and SetAnimation(0).
- CNWSCreature::SetAnimation = vtable +0x80 = 0x649600 (thiscall, ret 4;
  vtable 0x8B5F0C): idle 0/1 resolves from the posture byte (5 -> 7, 6 -> 5),
  so setting the posture byte keeps a creature lying down; the value goes to
  +0xAC unless seated (+0xA7C). PlayAnimation (0x6E9CC0) has no constant for
  engine animations 5/7.
- The engine's hit flinch (in OnApplyDamage, 0x668C0C-0x668C61) is the
  template for a timed animation: GetActionResponsePossible 0x73BC90(attacker,
  ms), SetRoundPaused(1, attacker), SetPauseTimer(ms, 0), DecrementRoundLength
  0x73BAF0(ms, 0) (the round loses no attacks), SetAnimation(14). Each AI
  update 0x64B4E0 counts a finite pause down by +0xA8 ms and on expiry
  unpauses and SetAnimation(1) ("Animation Reset"), for any animation except
  2/3/4/0x4E/0x4F/0x54-0x57. SetPauseTimer adds to a running timer (+0x210C).
- Moves: JumpToLocation (313) / ActionJumpToLocation (214) share 0x6E7250 and
  queue action 5, run by AIActionJumpToPoint 0x680D00 (SetAnimation(1),
  ComputeSafeLocation radius 20, SetPosition, 0x6833F0, SetOrientation
  0x63D2B0, +0xB2C = 1, associates jump too). Callable pieces:
  CNWSArea::ComputeSafeLocation 0x6DA0C0 (Vector, maxDist, pathfind info =
  creature +0x304, straight-line flag, Vector* out; ret 0x1C);
  NoNonWalkPolysOnLine 0x6DF020 (x1, y1, x2, y2, -0.1f; 1 = walkable line;
  ret 0x14); ComputeHeight 0x6D9730 (-6.0 off the walkmesh; ret 0xC); creature
  footprint fits 0x6D9890 (x, y, z, pathfind; ret 0x10); CNWSObject::
  SetPosition 0x63D2D0 (Vector, force; 0 also updates the area grid; ret
  0x10); trigger enter/exit at a new spot 0x6833F0 (Vector, area id; ret 0x10);
  GetArea 0x63ECC0. Object position +0x78, facing +0x84, area id +0x74.
  Other creatures are not checked (overlap possible).
- Size: creature +0x4D4 (GetCreatureSize, handler 0x7020F0): 1 tiny .. 4
  large, 5 huge.
- Not decoded: whether the client slides or snaps an NPC moved 3 m
  server-side, and whether a creature's animation / round pause is saved.

## Conditions: hook points (decoded 2026-10-19, CN1-CN6 build)

- Applied effects: CNWSObject +0xE8 CGameEffect* array, +0xEC count, sorted by
  type (engine loops stop past the wanted type). Effect: type +8 (uint16),
  flags +0xA, creator +0x18, integer count +0x34, int32 array +0x38
  (GetInteger 0x666A80 has no bounds check). AI-state word +0xB08 (SET_AI_STATE
  0x66D680 ANDs it); +0x9DC vision flags (GetBlind 0x6621D0); +0xC0 is the
  commandable flag (set around ClearAllActions; inferred).
- Entangle (0x66CB20): immunity type 10, clears actions, attack -2 vs all
  (type 0x0B, int0 2 at 0x66CD00, race 0x1C), Dexterity -4 (type 0x25, at
  0x66CD56), AI bit 0x2 cleared (can't move), icon 5. Web and Entangle use it;
  Evard's Black Tentacles (nw_s0_evardsa/c) applies EffectParalyze for a round.
- Attack modifier: the only state term is creature +0x9DD == 6 (stunned, +2).
  Flat-footed (0x749D70): stunned, or AI bits 0x2/0x4 cleared (paralysed,
  asleep, petrified). Coup de grace (0x745560): sleeping or paralysed target
  of total level <= 4, not a PC, not immune, melee or within 10 m -> attack
  data +0x44 = 1 (patch 0x74565A 01 -> 00 to drop it); ResolveAttackRoll then
  makes it result 7 (automatic hit, d20 shown 20) and damage sets +0x40.
- Attack result (data +0x50): 1 hit, 2 parried, 3 critical, 4 miss, 7
  automatic hit, 8 target concealed, 9 attacker miss chance. Damage reads
  +0x50 == 3 (ResolveDamage 0x745670 -> 0x69AEB0), so 1 -> 3 after
  ResolveAttackRoll gives critical damage. Critical checks (0x74477B):
  difficulty rule (0x600100(0) == 1, target PC +0xAD8 / 0x68E510, attacker
  not), GetIsImmune 0x6B0090 (stats, type 31, versus creature; ret 8),
  HasFeat(0x380).
- Saves: every Reflex save goes through 0x749230 (also GetReflexAdjustedDamage
  0x6F9FAF, poison and disease saves). GetReflexAdjustedDamage is command 299,
  handler 0x6F9DC0 (table slot +0x4AC, written at 0x6E198C), separate from the
  Fortitude/Reflex/WillSave handler 0x6EE070; Evasion 0xCE / Improved Evasion
  0xD4 adjust its damage after the roll whatever the save ability.
- Save feedback ("Weak Goblin : Reflex Save vs. Cold : *failure* : (12 + 0 =
  12 vs. DC: 17)"): 0x749230 fills a CNWCCMessageData with ints DC, bonus,
  d20, save kind (1/2/3), vs type, feat; the writer at 0x62FDDA sends them in
  7/8/5/.../8 bits (the kind in 8). The client reads the kind at 0x45E8E9 and
  picks the label at 0x45E9ED (1 -> 520, 2 -> 519, 3 -> 745, else none), so the
  label is looked up when the line is shown, after the roll has returned. Order: bonus, d20 (0x7499C9), immunity
  by save type (returns 2 before feedback), feedback (DC, total, roll), then
  natural 1 / 20 / total >= DC.
- Poison (0x670590): poison.2da, Fortitude save (save 1, type 2) at 0x67095C;
  success or immune -> not kept; failure -> ability damage, icon 20, the type
  35 effect stays on the creature.
- Fear (state 3): AIUpdate 0x647BF0 runs statescripts.2da SCRIPTNAME[state]
  each AI tick (6-7.2 s) for states 1, 2, 3, 4, 6, 8, 9 and makes those not
  commandable (+0xC0 = 0; 0x647DA4 74 14 skips it); switch byte table 0x64819C
  (index state-1; 0 run the script, 1 skip). Row 3 / 4 is NW_G0_fear
  (ActionMoveAwayFromObject from enemies within 5 m). Case 3 also gives saves
  -2. x0_inc_generic bkEvaluationSanityCheck aborts DetermineCombatRound while
  the creature is frightened (EFFECT_TYPE 25), paralysed, stunned, asleep or
  dazed, so engine patches alone leave frightened monsters standing idle.
  Options: recompile the AI includes; hide FRIGHTENED from GetEffectType while
  AI scripts run; never let state 3 take hold and track fear ourselves; or
  accept idle monsters.
- Concealment / miss chance (0x746EF0, before the hit decision): attacker
  miss chance = highest type-75 effect (int1 1 all / 2 ranged / 3 melee);
  target concealment = highest of type-47 invisibility the attacker can't see
  (50%), type-76 concealment (int0 %, int1 race, int2/int3 alignment, int4 0
  all / 2 ranged / 3 melee) and Epic Self-Concealment feats 0x2EC-0x2F0;
  d100 with a Blind-Fight reroll -> result 8 / 9. The only entry is the gate
  at 0x7472CC (jmp 0x7473E1 skips it).
- Confusion (NW_G0_confuse), d10 per AI tick: 1 wander, 2-6 nothing, 7-10
  attack the nearest creature (any side); 2024 has 9-10 act normally. Charm
  (nw_g0_charm) runs DetermineCombatRound; reputation picks targets.

## Push and fear: calls as used (0.9.13)

- Knockdown fall direction (0x66D0CE-0x66D1E1): d = normalize(attacker
  position - target position), f = normalize(target facing +0x84); dot(d, f)
  >= 0.707 (double at 0x8A9668) -> posture 5 / animation 7, else posture 6 /
  animation 5; no attacker object -> 5. Knockdown duration + 1.5 s (float at
  0x8ADA34) for its AI-state effect.
- Hit flinch (0x668BF9): only when the target's animation (+0xAC) is 0 or 1;
  the round calls take the damage effect's creator id (+0x18) as "attacker".
  Round: +0x2104 paused, +0x2108 paused by, +0x210C timer;
  GetActionResponsePossible refuses when paused by someone else.
- NoNonWalkPolysOnLine 0x6DF020 takes (x1, y1, x2, y2, tolerance): arguments
  1/3 and 2/4 are compared pairwise, argument 2 picks the tile row (times the
  area width, +0x8; height +0xC), tiles 10 m (float at 0x8ACC18).
- Creature footprint check 0x6D9890 (area; Vector, pathfind info; ret 0x10)
  returns nonzero when the creature fits (0x6635D3) — not used by Push, which
  keeps 0.5 m of walkable line behind the new spot instead.
- Same-area jump (0x680FDA): SetPosition(Vector, 0), then 0x6833F0(Vector,
  [area + 0xC8]).
- CN5: OnApplySetState 0x66DD30 is called (handler, object, effect, loading)
  and returns 1 to drop the effect; nwn5e drops state 3 in faithful after
  NWN's own refusals and applies an ICON effect (type 67, integer 0 = 14).

## Push v2: placeables, per-tick moves, retargeting (decoded 2026-10-19)

- Placeable walkmeshes (.pwk, type 0x805) load through GetWalkMesh 0x4AD510
  (lazy, 0x5836E0); server placeable code (0x733E8A) registers each one's box
  with the area (CNWArea) through 0x53EB90 — static objects (count +0x48,
  0x18-byte boxes +0x4C, ids +0x50), not tile walkmesh. ComputeHeight,
  NoNonWalkPolysOnLine and the creature-fits check 0x6D9890 (tile surface
  mesh via GetTile 0x725B10; tiles at area +0x11C, 0x58 bytes) don't see them.
- Static-object line test 0x53FBF0 (thiscall on the area, ret 0x30): (x1,
  y1, x2, y2, zA, zB, radius, exclude, uint32* blocker, flag, 0, 0); 1 clear,
  0 blocked with the blocker id written (argument 12 = 0); a per-object
  virtual line-vs-mesh test (vtable +0x10). ComputeSafeLocation (0x6DA358)
  calls it after NoNonWalkPolysOnLine; creature placement (0x66362C) with
  radius [[creature + 0x304] + 4], exclude 0x7F000000, flag 1. Doors (.dwk,
  0x804) are not loaded on the server or in that list.
- CNWSCreature::AIUpdate 0x6475B0 (vtable +0x74, thiscall, plain ret at
  0x647BE0) runs once per object per AI master pass (0x6BF2CD); +0xA8 = ms
  since the creature's last update (0x6476E6), used by the round-pause
  countdown 0x64B4E0. (0x647BF0 is the next function.)
- Object updates to the client (0x62A4xx-0x62A7xx) send x, y when flag 0x200
  is set; NPCs have no teleport bit (+0xB2C only affects the player's own
  creature), so the client always slides an NPC to its latest position.
- AddAttackActions 0x646D80: (target, passive -> +0x4C4, clear first ->
  ClearAllActions 0x63A650, at the front -> 0x63A100 else 0x638F90). Callers:
  player click (t, 0, 1, 0) at 0x6CD87E / 0x6CF800; AI retaliation (t, 1, 1, 0)
  at 0x64A8B7; ActionAttack (t, bPassive, 0, 0) at 0x6E8B39; UseFeat 0x656DB0
  (t, 0, 0, 0/1); AoO (t, 0, 1, 0) in 0x655E70. Cancelling an attack action
  (0x649DCA) clears +0x4E0/+0x4E4, special attacks (0x73BF00) and all 50
  attack slots (0x73C0E0): mid-round it loses the round's remaining attacks.

## AoO direction (0.9.16)

- The AoO broadcast 0x655E70 (this = provoker, kept in ebx) loops over the
  creatures around and, after the visibility / dead / immunity checks, asks
  `provoker->GetReactionTo(candidate id)` at 0x6560F2 (push [esi+4]; mov ecx,
  ebx); only 2 (enemy) continues to the AoO. NWN's movement provocation has
  no direction check, so a creature running toward an enemy provokes it as
  much as one running away. Faithful redirects that call: a running provoker
  (animation 4 / 0x56 / 0x57) whose facing doesn't point away from the
  candidate gets "not an enemy" there. 0.9.15 also showed the round target
  (+0x2134) is already cleared (0x7F000000) when a hit's damage lands, so the
  Push retarget checks the attack order (queued action) as well.

## Potions in the item-use action (0.9.22)

- In the item-use action 0x67B550, the potion branch (base items 49, 54, 104)
  calls the AoO broadcast at 0x67B9E5 — skipped for the party when the
  difficulty option 2 (0x600100(2)) is set (0x67B9A8-0x67B9DA) — then always
  stores 1 at creature +0xA78 (0x67B9EA, `c7 86 78 0a 00 00 01 00 00 00`,
  esi = the drinker; nothing jumps into it). nwn5e calls a stub there that
  does the store and notes the drink. After the drink the player character
  has no orders left (queue empty, current action -1), which is when the
  attack is reissued.

## Stealth and perception (decoded 2026-10-19, SK5)

- Creature bytes: detect mode +0x4A8 (0 passive, 1 and 2 searching;
  SetDetectMode 0x67AFD0), stealth mode +0x4A9 (SetStealthMode 0x67AFF0,
  thiscall (byte mode), ret 4, acts only on a change; loaded from the save at
  0x69EA8F), cached rolls +0xA28 Hide, +0xA29 Move Silently, +0xA2A Spot,
  +0xA2B Listen (bytes; the constructor seeds all four with a d10, 0x6431C4).
- Rolls: the perception update re-rolls every 6 s (0x65302C compares the
  time since +0x338/+0x33C with 6000 ms). In stealth mode: Hide and Move
  Silently d20 (0x65304D, 0x653062). Spot: d20 with Keen Sense (feat 240),
  in either detect mode, or standing still (animation +0xAC == 0); else a d10.
  Listen uses Spot's die size (0x6530E2 / 0x6530FB push the sides).
- Checks (this = observer; (target, flag), ret 8; returns 1 when noticed):
  Listen 0x653180, Spot 0x653870. The first part tests range and line of
  sight; then a score in ebx: the observer's skill (halved, `cdq; sub; sar`,
  in the d10 case) + its cached roll + the area's modifier (+0x14C Listen,
  +0x148 Spot) − the hider's skill total and cached roll (only in stealth
  mode; a total of exactly 0 → noticed at once; a master is always heard by
  its associate, +0xB30) − 10 when the observer is in combat (+0x4BC) − 5 if
  the hider moves, + 5 if the observer moves (0x63A600) − distance; Spot also
  − 5 when behind the observer, light terms (module +0x1F0 == 2 and effect
  types 0x99-0xB4 / 0xB6); size (Listen: the hider's own size +0x4D4, 1..5 →
  −8 / −4 / 0 / +4 / +8; Spot: hider minus observer size, ±4 / ±8); +
  0x6AE770 (the observer's Favored Enemy bonus: the target's race → feats
  0x105-0x117); noticed when the score is > 0.
