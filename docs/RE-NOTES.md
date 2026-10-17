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
Community Patch plugin (`github.com/Shad000w/nwnx_patch-win`, `CreateHook`
calls in `patch.cpp`; repository has no license, so only addresses/names are
used, no code). Spot-checked: every address starts a function (preceded by
`0x90` padding) in the GOG `nwserver.exe`.

`re/match-bytes.mjs` locates each in `nwmain.exe`:

1. Masked byte signature of 24–256 bytes from the server function (call/jmp
   rel32 and any 32-bit value inside the image are wildcards), searched in
   `nwmain.exe`'s `.text`, lengthened until unique.
2. Spacing check: the linker keeps each object file's functions in order, so
   two neighbouring functions sit the same distance apart in both executables.
   A unique match whose distance to a neighbouring match is preserved is
   **confirmed**; ambiguous or missing entries are resolved from a confirmed
   neighbour's distance when the predicted address starts a function.

Result (`re/maps/nwmain-1.69-bytematch.tsv`, 113 names): 83 confirmed,
15 unique, 7 by-spacing, 2 predicted, 3 ambiguous, 3 none.

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

## Open

- What vanilla threat 21 and multiplier x0 mean (see above).
- Name 0x693890 and the multiplier's damage callers.
- Verify the 2 predicted and 3 ambiguous map entries with REA decompilation.
- The threat roll is still rolled and shown in the combat log.
