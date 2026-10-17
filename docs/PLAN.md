# nwn5e — full D&D 2024 (5.5e) conversion of Neverwinter Nights Diamond

Target: **NWN Diamond 1.69 (GOG), single-player**, `X:\GOG\NWN Diamond\nwmain.exe`
(PE32 x86, linked 2008-07-03, unpacked, fixed base 0x400000, no relocations).
Rules: **2024 rules as published in SRD 5.2** (CC-BY-4.0). Non-SRD 2024 material
(extra subclasses, backgrounds, feats) is personal-use only and must not ship in
anything shared.

Goal: a **full conversion** — the game plays by 2024 rules end to end, not a
"5e-flavoured" 3e.

## How it works

NWN keeps its rules in three places. Each 5e rule lands in the one that owns it:

| Layer | Owns | How we change it |
|---|---|---|
| Data (`.2da`, `dialog.tlk`) | numbers: hit dice, class tables, feats, spells, skills, base items | override folder / hak |
| NWScript (`.nss` → `.ncs`) | spell effects, rest, dying, module events | override folder / hak |
| Engine (`nwmain.exe`) | attack resolution, saves, crits, attacks per round, level-up, spell memorisation, GUI | **nwn5e.dll** hooks injected by **nwn5e.exe** |

- `nwn5e.exe` starts `nwmain.exe` suspended, injects `nwn5e.dll`, resumes it.
  The game files on disk are never modified; launching NWN normally is vanilla.
- `nwn5e.dll` refuses to patch anything unless `nwmain.exe`'s SHA-256 matches
  the build we mapped.
- Single-player runs an embedded server inside `nwmain.exe`; the server-side
  rule classes (`CNWS*`) are compiled into it at different addresses than in
  `nwserver.exe`. Function addresses are found with REA (Ghidra provider)
  starting from the community's 1.69 `nwserver.exe` names
  (`re/seeds/`, provenance in `docs/RE-NOTES.md`), and recorded in `re/maps/`.

## Phases

Each phase ends with an in-game test by the owner (the build machine cannot
play the game).

### Phase 0 — Foundation
- Toolchain in `X:\tools` (Ghidra 12.1.4, Node 24.21, REA 6.5.0). ✅
- Function map `nwserver.exe` → `nwmain.exe` for the rule functions.
- Loader + DLL + hook framework (MinHook, logging, hash check, ini config).
- Proof of concept: crits only on a natural 20, no confirmation roll.

### Phase 1 — Combat core

Split into sub-plans that each ship on their own: **1a** attack math
(proficiency bonus, Extra Attack, 2024 crit damage, creature numbers), **1b**
advantage/disadvantage and conditions, **1c** saving throws, **1d** weapon
mastery, two-weapon fighting, opportunity attacks. The rule math lives in
`src/rules/` (no engine code, unit-tested by `test.bat`); the hooks only read
engine state, call it, and write the result back.

Built so far (`src/rules/`, 31 tests): ability modifier, proficiency bonus by
level and by CR, save DC / spell attack, advantage/disadvantage d20 rolls,
Exhaustion penalty, attack ability and bonus, hit/miss/critical resolution,
damage with crit dice rolled twice, attacks per Attack action, expected
monster AC by CR.

- Attack roll: d20 + ability mod + proficiency bonus (+ magic) vs AC; no BAB.
- Advantage / disadvantage as a first-class roll modifier (sources: conditions,
  flanking replaced, hidden/unseen, prone, ranged in melee, etc.).
- Critical hits: natural 20 only, double damage dice, no confirmation.
- Attacks per round: 1, Extra Attack (2/3/4 for Fighter), no iterative −5s.
  NWN drops the rest of a round when its target dies (Cleave exists for
  that); 2024 Extra Attack lets each attack pick a target, so remaining
  attacks should move to the next enemy in reach.
- 2024 weapon properties and **Weapon Mastery** (Cleave, Graze, Nick, Push,
  Sap, Slow, Topple, Vex) on NWN base items.
- Two-weapon fighting per 2024 (Light property, Nick; bonus-action attack).
- Opportunity attacks as reactions.
- Conditions (2024 list) mapped onto NWN effects; exhaustion (−2 per level to
  d20 tests, −5 ft speed).

### Phase 2 — Saves, checks and skills
- Six saving throws (Str/Dex/Con/Int/Wis/Cha); class save proficiencies.
- Save DC = 8 + PB + ability mod; spell attack = PB + ability mod.
- Skills as proficiency / expertise (no skill points); 2024 skill list mapped
  onto NWN's skills (drop/merge the 3e-only ones).
- Ability checks and contested checks (grapple/shove → 2024 Unarmed Strike options).

### Phase 3 — Character model
- Levels 1–20, proficiency bonus +2…+6, 2024 XP table.
- Classes: the 12 2024 classes. NWN has 11 of them; **Warlock** is new.
  Subclass at level 3 (2024). NWN prestige classes become subclasses where a
  2024 equivalent exists, otherwise disabled.
