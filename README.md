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
| 2024 saves: Con/Dex/Wis values, class proficiency, Aura of Protection, spell DC 8 + PB + mod, fixed DCs converted, no automatic 1/20, species advantages, Halfling Luck (C1–C6) | `saves` | built (0.8.0); racial "+2 vs X" inside the save roll dropped in faithful (C7, 0.8.1); `!saves` answers (0.8.7); 2024 labels ("Constitution / Dexterity / Wisdom", sheet, combat log, effect and spell texts) swapped in memory in faithful mode from 0.9.9 (C8, not yet seen in game); 0.8.6 reads the engine's save values as the byte they are (the rest of the register is stack garbage: the log and `!saves` could show nonsense, in-game saves were right) |
| Opportunity attacks: only moving away provokes — no AoO for casting, drinking potions, ranged shots, unarmed attacks or Disarm; Spring Attack no longer protects; one per round and Tumble as before (D2) | `opportunity_attacks` | verified in game (0.8.4, 2026-10-18, D&D Hardcore difficulty so the party's ranged/potion AoOs are live in NWN): no AoO for a bow shot with two goblins adjacent, a potion next to a goblin, or a missed punch (twice); running around a goblin still drew four AoOs. Casting, Disarm and Spring Attack patched (verified on load), not tested — no caster or feat. nwn keeps NWN's |
| Opportunity attacks only from creatures the mover runs away from (D2 follow-up): NWN gave one to every enemy a running creature passed, also when it ran toward them (a goblin charging back after a Push) | `opportunity_attacks` (faithful) | built (0.9.16; patch verified on load) |
| After drinking a potion in a fight, a player character goes back to attacking its last target (or, if it fell, the nearest enemy it sees within 10 m) — NWN dropped the attack order on item use (owner request; a convenience, not a 2024 rule) | `resume_attack_after_potion` (both modes) | built (0.9.22; patch verified on load) |
| Levels and XP (CM1): the 2024 XP table (level 2 at 300 … 20 at 355,000), level 20 is the cap (also with HotU); scripts that set XP by NWN's table keep their level (HotU starts you at 15), and no XP loss (respawn, crafting) takes a level | `progression` | verified in game (2026-10-19: the 2024 table on the sheet; script SetXP not seen yet) |
| Multiclassing (CM2): no XP penalty; a new class needs 13 in its primary ability and in every class you have (the level-up screen greys it out) | `multiclass` | verified in game (2026-10-19: a Fighter with Int 10 / Wis 10 / Cha 9 refused 8 classes, kept Barbarian and Rogue) |
| Alignment (CM14): only a description, as in 2024 — no class needs one (chaotic paladins, non-lawful monks; drifting never stops paladin or monk levels), every alignment at creation, items' alignment limits ignored; the campaigns still shift and check it | `alignment` | verified in game (owner, 2026-10-19) |
| Served data (CM0): nwn5e's own 2da rows and strings (data/rows, data/strings.txt) are merged into the game's tables at start-up and served from build\served — the game install isn't touched | `served_data` | built (0.9.50; probe verified on load) |
| When feats come (CM7): a feat at class levels 4, 8, 12, 16, 19 (Fighter also 6, 14; Rogue 10) where Ability Score Improvement is one choice; general feats only at level 1; no ability +1 every 4 levels; NWN's class bonus feats gone | `feat_cadence` | built (0.9.51) |
| Ability scores and backgrounds (CM4, CM5): no racial ability increases; 27-point buy to 15 at creation; NWN's +1 every 4 levels no longer counts; `!background` (Acolyte, Criminal, Sage, Soldier: +2 / +1 or +1 / +1 / +1, two skills, Origin feat — Criminal's Alert works now) and `!asi` for each Ability Score Improvement (henchmen pick their own); nothing above 20 | `ability_scores` | built (0.9.52) |
| Hit points (CM3): 2024 — the hit die's maximum at level 1, then its fixed average, plus Constitution (at least 1 per level), with the 2024 dice (Bard and Rogue d8, Sorcerer and Wizard d6); Toughness and Epic Toughness add nothing; player characters and henchmen | `hit_points` | verified in game (2026-10-20: a Fighter, Con +2: 12 at level 1, 20 at level 2 — NWN 24) |
| The 3e-only skills (SK8): parry mode does nothing (no parries; the creature fights normally), Taunt does nothing, stores keep their own prices (no Appraise haggling), Use Magic Device no longer lifts class / race / alignment limits on items | `nwn_only_skills` | built (0.9.45; patches verified on load) |
| Conditions on checks (SK11): poisoned or frightened (source in sight) gives disadvantage on initiative, perception (passive 10 → 5), Search and scripted skill checks; deafened can't hear, blinded can't see or search | `conditions` (faithful) | initiative verified in game (0.9.44 test switch: 14 / 19 -> 14 with disadvantage; hidden + poisoned cancel, 6 / 7 -> 6); perception / Search / scripted checks built; the engine's lock / trap / heal-kit actions not yet |
| Initiative (SK9): d20 + Dexterity, + proficiency bonus with Improved Initiative (2024's Alert); no Epic / Blooded / Thug bonuses; advantage while invisible or hidden; Halfling Luck | `initiative` | verified in game (0.9.42: attacking from hiding, d20 7 / 6 -> 7 with advantage, + Dex 2 = 9); Improved Initiative not yet seen |
| Knockdown and Disarm (SK6): Knockdown and Improved Knockdown are a 2024 Shove — no attack roll, no damage; the target makes its better Strength or Dexterity save against 8 + your Strength modifier + proficiency bonus or is knocked down (at most one size larger than you); Disarm is an ordinary attack (2024 has no disarming) | `knockdown` | verified in game (0.9.41: 7 Shoves, no roll, no damage, Strength save vs DC 13, 5 knocked down); Disarm not tested |
| Concentration (SK7): casting is not interrupted by damage; no Concentration check to cast defensively or while entangled (keeping concentration spells comes with Phase 4) | `concentration` | built (0.9.39; patches verified on load) |
| Spell attacks (SK10): touch attacks (Ghoul / Vampiric Touch, Harm, Slay Living, monster bolts, beholder rays, ...) use proficiency + the spellcasting ability (a scroll's bonus from items) against full AC, with advantage / disadvantage; a natural 20 crits without confirmation | `spell_attacks` | verified in game (0.9.38: Mask of Death Vampiric Touch — item bonus +7, full AC 13, d20 13 + 7 = 20 hit); a monster caster, advantage and a natural-20 crit not yet seen |
| Stealth and Perception (SK5): a hiding creature rolls Stealth once, when it starts hiding, and keeps it until it stops (told in chat; below 15 it is not hidden — `hiding_needs_15`); observers (and Search for traps) use passive Perception (10 + modifier) unless searching (detect mode: a d20); no size modifier, no −10 in combat, no Favored Enemy bonus; distance, light and movement still count; an enemy finds you in front of it within 2 m (`found_up_close`), and creatures in combat search with a d20 (`alert_creatures_search`) | `perception` | verified in game (0.9.29–0.9.35: one roll per hide, DC 15, passive Perception, found up close in front within 2 m — behind still hidden); alert searching built, not yet seen in a test |
| Skill DCs (SK3): the engine's fixed DCs on the 2024 scale, 10 + (DC − 10) / 2 — locks (pick and lock), traps (flag, recover, disarm, examine, set), Search, Heal kits, Pick Pocket, Animal Empathy, Tumble past attacks of opportunity, Spellcraft identifying spells, scripts' GetIsSkillSuccessful; the combat log shows the 2024 DC; Lore identification reads the 2024 total on the 3e table; no take 20, no Disable / Set Trap synergy, no Rogue-only traps above DC 35 | `skills` (faithful) | built (0.9.30–0.9.31; patches verified on load) |
| Skills (Phase 2 SK1–SK2, SK3 a): the 2024 skills — ability modifier + proficiency bonus if proficient (twice with Expertise) + item and spell effects; no ranks, Skill Focus or racial skill bonuses; NWN skills shown with 2024 names (Hide / Move Silently = Stealth; Spot / Listen / Search = Perception; locks and traps = Thieves' Tools; ...); choose with `!skills` (checked against 2024 class, background, species and Expertise choices; until then taken from your ranks); scripts (conversation checks) read the values scaled to the 3e range; Concentration, Discipline, Parry, Taunt, Appraise, Use Magic Device and Craft Trap keep NWN's values until SK6–SK8 | `skills` | built (0.9.23; hook and patch verified on load) |
| Two-weapon fighting: no attack penalty on either hand; one off-hand attack, only with two Light weapons, a double weapon or Improved Two-Weapon Fighting (= Dual Wielder); off-hand damage without Strength unless negative or with the Two-Weapon Fighting feat (= the style); Ambidexterity does nothing (D1) | `two_weapon_fighting` | verified in game (0.8.5, 2026-10-18): longsword + dagger made no off-hand attack; short sword + dagger made one off-hand attack per round at +5 like the main hand (NWN: −3), dagger damage without Strength; sheet +5 off-hand. Dual Wielder, double weapons and the Two-Weapon Fighting style not tested (no feats). nwn keeps NWN's |
| Weapon mastery, who (D3): 2024 slots by class, chosen with `!mastery <weapon>` or filled with the weapons a character fights with (announced in chat), one swap per long rest (`!mastery <old> for <new>`, either order); Weapon Focus adds its weapon; stored on the character, so it follows the save game. The effects (Vex, Sap, …) are D4 | `weapon_mastery` | verified in game (0.8.8, 2026-10-18): `!mastery` status and choosing, slots filled by use for Unen and Pavel (announced), swap refused before a rest and done after it (`!mastery dart for sling`), choices kept through save and reload; `!saves` answers. Choices live in the save, so reloading an older save drops later ones |
| Weapon mastery effects (D4), stage 1: Vex (a hit gives advantage on your next attack at that target until the end of your next round) and Sap (a hit gives the target disadvantage on its next attack until your next round starts), for characters with mastery of the weapon; needs `advantage_disadvantage` | `weapon_mastery` | verified in game (2026-10-18): Vex - shortbow hit, then "mastery: Vex advantage; d20 20 / 3 -> 20"; Sap - after a longsword hit the goblin rolled "disadvantage; mastery: Sap disadvantage; d20 4 / 1 -> 1" |
| Weapon mastery effects (D4), stage 2: Graze (a miss deals the attack's ability modifier as slashing damage), Slow (a hit: speed −33% for a round, not stacking), Topple (a hit: Constitution save, DC 8 + ability modifier + proficiency bonus, or knocked down for a round); applied when the swing lands, like the engine's own hits | `weapon_mastery` | verified in game (0.9.4, 2026-10-18): Graze 3 damage per greatsword miss; Topple save DC 13 (goblin +0), on a failure knocked down - the next swing got 2024 prone advantage ("target prone"); Slow applied on crossbow hits. From 0.9.10 Vex and Slow need the hit to deal damage (measured where the damage lands). Push still to come |
| Weapon mastery effects (D4), stage 3: Cleave — a melee hit adds NWN's cleave attack (once per round) at the nearest other enemy within 1.5 m of the target and in reach; that attack's damage has no Strength unless it is negative | `weapon_mastery` | verified in game (0.9.7, 2026-10-18): after a greataxe hit the extra attack went at the other goblin (logged, and that goblin was first attacked then); its hits did 2 and 2 damage (1d12, no Strength) where normal hits did 8-14 (1d12+4) |
| Weapon mastery Push (D4, stage 4): a hit with a warhammer or heavy crossbow pushes a Large or smaller target up to 3 m straight away — it plays the knockdown fall as a visual only (its round pauses 1.2 s and is shortened by as much, no knockdown) and slides back along the line during the fall (0.15 s, stepped each AI update; it can't walk for the 1.2 s fall); the line must be walkable and clear of placeables (fountains, statues), else the push is shorter or none. When an enemy is then closer to the attacker than the pushed one, the attacker goes for the nearest one instead of chasing | `weapon_mastery` (faithful) | 0.9.13 tested in game (2026-10-19): pushes 3 m, stops at walls, but slid after the move and got stuck in a fountain; 0.9.14 (slide during the fall, placeables, retarget) built, hooks verified on load; **0.9.22 verified in game (2026-10-19): pushing a goblin into a Caltrops field fires the field's on-enter event** (1 damage 1.4 s and 1.8 s after two pushes; the field's per-round check cannot account for both); the fall animation was being reset to idle ~70 ms after the push, now put back by the hold |
| Two-handed weapons add Strength once to damage, not NWN's 1.5 × (E1) | `two_handed_strength` | verified in game (0.9.8) |
| Spells use their 2024 save ability (C1): Strength for Entangle, Gust of Wind, Evard's Black Tentacles; Intelligence for Feeblemind; Charisma for Bane, Banishment, the Planar Bindings; Constitution for Cone of Cold, Sunbeam, Sunburst, Inflict Wounds — with that ability's 2024 save value (proficiency by class). A spell's own DC is never re-converted (fixes lingering areas of effect) | `saves` (faithful) | verified in game (0.9.28: Cone of Cold rolls and shows "Constitution Save") |
| Deafened no longer makes spells fail (CN8; NWN's 20% only ever hit Bard / Sorcerer / Wizard spells); a deafened creature still can't hear | `conditions` | built (0.9.11) |
| Conditions (CN1–CN4, CN6): restrained (webs, entangle) — advantage against it, disadvantage on its attacks and Dexterity saves, NWN's −2 attack / −4 Dex gone; stunned, paralysed and sleeping targets give advantage (both modes) and fail Strength / Dexterity saves (stunned both modes, the others faithful); a hit on a paralysed or sleeping creature from within 5 ft is a critical hit (crit immunity respected); poisoned — disadvantage on its attacks; concealment (Blur, Displacement, Epic Self-Concealment) — disadvantage on attacks against it instead of NWN's miss chance. Close-combat threats (B5) now ignore any incapacitated enemy, not only NWN's top state | `conditions` | built (0.9.12; patches and hooks verified on load) |
| Frightened (CN5, faithful): no fleeing and no loss of control — NWN's fear state is replaced by the fear icon for its duration, and the creature attacks with disadvantage while it can see the source (monsters keep fighting); immunities stay NWN's; Remove Fear doesn't end it early. NWN's coup de grace on helpless creatures is gone | `conditions` | built (0.9.13; hooks verified on load) |

Prestige-class levels count as levels of their parent class for Extra Attack
(decision 10).

Known gaps: the sheet's "Base Attack Bonus" line stays 3e (the client may use
it for feat prerequisites); player AC is unchanged. Flat effect damage bonuses (e.g. Divine Favor) are
added once on a crit since 0.6.4, not yet seen in a crit in game.

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
distributed. 2024 rules text comes from SRD 5.2 (CC-BY-4.0); see PLAN.md. This
repo implements the SRD only; non-SRD material lives in a private layer that is
not published (docs/PRIVATE-LAYER.md).
