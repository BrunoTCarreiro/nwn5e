# NWN Diamond 1.69 — how the UI is built, and how Phase 7 can change it

Recon 2026-10-19, read only (no launches, no Ghidra: llvm-objdump, strings,
GFF/ERF/2DA parsing of the game data). **D** = decoded / read in data or code;
**I** = inferred. "EE?" = does this carry over if the project moves to NWN:EE.

## 1. How the 1.69 UI is built

**Three layers, all in the client half of nwmain.exe:**

1. **Panel art = 3D models.** Each panel is an MDL model (`pnl_*`, controls
   `ctl_*`) whose named dummy nodes are hooks the code fills by name. **D**:
   27,605 models in the keys, including `pnl_chrsht*` (character sheet, 7),
   `pnl_chr_*` (creation/level-up, 23), `pnl_cg_*`, `ctl_cg_*` (78),
   `ctl_chrsht_*`. The sheet model `pnl_chrsht_char` has hooks `STRStat0`,
   `STRMod0`… `CHAMod1`, `ArmorClass0/1`, `HitPoints0/1`, `Experience0/1`,
   `NextLevel0/1`, `ClassOne0`…`ClassThree0`, `RaceAlignClass0/1`,
   `LevelUpButton`, and a `List` / `scrollpane` for the stats list.
2. **Layouts = `.gui` files** (type 2047, GFF `GUI V3.28`). **D**: 103
   (177 key entries) in `aurora_gui.bif`: `cg_*` (creation and level-up: race,
   class, attributes, skills, feats, spells, domain, school, familiar, package,
   portrait, `cg_levup_attrib`, `cg_lvlup_summary`), inventory, journal,
   options, examine, store, menus. They place labels/buttons (Obj_Tag,
   Obj_Type 5 = label, position, font `fnt_galahad14`, caption strref)
   attached to model hooks (`Obj_Parent "PNL_CHR_SK"`, `Obj_Parent_Hook
   "PNL_CHR_GENM"`). **No `.gui` for the character sheet, spellbook, radial
   menu or quickbar** — those are model + code only.
3. **Behaviour = hardcoded C++ in the client** (no data-driven logic): which
   values go where, what the level-up panel offers, how many feats / skill
   points / ability points a level grants (I — the engine-coded cadence; not
   found in data).

**Skins:** about 2,600 GUI/icon textures (`gui_*`, `ctl_*`, icons `is_` /
`ife_` / `ir_` / `iit_`…), fonts = 17 texture atlases `fnt_*` + `.txi`. **D**.

**Where the numbers come from (single player = embedded server + client in
one process):** the server builds the character-sheet message at
**0x62BD30** (ret 8, **D**): it reads ability scores and modifiers (0x6A0FF0…,
0x6ADEF0…), AC (0x6A1430), the **total saves via GetFortSavingThrow 0x6AD4F0,
GetWillSavingThrow 0x6AD590, GetReflexSavingThrow 0x6AD630** and the base saves
(0x6A26D0 / 0x6A2810 / 0x6A2960), then combat values (0x693720, 0x695110), and
writes them with WriteBYTE/WriteCHAR (0x4FBD80 / 0x4FBE20). The client keeps
them in a struct (Fortitude `+0x5C`, Will `+0x5D`, Reflex `+0x5E`) and the
stats-list builder (**0x5B2A93–0x5B2E30**, again 0x5B31BA / 0x5B57AD /
0x5B6653 for other sheet variants) prints `GetSimpleString(317/318/319)` +
the byte, and "Base Attack: " (strref 40424) — one list row each (row
creation 0x5A2DB0, text set 0x5CAF20, I). So **every number the sheet shows is
a server value**: server-side hooks already change them (that is why C1–C3
show on the sheet), but a **new row** needs the message, the client reader and
the client list code.

