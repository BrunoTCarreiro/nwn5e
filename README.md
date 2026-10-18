# nwn5e

D&D 2024 rules (SRD 5.2) for **Neverwinter Nights Diamond 1.69**, single-player.
The full plan is in [docs/PLAN.md](docs/PLAN.md); reverse-engineering notes are in
[docs/RE-NOTES.md](docs/RE-NOTES.md).

`nwn5e.exe` starts the game with `nwn5e.dll` loaded; the DLL replaces engine
rule functions in memory. Nothing in the game folder is modified — starting
NWN the usual way is the unmodified game.

## Status

Phase 0 done; Phase 1a in progress. Rules implemented so far, each switchable
in `nwn5e.ini`:

| Rule | Setting | State |
|---|---|---|
| Critical threat only on a natural 20 | `crit_nat20_only` | verified in game |
| x2 critical multiplier for every weapon | `crit_double_damage` | verified in game |
| No critical confirmation roll | `crit_no_confirmation` | verified in game (debug switch); combat log no longer shows a "Threat Roll" (verified in game 2026-10-18: a goblin's crit read "\*critical hit\* : (20 + 0 = 20)") |
| Attack rolls use the proficiency bonus instead of the 3e BAB | `attack_proficiency` | verified in game (log) |
| Non-player creatures use the expected AC for their CR | `creature_ac_by_cr` | verified in game (log) |
| PCs attack per 2024 Extra Attack; no 3e −5 on extra attacks | `extra_attack` | verified in game (debug switches) |
| Crits roll the damage dice twice, flat bonuses once | `crit_dice_only` | verified in game (log); flat effect damage bonuses once too (0.6.4: call sites verified in game, the damage itself awaits a crit with such an effect) |
| Henchmen use the player-character rules (decision 11) | — (always) | verified in game (2026-10-18, Pavel: "henchman", PB by level 1, attack +4, gear AC kept, sheet converted) |
| After a kill, the round's remaining attacks go to the nearest enemy (decision 14); ranged: only to enemies already in the fight, within `ranged_retarget_range` (40 m) | `retarget_remaining_attacks` | verified in game: melee (debug switches); ranged 40 m with the "in the fight" filter (2026-10-18: the caged Hook Horror at 30.9 m skipped, "none in the fight") |
| No −5 on additional attacks (opportunity, Haste, Flurry, Rapid Shot) | with `extra_attack` | verified in game |
| Character sheet shows the 2024 attack bonus and attacks per round | with `attack_proficiency` / `extra_attack` | attack bonus verified in game (+6 with a sword) |
| Advantage/disadvantage replace NWN's situational bonuses (Phase 1b, PLAN.md B1–B6) | `advantage_disadvantage` | verified in game (log, 2026-10-18): moving target and flanking removed, disadvantage when shooting with goblins adjacent, kept die in the combat log |
| Creatures can crit your party at every difficulty (B7) | `crits_against_players` | verified in game (2026-10-18: a goblin's natural 20 was a critical hit) |
| Faithful / NWN mode for the rules decided since 2026-10-18 (PLAN.md "Modes") | `mode` (faithful, nwn) | built (0.7.2) |
| No racial bonuses vs specific enemies: Battle Training, Good Aim, sling, +4 AC vs giants (R1) | `racial_combat_bonuses` | built (0.7.2); faithful removes, nwn keeps |
| 2024 saves: Con/Dex/Wis values, class proficiency, Aura of Protection, spell DC 8 + PB + mod, fixed DCs converted, no automatic 1/20, species advantages, Halfling Luck (C1–C6) | `saves` | built (0.8.0); racial "+2 vs X" inside the save roll dropped in faithful (C7, 0.8.1); labels / `!saves` (C8) still to come; 0.8.6 reads the engine's save values as the byte they are (the rest of the register is stack garbage: the log and `!saves` could show nonsense, in-game saves were right) |
| Opportunity attacks: only moving away provokes — no AoO for casting, drinking potions, ranged shots, unarmed attacks or Disarm; Spring Attack no longer protects; one per round and Tumble as before (D2) | `opportunity_attacks` | verified in game (0.8.4, 2026-10-18, D&D Hardcore difficulty so the party's ranged/potion AoOs are live in NWN): no AoO for a bow shot with two goblins adjacent, a potion next to a goblin, or a missed punch (twice); running around a goblin still drew four AoOs. Casting, Disarm and Spring Attack patched (verified on load), not tested — no caster or feat. nwn keeps NWN's |
| Two-weapon fighting: no attack penalty on either hand; one off-hand attack, only with two Light weapons, a double weapon or Improved Two-Weapon Fighting (= Dual Wielder); off-hand damage without Strength unless negative or with the Two-Weapon Fighting feat (= the style); Ambidexterity does nothing (D1) | `two_weapon_fighting` | verified in game (0.8.5, 2026-10-18): longsword + dagger made no off-hand attack; short sword + dagger made one off-hand attack per round at +5 like the main hand (NWN: −3), dagger damage without Strength; sheet +5 off-hand. Dual Wielder, double weapons and the Two-Weapon Fighting style not tested (no feats). nwn keeps NWN's |
| Weapon mastery, who (D3): 2024 slots by class, chosen with `!mastery <weapon>` or filled with the weapons a character fights with (announced in chat), one swap per long rest (`!mastery <old> for <new>`, either order); Weapon Focus adds its weapon; stored on the character, so it follows the save game. The effects (Vex, Sap, …) are D4 | `weapon_mastery` | verified in game (0.8.8, 2026-10-18): `!mastery` status and choosing, slots filled by use for Unen and Pavel (announced), swap refused before a rest and done after it (`!mastery dart for sling`), choices kept through save and reload; `!saves` answers. Choices live in the save, so reloading an older save drops later ones |
| Weapon mastery effects (D4), stage 1: Vex (a hit gives advantage on your next attack at that target until the end of your next round) and Sap (a hit gives the target disadvantage on its next attack until your next round starts), for characters with mastery of the weapon; needs `advantage_disadvantage` | `weapon_mastery` | Vex verified in game (2026-10-18: shortbow hit, then "mastery: Vex advantage; d20 20 / 3 -> 20"); Sap marks targets (verified) but its disadvantage roll is not seen yet - the marked goblin died to that hit |
| Weapon mastery effects (D4), stage 2: Graze (a miss deals the attack's ability modifier as slashing damage), Slow (a hit: speed −33% for a round, not stacking), Topple (a hit: Constitution save, DC 8 + ability modifier + proficiency bonus, or knocked down for a round); applied when the swing lands, like the engine's own hits | `weapon_mastery` | verified in game (0.9.4, 2026-10-18): Graze 3 damage per greatsword miss; Topple save DC 13 (goblin +0), on a failure knocked down - the next swing got 2024 prone advantage ("target prone"); Slow applied on crossbow hits. Known gap: Vex and Slow also trigger on hits that deal 0 damage (immune targets). Push still to come |
| Weapon mastery effects (D4), stage 3: Cleave — a melee hit adds NWN's cleave attack (once per round) at the nearest other enemy within 1.5 m of the target and in reach; that attack's damage has no Strength unless it is negative | `weapon_mastery` | in game (0.9.5, 2026-10-18): the extra attack happened after each greataxe hit, at the other goblin (all five missed); 0.9.6 pins the Strength removal to that attack and logs its roll |

Prestige-class levels count as levels of their parent class for Extra Attack
(decision 10).

Known gaps: flat damage bonuses from effects (e.g. Divine Favor) still double
on a crit; two-handed weapons still add 1.5 × Strength to damage (3e; not
yet decided);
the sheet's "Base Attack Bonus" line stays 3e (the client may use it for feat
prerequisites); player AC is unchanged.

Test switches (`[debug]`, all off by default): `attacks_per_round=N` (1–6)
gives player characters N main-hand attacks per round; `creature_ac=N` gives
every non-player creature AC N (40 forces misses, so a round's attacks are all
rolled); `creature_hp_multiplier=N` / `creature_min_hp=N` give a creature N times
its hit points / at least N the first time your party attacks it (so weak monsters survive several hits;
`dm_spawncreature` makes the creature your summon in single player, so it is
no help for test enemies); `attack_trace=1` logs round setup and every PC
attack roll. With
`ClientEntireChatWindowLogging=1` in the game's `nwnplayer.ini`, the game
writes its chat window (combat log included, timestamped) to
`logs\nwclientLog1.txt`.

Test enemies: the game console (the key left of 1) takes `DebugMode 1`,
then `dm_god` (cannot die) and `dm_spawncreature <resref>` (standard
blueprints, e.g. `nw_goblinb` goblin archer CR 1/4, `nw_bandit002` bandit
archer, `nw_kobold005` kobold shaman, `nw_gobwiza` goblin shaman);
`DebugMode 0` turns it off. At D&D Hardcore difficulty NWN also makes the
party provoke with ranged attacks and potions (not at Normal).

Pipeline verified in game (2026-10-17): with the test switch
`[debug] every_hit_crits=1` every hit was a critical, and `nwn5e.log` showed
both hooks and both patches active on the mapped build. The switch is off by
default.

`build\inject_test.exe` checks the injection path without starting the game
(starts SysWOW64\whoami.exe with the DLL; expects it to load and refuse the host).

## Tests

```bat
test.bat
```

Builds and runs `build\rules_test.exe`: unit tests for the 2024 rule math in
`src/rules/` (no game needed). `build\inject_test.exe` (built by `build.bat`)
checks the injection path.

## Build

Requires Visual Studio 2019 Build Tools with the C++ x86 tools.

```bat
build.bat
```

Outputs `build\nwn5e.exe`, `build\nwn5e.dll`, `build\nwn5e.ini`.

## Run

Run `build\nwn5e.exe` and accept the UAC prompt: `nwmain.exe`'s own manifest
requires administrator rights (and the GOG installer sets RUNASADMIN on it),
so the loader must be elevated to start it — the same prompt the normal NWN
shortcut shows. It finds the game through the registry key the GOG
installer writes (override with `[loader] game_dir` in `nwn5e.ini`), checks
that `nwmain.exe` is exactly the mapped 1.69 build (SHA-256
`4d8aa22b1bc9a48c108f3c5d4dc08f49dcddad98d863b89dc3684968d2211c49`), and
refuses to start anything else. The DLL writes `build\nwn5e.log`: which hooks
were installed and each rule decision.

Quick check without the UAC prompt (used by Claude, owner-approved
2026-10-18; the game runs windowed): from Git Bash in `build\`,
`__COMPAT_LAYER=RunAsInvoker ./nwn5e.exe` runs the loader and the game with
normal user rights (the game folder is user-writable). `build\nwn5e.log`
then shows the version, the verified call sites and "hooks active"; close
the game completely with `taskkill //F //IM nwmain.exe` (without `/F` NWN
ignores the request and keeps running) and check `tasklist` before building
again.

## Changes to the game folder

The mod itself changes nothing in `X:\GOG\NWN Diamond`. These owner requests
did, and each is reversible from `backup\` (not in git):

| Change | Why | Undo |
|---|---|---|
| `nwnplayer.ini` `[Game Options] ClientEntireChatWindowLogging=1` | timestamped combat log in `logs\nwclientLog1.txt` for tests | set it back to `0` (original: `backup\nwnplayer.ini.before-chatlog`) |
| `movies\AtariLogo.bik`, `BiowareLogo.bik`, `WOTCLogo.bik`, `fge_logo_black.bik`, `NWNintro.bik` moved out | skip the loud startup logos and intro | copy them back from `backup\movies\` |
| `nwn.ini` `[Display Options] FullScreen=0`, `AllowWindowedMode=1` (added), `Width=1600`, `Height=900` (2026-10-18) | the game runs in a window, so it can be started for quick checks. `FullScreen=0` alone still opened maximized and switched modes on focus loss; the binary also reads `AllowWindowedMode` | `FullScreen=1`, remove `AllowWindowedMode`, `Width=2560`, `Height=1080` (original: `backup\nwn.ini.before-windowed`) |

## Reverse-engineering toolchain

Portable, under `X:\tools` (Node 24.21, Ghidra 12.1.4, REA 6.5.0). Use
`source X:/tools/rea-env.sh` (Git Bash) or `. X:\tools\rea-env.ps1`.

- `re/rea-bridge.mjs` keeps one REA MCP session (Ghidra provider) alive on
  `127.0.0.1:47815`, so the long first analysis of `nwmain.exe` is paid once.
- `re/call.sh <tool> '<json>'` calls one REA tool through the bridge.
- `re/match-bytes.mjs` maps named `nwserver.exe` functions to `nwmain.exe`.
- `re/prologue.mjs` prints the first bytes of a function for the DLL's checks.

## Legal

Personal-use modification of a legally owned copy. No game code or data is
distributed. 2024 rules text comes from SRD 5.2 (CC-BY-4.0); see PLAN.md.
