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

## Phase 1b — advantage/disadvantage (decided by the owner, 2026-10-18)

Proposed the same day; the owner took every recommendation except B5, where
the 2024 rule was chosen over NWN's check.

**How.** NWN adds situational bonuses as flat 3e numbers inside
GetAttackModifierVersus (all of them decoded, RE-NOTES "situational terms").
ResolveAttackRoll rolls its d20 (call at 0x744401, `RollDice(1, 20)`
0x47E120) *before* it asks for the attack modifier, so:
1. ResolveAttackRoll hook (already hooked for the trace), at entry: work out
   which situational terms apply to this attacker and target (the engine's
   own checks), keep their 3e total (`rules::NwnSituationalTotal`) and the
   roll mode (`rules::SituationalRollMode`, with the B5 close-combat check)
   for this attack.
2. The d20 call at 0x744401 goes to a function that rolls two dice and keeps
   the higher or lower (`rules::RollD20`, the engine's own RollDice for each
   die). The kept die is the natural roll for natural 1 / 20.
3. The GetAttackModifierVersus hook takes the 3e total back out for that
   attack.
The parry roll (0x74465E) and the threat roll (0x744707) are untouched.

**B1. Mapping** (decided as proposed; `src/rules/nwn_situational.cpp`):

| NWN term (3e) | 2024 |
|---|---|
| Target blind (+2), attacker invisible to the target (+2) | Advantage |
| Target stunned (+2) | Advantage |
| Target prone: melee +4 / ranged −4 | Advantage in melee, Disadvantage at range |
| Can't see the target (−4) | Disadvantage |
| Ranged attack within the target's melee reach (−4) | removed; see B5 |
| Moving target (−2, ranged) | nothing (no 2024 rule) |
| Target flanked (+2) | see B2 |

**B2. Flanking: off.** Core 2024 has no flanking; the DMG's optional rule
(Advantage) is not used — with a henchman almost every melee attack would
flank, making Advantage the norm. NWN's +2 is removed. (Sneak Attack's own
flanking check is separate and unchanged.)

**B3. Point Blank Shot.** The NWN feat removes the −4 for shooting in melee
and adds +1 within 5 m; 2024 has no such feat. Until feats are reworked
(Phase 3): it removes the close-combat Disadvantage (as 2024 Sharpshooter
and Crossbow Expert do) and keeps its +1.

**B4. Every attacker** — party, henchmen, creatures, summons — as in 2024.

**B5. Close combat: the 2024 rule.** A ranged attack has Disadvantage when
any enemy of the attacker that is within its own melee reach of the
attacker can see the attacker and is not incapacitated (dead, dying,
helpless) — not only the target. NWN's target-only −4 is still taken out of
the engine's number.

**B6. Combat log.** It shows one d20 ("17 + 2 = 19"): the kept die;
nwn5e.log records both dice and the mode. An "Advantage" label in the log
text is Phase 7 (interface).

**B7. Crits against players** (taken with "your recommendation for all"):
NWN turns a creature's critical hit on a player-controlled character into a
plain hit (ResolveAttackRoll's crit path, 0x600100 — likely a difficulty
rule); 2024 has no such rule, so it is removed, behind its own switch.