**Level-up:** the client's creation/level-up panels build their choices from
the 2das; the server validates (D: "AutoLevelUp: Validation failed,
returned: '%s'" at 0x914410, used at 0x6AB787 with a tlk error strref; the
validator call itself not decoded — I that player level-ups pass the same
validator). Both sides read the same 2das, so data changes stay consistent;
engine-coded counts would need both sides changed.

## 2. What can be changed and how (routes)

| Route | What it can do | Limits | EE? |
|---|---|---|---|
| **A. 2da data** (hak/override) | Rules both panels and server read: classes.2da (HitDie, AttackBonusTable, FeatsTable, SavingThrowTable, SkillsTable, BonusFeatsTable, **SkillPointBase**, SpellGainTable, **SpellKnownTable**, PreReqTable, Package…), `cls_feat_*` (32), `cls_bfeat_*` (24, bonus-feat levels), `cls_skill_*` (24), `cls_savthr_*`, `cls_pres_*`, `cls_spgn_*`/`spkn`, racialtypes.2da (ability adjusts, FeatsTable, PlayerRace), `race_feat_*`, feat.2da (prereqs, **MinLevel / MinLevelClass / MaxLevel**, SPELLID, USESPERDAY, CATEGORY), skills.2da, spells.2da (**SubRadSpell1–5**, ImpactScript, UserType), packages + packft/sk/sp | Can't change engine-coded cadences (general feat levels, skill-point formula, ability-point levels) or add panels | **Yes** |
| **B. tlk** | Rename anything (panel labels, feat/class/race names, descriptions); new strings in a **module custom tlk** (`Mod_CustomTlk`, strrefs 0x1000000+) | Stock strings are replaced only by editing dialog.tlk (game file) or our GetSimpleString hook (C8, 1.69-only) | **Yes** (custom tlk / dialog.tlk); the hook no |
| **C. Textures, fonts, models, `.gui`** | Reskin; move/resize existing hooks; static labels on `.gui` panels | New *dynamic* content isn't filled (code fills hooks by name) | **Yes** for art (EE also reads them; EE adds NUI) |
| **D. NWScript UI** | Conversations as choosers (`ActionStartConversation` + `SetCustomToken` + conditional nodes); radial entries = feats with SPELLID → spells.2da ImpactScript, up to 5 sub-options (SubRadSpell); chat commands in `OnPlayerChat` (`GetPCChatMessage` exists in 1.69); feedback `SendMessageToPC`, `FloatingTextStringOnCreature`; examine text via `SetDescription`; journal entries | **Entry point:** the OC modules leave `Mod_OnPlrChat` and `Mod_OnClientEntr` empty (D: Prelude, Chapter1, XP1-Chapter1 module.ifo) and name module-specific level-up/rest scripts (`m1q0_levelup`, `nw_o0_levelup`, `x1_playerlevelup`, `m1q0_rest`) → our scripts run only via overriding stock scripts (override folder; must keep their behaviour) or the DLL setting the empty event slots at module load (1.69-only glue). 1.69 NWScript has **no** AddFeat, AddKnownSpell/RemoveKnownSpell, memorised-spell or skill-rank setters | **Yes** (scripts, dlg, 2da); the DLL glue no |
| **E. Server-side engine hooks** (as all rules so far) | Change any value the panels show (saves, the "Base Attack" value → PB, attacks), grant feats/skills/known spells from C++, enforce level-up rules on the server, chat commands (`!saves`, `!mastery`) | Can't add client rows; 1.69 addresses | **Logic yes** (src/rules is engine-free), **hooks no** (EE: other binaries, and no NWNX in EE single player — I) |
| **F. Client panel hooks** | Add sheet rows (e.g. three more saves: hook the stats-list builder near 0x5B2AA9; in single player the client can read server objects directly), change level-up panel logic (feat/skill/ability cadence on the client side) | Deepest RE (client classes unnamed, no Ghidra right now); every panel separately | **No — 1.69-only** (EE would use NUI) |

## 3. Phase 7 items → cheapest route

- **Species:** racialtypes.2da (ability adjusts → 0 for 2024, traits as
  `race_feat_*`) — **A**, EE yes.