- Species (2024): no ability increases from species.
- Backgrounds (2024): ability increases + Origin feat + skill/tool proficiencies.
- Feats: Origin, General (ASI at 4/8/12/16), Fighting Style, Epic Boon (19).
- Multiclassing prerequisites (13 in primary ability).
- Hit points: max at 1st level, fixed average or roll per level.

### Phase 4 — Spellcasting
- Slot tables per class (full/half/pact casters); Pact Magic for Warlock.
- 2024 preparation: prepared list changes on a long rest; spells cast from any
  slot of sufficient level — **no per-slot memorisation**.
- Cantrips at will, scaling at 5/11/17.
- Upcasting; concentration (one at a time, Con save on damage: DC 10 or half
  damage, max 30); rituals (decide).
- Rewrite spell scripts to SRD 5.2; add missing SRD spells; retire 3e-only ones.

### Phase 5 — Rest, healing, death
- Short rest (spend Hit Dice) and long rest (8 h, regain all HP and half max HD).
- Death saving throws at 0 HP; stabilise; massive damage.

### Phase 6 — Action economy
- Action / Bonus Action / Reaction / free object interaction inside NWN's
  6-second round. Highest design risk: NWN's action queue has no bonus action
  or reaction concept. Needs a design pass before code.

### Phase 7 — Interface
- Character creation and level-up screens for the 2024 flow (background,
  species, subclass at 3, feat choices, no skill points).
- Character sheet: six saves, proficiency bonus, spell DC/attack.
- Spellbook: prepared list + slots instead of memorised slots.
- Combat log strings (custom `.tlk`): advantage/disadvantage, saves, mastery.

### Phase 8 — Creatures and campaign balance
- Bounded accuracy breaks against 3e stat blocks (HotU creatures with AC 30+).
  Convert creatures at spawn from their 3e blueprint to a 5e-style stat block
  (CR-based AC/HP/attack/DC) with hand-tuned overrides for named creatures.
- Campaign XP and the level cap: OC/SoU/HotU expect up to level 40.

### Phase 9 — Balance and polish
- Play-through of OC chapter 1, SoU, HotU openers; tuning tables.

## Open decisions (owner)

1. Half-elf and half-orc are not 2024 species — keep as legacy, or replace?
2. New species (dragonborn, tiefling, goliath, aasimar, orc) — appearance work needed.
3. Epic levels: cap at 20 with Epic Boons, or a custom 21+ track for HotU?
4. Hit points per level: fixed average or rolled?
5. Prestige classes with no 2024 subclass equivalent: drop or adapt?
6. Rituals and material components: model or ignore?
7. Action economy design (Phase 6).
12. Subclass selection UI: prestige classes become 2024 subclasses (decision
    10). Proposed: pick the subclass at level 3 in the existing level-up feat
    step (one "subclass" feat per option, custom names/descriptions/icons),
    grant subclass features at their levels from the DLL, and hide prestige
    classes from the class list. A dedicated subclass screen would need the
    client's level-up UI code reworked — not planned.

## Decided (owner, 2026-10-17)

8. Expanded critical range (Champion's Improved Critical, 19–20): the expanded
   range only upgrades a hit; a natural 19 that does not reach the AC misses.
   Only a natural 20 always hits (`rules::ResolveAttackRoll`).
9. Creature AC conversion target: keep the 2014 DMG "Monster Statistics by
   Challenge Rating" AC column as a personal-use guideline
   (`rules::ExpectedMonsterArmorClass`); not shipped in anything shared.
10. Prestige classes count as levels of the class they would be a 2024
    subclass of (`rules::ClassFromNwnId`): Weapon Master, Arcane Archer,
    Purple Dragon Knight, Dwarven Defender → Fighter; Champion of Torm,
    Blackguard → Paladin; Assassin, Shadowdancer → Rogue; Harper Scout → Bard;
    Red Dragon Disciple → Sorcerer; Pale Master → Wizard; Shifter → Druid.
    Levels add to the parent class (Fighter 6 / Weapon Master 6 = Fighter 12).
11. Henchmen use the player-character rules (gear AC, proficiency by level,
    Extra Attack from their classes). Summons, familiars and animal companions
    keep the creature rules (CR-based AC and proficiency, engine attack count)
    — they have stat blocks, not gear.
13. Critical hits double only the damage dice (2024), not modifiers.
14. When the target dies mid-round, the remaining attacks move to the next
    enemy in reach (2024 Extra Attack lets each attack choose a target).
    Ranged (owner, 2026-10-17): within 20 m, and only to an enemy already in
    the fight — one charging or attacking the attacker or the party — so a
    bow never picks an idle, caged or sleeping enemy. With none, the
    remaining shots are lost, as in NWN.