Later sources, as each NWN effect is converted: conditions (Restrained,
Paralyzed, Poisoned, Frightened…), hidden attackers, Reckless Attack, Help,
spells (Faerie Fire, Bless's d4 is not adv/disadv).

## Phase 1c proposal — six saving throws (2026-10-18, awaiting owner)

**How NWN does it** (RE-NOTES "Saving throws"). Scripts ask for one of three
saves with a DC and a save type (fear, poison, fire…). The engine's save roll
(0x749230) adds the save value (GetFort/Ref/WillSavingThrow 0x6AD4F0 /
0x6AD630 / 0x6AD590), effect and feat bonuses against that save type (capped
at +20) and a d20; a natural 20 always succeeds, a natural 1 fails only under
a server option. The save value is: class base save (table) + ability
modifier (Con / Dex / Wis) + Luck of Heroes (+1) + Dark Blessing (Cha) + a
creature blueprint bonus + magic effects. Spell DCs (0x6659D0) are 10 +
spell level + casting ability modifier + Spell Focus (+2 / +4 / +6). The
character sheet reads the same three save functions.

**Plan.** Hook the three save functions to return the 2024 save: ability
modifier + proficiency bonus when proficient + magic effects; hook the spell
DC to 8 + proficiency bonus + casting ability modifier. The sheet follows.
Rules built and tested (`src/rules/saves.cpp`).

**C1. Which 2024 save each NWN save is.** Proposed: Fortitude →
Constitution, Reflex → Dexterity, Will → Wisdom. Strength, Intelligence and
Charisma saves only appear when spells are converted (Phase 4).

**C2. Proficiency.** Characters (and henchmen): the two saves of their first
class (2024; multiclassing adds none) — proposed. Creatures: proficient where
their NWN base save is a "good" save (the 3e good progression) — proposed;
alternatives: never proficient, or keep NWN's numbers for creatures.

**C3. 3e save bonuses** (Luck of Heroes, Dark Blessing, Divine Grace, Great Fortitude /
Lightning Reflexes / Iron Will, racial +1/+2s, the blueprint bonus).
Proposed: dropped, except magic effects and items; 2024 equivalents
(Paladin Aura of Protection, dwarf/halfling/gnome advantages) come with the
class and species rework (Phase 3). Alternative: keep them for now.

**C4. Natural 1 / 20 on saves.** 2024: no automatic success or failure on
saves. Proposed: follow 2024 (a 20 can fail a DC 25).

**C5. Spell DC.** Proposed: 8 + proficiency bonus + casting ability modifier
— spell level no longer raises the DC (2024). Spell Focus feats: proposed
dropped (no 2024 equivalent) — alternative: keep +1/+2/+3.

**C6. Other DCs** (scrolls/wands/potions, monster abilities with the fixed
"3" modifier, traps, poisons from 2DAs). Proposed: monster abilities use 8 +
their CR proficiency bonus + 3 now; items, traps and poisons stay NWN's until
Phase 8 (creatures and campaign balance).

**C7. Effect and feat bonuses inside the save roll** (vs fear, poison,
mind…). Proposed: kept for now (they are mostly items and spells); racial
ones move to advantage with Phase 3.

**C8. Character sheet.** It shows the three NWN saves; with C1 it would show
the Con / Dex / Wis saves under the Fortitude / Reflex / Will labels.
Proposed: fine until the interface phase (Phase 7) relabels them.

## Phase 1d proposal — two-weapon fighting, opportunity attacks, weapon mastery (2026-10-18, awaiting owner)

Engine facts: RE-NOTES "Two-weapon fighting and attacks of opportunity".
Ordered by how much each needs: D1–D2 are small engine changes, D3–D4 need
the Phase 1c saves (Topple) and new effects (Slow, Push).

**D1. Two-weapon fighting.** NWN: an off-hand weapon gives one off-hand
attack each round (two with Improved Two-Weapon Fighting), with −6/−10 on
the main and off hand (−4/−8 if the off-hand weapon is light; feats reduce
it); the off-hand adds half Strength to damage. 2024: one extra attack with
the other Light weapon (a Bonus Action), no attack penalty, and no ability
modifier on its damage unless negative (the Two-Weapon Fighting style adds
it). Proposed: no penalties on either hand; one off-hand attack (Improved
Two-Weapon Fighting's second one dropped); off-hand damage without the
Strength bonus unless negative — NWN's Two-Weapon Fighting feat stands in
for the fighting style and keeps it. Light weapons: 2024 requires both to
be Light; proposed: require a light off-hand weapon (NWN's "light" = smaller
than the wielder) or a double weapon — alternative: any one-handed weapon.

**D2. Opportunity attacks.** NWN: moving away and casting a spell provoke
(Spring Attack and Epic Improved Combat Casting avoid them; a Tumble check
can avoid the movement one); each creature has one per round. 2024: only
leaving a creature's reach provokes, one per round (the Reaction).
Proposed: casting no longer provokes; moving keeps provoking, one per round
as now; the Tumble check stays as NWN's stand-in for Disengage until the
action-economy phase (Phase 6).

**D3. Weapon mastery — who.** 2024: Barbarian, Fighter, Paladin, Ranger and
Rogue master a few chosen weapons (2–3 at level 1, Fighters more later).
NWN has no screen for choosing. Proposed: those classes have mastery with
every weapon they are proficient with (simple, generous); alternative: a
"mastery" feat per weapon picked at level-up (like decision 12).

**D4. Weapon mastery — what.** Each NWN weapon gets its 2024 property
(dagger Nick, longsword Sap, greatsword Graze, greataxe Cleave, rapier Vex,
shortbow Vex, longbow Slow, warhammer Push, maul Topple…); NWN-only weapons
take the closest (bastard sword Sap, katana Sap, kukri Nick, kama Nick,
scythe Graze, dire mace / double axe / two-bladed sword Cleave, dwarven
waraxe Topple, throwing axe Vex, shuriken Vex, heavy flail Topple…).
Proposed build order: Vex, Sap (the advantage system already exists), Graze
and Cleave (Cleave reuses NWN's cleave attack), Nick (with D1), Topple
(after 1c), then Slow and Push (need new effects). Per-property approval
with each step.

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
    Ranged (owner, 2026-10-17): only to an enemy already in the fight — one
    charging or attacking the attacker or the party — so a bow never picks an
    idle, caged or sleeping enemy. With none, the remaining shots are lost,
    as in NWN. Range: 20 m at first; 40 m (the weapon's range) once the
    filter was in (owner, 2026-10-18), set by `ranged_retarget_range`.
