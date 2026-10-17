# nwn5e

D&D 2024 rules (SRD 5.2) for **Neverwinter Nights Diamond 1.69**, single-player.
The full plan is in [docs/PLAN.md](docs/PLAN.md); reverse-engineering notes are in
[docs/RE-NOTES.md](docs/RE-NOTES.md).

`nwn5e.exe` starts the game with `nwn5e.dll` loaded; the DLL replaces engine
rule functions in memory. Nothing in the game folder is modified — starting
NWN the usual way is the unmodified game.

## Status

Phase 0 (foundation). Rules implemented so far, each switchable in `nwn5e.ini`:

| Rule | Setting | State |
|---|---|---|
| Critical threat only on a natural 20 | `crit_nat20_only` | built, awaiting in-game test |
| x2 critical multiplier for every weapon | `crit_double_damage` | built, awaiting in-game test |
| No critical confirmation roll | `crit_no_confirmation` | built, awaiting in-game test |

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
