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

## Open

- Critical confirmation roll: inside the attack resolution called from
  `CNWSCreature::ResolveAttack` (0x740180). Next: REA `procedure_callees` +
  `procedure_pseudo_code` to find the attack-roll function and the confirm
  comparison.
- Verify the 2 predicted and 3 ambiguous entries with REA decompilation.