- **Background:** no stock step. Make backgrounds feats (skill proficiencies +
  origin feat) picked in a **level-1 bonus-feat slot** (`cls_bfeat_*` row 1)
  whose bonus list holds only background feats (feat.2da MaxLevel 1) — **A**,
  EE yes. Background ability increases: from the feat (effect on first entry
  via script, D) or the creation point-buy — decision.
- **Subclass at 3:** subclass = feat chosen in a **bonus-feat slot at class
  level 3** (`cls_bfeat`, feat.2da MinLevel/MinLevelClass), later features as
  feats with that prerequisite — **A**, EE yes; automatic grants per subclass
  aren't expressible in data → **E** (grant on level-up), logic ports.
- **Feat choices at 4/8/12/16/19:** extra slots via `cls_bfeat` (**A**); the
  stock general-feat cadence (1, 3, 6, 9…) is engine-coded — removing it needs
  **E** (server) **and F** (client panel), 1.69-only. Unknown: where the
  cadence lives (RE).
- **No skill points:** SkillPointBase 0 (**A**) still leaves the engine's Int
  bonus / ×4 at level 1 (I) and the skills panel; 2024 proficiency = feats
  ("Skill: X" picked via background/class slots, **A**) + a server hook making
  the skill value PB-if-proficient (**E**). Hiding the skills step = **F**.
- **Character sheet — six saves, PB, spell DC/attack:** Con/Dex/Wis already
  show (C1–C3 values + C8 labels). Strength / Intelligence / Charisma saves,
  PB, spell DC/attack: cheapest **D** — write them into the PC's examine text
  (`SetDescription`, refreshed on level-up/equip) or a journal page, plus
  `!saves` (exists). Relabel "Base Attack:" to "Proficiency Bonus:" (**B**)
  and send PB in that slot (**E**). Real extra rows = **F**, 1.69-only.
- **Spellbook — prepared list + slots:** use NWN's spontaneous model
  (SpellKnownTable: slots + known list, as sorcerers) for prepared casters
  (**A**), and change the known list on a long rest from a chooser (**D**
  conversation) through the engine's add/remove-known-spell (**E**, not in
  1.69 NWScript; RE the functions + the client refresh). Chooser and data
  port; the known-list calls are 1.69-only.
- **Combat log strings:** new lines → module custom tlk / `SendMessageToPC`
  (**B/D**, port); renaming stock lines → GetSimpleString hook (exists,
  1.69-only) or a dialog.tlk edit (ports, touches a game file).

## 4. Risks

1. **Engine-coded cadences** (general feats, skill points, ability points) sit
   on both client and server; data can add choices but not remove the stock
   ones. Mitigation: accept NWN's cadence in NWN mode, RE the two sides for
   faithful (1.69-only).
2. **Script entry point in the OC:** no chat / client-enter events in the
   official modules; overriding module-specific scripts must keep their
   quest logic. Mitigation: DLL glue that sets empty event slots (small,
   1.69-only), keep the script itself portable.
3. **Client panels can't show new dynamic values** without client hooks;
   workarounds (examine text, journal, chat) are less discoverable.
4. **Unnamed client code:** the client classes have no symbol seeds (the
   community map covers the server); every client hook is fresh RE.
5. **Save compatibility:** feats/skills granted by data or hooks are stored in
   the character (.bic); changing tables later can invalidate existing
   characters' level-up history (server validation on later level-ups).

## 5. Effort (rough)

- Data-only pass (species, backgrounds, subclass slots, extra feat slots,
  skill proficiency feats, spontaneous model for prepared casters): 2da/tlk
  work, no RE — a few sessions; ports to EE.
- Script layer (choosers, examine-text sheet, chat in script) + DLL entry
  glue: 1–2 sessions.
- Server hooks (PB in the "Base Attack" slot, skill value, known-spell
  changes, level-up rules): 2–4 sessions of RE + build.
- Client panel hooks (extra sheet rows, level-up cadence on the client):
  open-ended; 1.69-only.
