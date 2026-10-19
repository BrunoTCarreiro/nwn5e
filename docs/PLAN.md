# nwn5e — full D&D 2024 (5.5e) conversion of Neverwinter Nights Diamond

Target: **NWN Diamond 1.69 (GOG), single-player**, `X:\GOG\NWN Diamond\nwmain.exe`
(PE32 x86, linked 2008-07-03, unpacked, fixed base 0x400000, no relocations).
Rules: **2024 rules as published in SRD 5.2** (CC-BY-4.0). Non-SRD 2024 material
(extra subclasses, backgrounds, feats) is personal-use only and must not ship in
anything shared.

Goal: a **full conversion** — the game plays by 2024 rules end to end, not a
"5e-flavoured" 3e.

**Possible later move to NWN: Enhanced Edition (owner, 2026-10-19; not now).**
Diamond 1.69 stays the target. When routes cost about the same, prefer the
one that would carry over: the rules in `src/rules`, 2da / tlk / texture data
and NWScript port; `nwmain_1_69.h` addresses and hooks into 1.69 client panels
do not (EE has NUI for custom windows). Mark 1.69-only work as such.

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
- Recon 2026-10-19: docs/UI-RECON.md — panels are models + `.gui` layouts +
  hardcoded client code; every sheet number is a server value; routes A–F
  (2da, tlk, art, NWScript, server hooks, client hooks) with EE portability,
  and the cheapest route per item above.

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

## Modes: faithful and NWN (owner, 2026-10-18)

Every rule decided from here on has two versions: **faithful** (D&D 2024 as
written) and **NWN mode** (2024, with NWN's own choice kept where it exists
for gameplay — NWN is a video game, and long playtests aren't planned). One
line in nwn5e.ini picks the default for all of them — `mode=faithful`
(default) or `mode=nwn` — and each rule's own key can still override it.
"NWN mode" is not the original 3e game: that stays available by switching a
rule off (its key = 0). Earlier rules (crits, attack bonus, Extra Attack,
retargeting, Phase 1b) are the same in both modes unless noted.

## Phase 2 — skills and checks: proposal for the owner (2026-10-19, not decided)

Saves (C1–C8) and spell DCs (C5) are done; what is left of Phase 2 is skills,
ability checks and spell attacks. Engine facts: docs/PHASE2-SKILLS-RECON.md.
Key fact: one function, **GetSkillRank (0x6A2C00)**, builds every skill total
the engine and the scripts use — one hook converts them all; each use then
rolls its own d20 (no shared roll), and **no skill check has a natural 1/20
rule** (already 2024). Each item: faithful (2024) and NWN mode;
recommendation first.

**SK1. The skill list.** NWN 1.69 has 28 skills (27 + Ride); 2024 has 18 plus
tool proficiencies. Recommendation (faithful): keep NWN's rows (the engine
and the official campaigns use them) but make them the 2024 skills —
Hide + Move Silently = **Stealth** and Spot + Listen = **Perception** (one
proficiency, both rows get it); Search → Investigation; Lore and Spellcraft →
Arcana; Heal → Medicine; Pick Pocket → Sleight of Hand; Tumble → Acrobatics;
Persuade → Persuasion; Bluff → Deception; Intimidate → Intimidation; Perform →
Performance; Animal Empathy and Ride → Animal Handling; Open Lock, Disable
Trap, Set Trap, Craft Trap → **Thieves' Tools**; Craft Armor and Craft Weapon
→ **Smith's Tools**; names changed through the tlk. Concentration,
Discipline, Parry, Taunt, Appraise and Use Magic Device have no 2024
counterpart (SK6–SK8). 2024's Athletics, History, Insight, Nature, Religion
and Survival have no NWN use (nothing in the engine or campaign scripts
checks them); add them only when content needs them (adding skills.2da rows
needs RE first: the per-creature rank arrays may be fixed-size). NWN mode:
NWN's 28 skills. Consequences: faithful's sheet lists fewer, merged skills;
nothing in the campaign loses a check.

**SK2. Proficiency instead of ranks.** NWN: ranks bought with skill points
each level (up to level + 3), plus Skill Focus (+3), Epic Skill Focus (+10),
racial affinities (+2), Alertness (+2), armour check penalty, −4 blind, item
and spell bonuses — totals reach +40. 2024: ability modifier + proficiency
bonus if proficient (+PB again with Expertise); a few items. Recommendation
(faithful): the GetSkillRank hook returns ability modifier + PB (proficient)
+ PB (expertise) + item and spell effects (kept, as for saves in C7); feat and
racial bonuses dropped; armour check penalty becomes 2024's **disadvantage on
Stealth** in the armours that have it (2024 armour table); blind keeps "fails
checks that need sight". Proficiency comes from class choices at level 1 (2024
counts: most classes 2, Bard / Ranger 3, Rogue 4), background (2), species
(e.g. Elf: Perception) and feats (Skilled); Expertise from Rogue 1 / 6, Bard 2
/ 9, Ranger 2 / 9. Until Phase 7 builds the level-up screens, they are chosen
with a chat command (`!skills`, like `!mastery`) and kept as local variables;
existing characters start proficient in the skills they have ranks in (then
can re-pick once). Skill points become 0 in faithful (classes.2da). NWN mode:
NWN's ranks. Consequences: faithful skill totals drop from up to +40 to at
most +17 (Expertise) — which is why SK3 is needed; Skill Focus and similar
feats stop doing anything in faithful (Phase 3 retires them).

**SK3. DCs on the 2024 scale.** NWN's fixed DCs (locks, traps, Search,
Concentration, scripted checks) are set for 3e totals; 2024 DCs run 5–30.
Recommendation (faithful): convert fixed DCs where the engine and
GetIsSkillSuccessful compare them, as C6 converts save DCs — **DC 10 + (3e DC −
10) / 2**, rounded (3e 20 → 15, 30 → 20, 40 → 25, 50 → 30); opposed checks
(Hide vs Spot, Pick Pocket vs Spot, Taunt) need no conversion (both sides
change). The campaign's AutoDC (levelled Persuade / Bluff / Intimidate DCs in
59 scripts) goes through the same conversion. Lore and Use Magic Device's
identify table (SkillVsItemCost.2da, by rank) gets compressed to the new
totals (data). NWN mode: NWN's DCs. Consequences: locks and traps stay
openable by trained characters; untrained ones fail much more often (2024's
intent).

**SK4. Natural 20 / 1 on checks.** NWN has none on skill checks; 2024 has
none on ability checks. Nothing to change (both modes).

**SK5. Stealth against Perception.** NWN: every 6 s each creature caches a
Hide d20, a Move Silently d20, and a Spot / Listen roll — d20 in detect mode,
**d10** otherwise — compared with distance, area, size (−8 tiny … +8 huge) and
"in combat −10" modifiers. 2024: the hider rolls Stealth; others notice it with
**passive Perception (10 + modifier)** or an active Perception check (a
search). Recommendation (faithful): keep the hider's cached roll; observers
not in detect mode use 10 instead of a d10 roll (passive Perception), detect
mode keeps the d20 (searching); drop the size modifier (not 2024); keep
distance and area modifiers (needed for NWN's real-time perception; 2024 uses
light and cover instead). NWN mode: NWN's. Consequences: watchful guards
become more consistent (no lucky d10 of 1); small creatures lose their hiding
bonus.

**SK6. Knockdown and Disarm (Discipline).** NWN: after a Knockdown / Improved
Knockdown hit the defender (Large or smaller) rolls d20 + Discipline against
the attacker's attack total, and is knocked down on a loss; Disarm works the
same way. 2024: **Shove** (an Unarmed Strike option) — the target makes a
Strength or Dexterity saving throw (its choice) against DC 8 + the attacker's
Strength modifier + PB, or is pushed 5 ft or knocked prone. **SRD 5.2 has no
Disarm at all** (the only disarming is the Battle Master's Disarming Attack,
a Player's Handbook subclass outside the SRD). Recommendation (faithful): the
Knockdown feats keep their attack roll; on a hit the target makes the better
of a Strength or Dexterity save (with our 2024 save values) against 8 +
Strength modifier + PB instead of the Discipline check; **Disarm does nothing
in faithful** (option A; B = keep it as a house rule with a Strength save
against the same DC; C, NWN's Discipline check, is not possible once SK1
removes the skill) — a PC who took it gets the choice back in Phase 3. NWN
mode: Discipline for both. Consequences: knockdown works on high-Discipline
monsters by their saves instead; Topple and Shove become one rule; NPCs with
Disarm stop disarming in faithful. *(Corrected 2026-10-19: the first draft
recommended a Strength save for Disarm, which is the Battle Master maneuver,
not SRD 2024.)*

**SK7. Concentration (the skill).** NWN: a Concentration check on damage
while casting (DC from the damage), for defensive casting, when entangled (DC
15) and against Taunt. 2024: casting is not interrupted by damage; only
maintaining a concentration spell needs a Constitution save (Phase 4); no
check to cast while restrained; no defensive casting (casting draws no
opportunity attacks, D2). Recommendation (faithful): remove the damage and
entangled casting checks; defensive casting has nothing left to do; the
concentration save comes with Phase 4. NWN mode: NWN's. Consequences: casters
in melee are no longer interrupted — a big buff to casters in melee, as in
2024 (balanced in 2024 by concentration and by spells being actions).

**SK8. The 3e-only skills.** Parry (parry mode), Taunt, Appraise, Use Magic
Device. 2024 has none of them. Recommendation (faithful): parry mode does
nothing until Phase 3 adds the Defensive Duelist feat (2024's Parry
reaction: +PB AC with a finesse weapon); Taunt has no 2024 equivalent and
is left out of faithful (it stays in NWN mode); Appraise dropped (stores use list
prices); Use Magic Device dropped (items usable by class as in 2024; Phase 4
decides scrolls). NWN mode: NWN's. These touch few campaign scripts (Appraise 4,
UMD 1, Taunt 3, Parry 4).

**SK9. Initiative and ability checks.** NWN: initiative = d20 + Dex + 4
(Improved Initiative) + 8 (Epic Superior Initiative); no other raw ability
checks in the engine; dialogs gate on ability *scores* (about 1,000
Int-gated lines), not rolls. 2024: initiative is a Dexterity check (+PB with
the Alert feat); advantage / disadvantage apply. Recommendation (faithful):
initiative = d20 + Dex modifier, Improved Initiative → Alert (+PB), Epic
Superior Initiative → dropped until Phase 3 sorts epic feats; leave the
score-gated dialog lines alone. NWN mode: NWN's.

**SK10. Spell attacks.** NWN: TouchAttackMelee / Ranged (in 59 spell
scripts) roll their own d20 with the melee / ranged attack bonus (today PB +
Str / Dex after attack_proficiency) against touch AC, with NWN's threat and
confirmation. 2024: spell attack = d20 + PB + spellcasting modifier against
AC; natural-20 crit, no confirmation; advantage and disadvantage apply.
Recommendation (faithful): the touch attack uses PB + the casting ability
(the same ability C5 uses for DCs), our roll mode (advantage / disadvantage,
conditions) and our crit rules; against normal AC or touch AC is SK10b — 2024
has no touch AC (spell attacks go against full AC; recommendation: full AC).
NWN mode: NWN's (with the global attack conversion). Consequences: rays and
touch spells hit by the caster's casting ability; armoured targets become
harder to hit with rays (full AC).

**SK11. Conditions on checks** (completes CN4 / CN5): poisoned and frightened
give disadvantage on ability checks, exhaustion later on d20 tests.
Recommendation: both, once the check rolls are hooked (SK2 / SK5 / SK9).

Build order if accepted: SK2 + SK1 (GetSkillRank hook, `!skills`, proficiency
storage) → SK3 (DC conversion at GetIsSkillSuccessful and the engine sites) →
SK10 (touch attacks) → SK6 / SK7 → SK5 → SK9 → SK8 / SK11. Rough size: SK2+SK1
two sessions, SK3 one, SK10 one, SK6+SK7 one, SK5 one, SK9+SK8+SK11 one. Ports
to EE: the rules (src/rules) and the data / tlk parts; the hooks are 1.69-only.

## Conditions — decided by the owner (2026-10-19)

**Decision (owner, 2026-10-19): the recommendations below, as written**, with
CN8 revised after decoding the engine (RE-NOTES "Condition effects"):
Deafened leaves CN8 — faithful drops NWN's 20% spell failure (it only ever hit
Bard / Sorcerer / Wizard spells, and Still Spell avoided it), keeps "can't
hear"; NWN mode unchanged. Petrified stays NWN for now; resistance to all
damage is a later decision. CN7 is built with Phase 5. Build order: CN8
Deafened, then CN3/CN2 saves, CN2 crits, CN4, CN6, CN1, CN5 (by size and
risk, adjusted after recon).


The last open Phase 1 item. Phase 1b (B1) already turned NWN's attack terms
for a blind, stunned or prone target, an invisible attacker and an unseen
target into advantage/disadvantage. Everything else about conditions is still
NWN's. "NWN today" is from NWN's own rules; where marked *(confirm)* the
engine code is not decoded yet and is checked before building. Each item has
a faithful and an NWN-mode version; recommendation first.

*Built 0.9.11–0.9.12 (2026-10-19):* CN8 Deafened, CN1, CN2 (advantage both
modes — NWN gives paralysed / sleeping targets no attack bonus, only
flat-footed — saves, 5 ft crits), CN3, CN4, CN6. Recon corrections: Evard's
Black Tentacles applies paralysis (CN2), not entangle; NWN mode's CN2
"advantage only" is new to NWN mode for the same reason. Still open (owner):
CN5 (the generic monster AI refuses to fight while frightened — options in
RE-NOTES "Conditions: hook points"), NWN's coup de grace against sleeping /
paralysed creatures of level 4 or less (automatic hit and kill; 2024 has
none), and whether paralysed / sleeping targets keep NWN's flat-footed
Dexterity loss on top of advantage (they do for now).

*Decided (owner, 2026-10-19), after the recon:* **CN5** faithful replaces
NWN's fear with nwn5e's own: the frightened state never takes hold (so no
fleeing, no loss of control, no -2 saves, and NWN's monster AI keeps
fighting); the creature gets the fear icon for the effect's duration, from
its source, and attacks with disadvantage while it can see that source.
Remove Fear and similar cures don't end it early (known gap). NWN mode keeps
NWN's fear. **Coup de grace:** faithful drops it (the 5 ft critical hit
replaces it); NWN mode keeps it. **Flat-footed Dexterity loss** on paralysed
and sleeping targets stays in both modes until Sneak Attack is converted
(Phase 3). **Push:** built as option A of the recon (RE-NOTES "Push").
*Built 0.9.13:* CN5 (OnApplySetState hook), no coup de grace, Push.

**CN1. Restrained** (Entangle, Web, Evard's Black Tentacles). NWN today: held
in place, plus 3e's −2 to its attacks and −4 Dex *(confirm)*. 2024: speed 0;
attacks against it have advantage, its attacks disadvantage, disadvantage on
Dex saves. Recommendation: faithful = the 2024 effects, NWN's flat penalties
removed; NWN mode = NWN's. Consequences: faithful makes webs and tentacles
much more dangerous — everyone hits restrained creatures easily (both ways:
an entangled PC is easy prey) — NWN mode unchanged.

**CN2. Paralyzed, asleep, held (helpless).** NWN today: can't act; attackers
get a bonus *(confirm: +2 "stunned" term or more)*. 2024 (Paralyzed /
Unconscious): incapacitated; auto-fail Str and Dex saves; attacks against it
have advantage; a hit from within 5 ft is a critical hit. Recommendation:
faithful = advantage (already, if the engine marks it as stunned), plus
auto-fail Str/Dex saves and melee hits become crits; NWN mode = advantage
only. Consequences: faithful makes Hold Person / Sleep on a fighter close to
a death sentence (every melee hit doubles its dice) and the same for the
party's paralyzed foes — the 2024 power level of hold spells.

**CN3. Stunned.** NWN today: can't act, +2 for attackers (now advantage).
2024 adds: auto-fail Str and Dex saves. Recommendation: both modes add the
auto-fail (it is part of being stunned in either). Consequences: stunned
targets can't dodge fireballs; small change.

**CN4. Poisoned.** NWN today: poison is ability damage (poison.2da), with no
ongoing penalty. 2024: Poisoned = disadvantage on attack rolls and ability
checks (poisons also deal damage). Recommendation: faithful = while a poison
effect is on a creature, disadvantage on its attack rolls (ability checks
when Phase 2 adds them); NWN's ability damage stays until Phase 8 converts
the poisons; NWN mode = NWN's. Consequences: poison bites (spiders, wyverns)
matter during the fight, not only after; poisoned PCs miss more.

**CN5. Frightened.** NWN today: the creature flees and can't act — a
frightened PC loses control for the duration. 2024: disadvantage on attack
rolls and ability checks while the source is in sight, and it can't move
closer to the source; it still acts. Recommendation: faithful = no fleeing;
disadvantage on attacks while it can see the source (moving closer is not
enforced — NWN's AI and pathing can't easily express it); NWN mode = NWN's
fleeing. Consequences: faithful keeps you in control under fear but fear
spells (Scare, Fear, dragon fright) become weaker against monsters, which keep
attacking at disadvantage instead of running.

**CN6. Concealment and miss chances** (Invisibility, Blur, Displacement,
Darkness, blindness). NWN today: a percentage miss chance (20–50%) after the
attack hits. 2024 has no miss chance: an invisible or unseen target already
gives disadvantage (B1); Blur and Displacement give disadvantage on attacks
against you. Recommendation: faithful = no miss chance; concealment from
effects becomes disadvantage on attacks against that creature (one source,
not stacking with the unseen one); NWN mode = NWN's miss chance. Consequences:
faithful removes the "hit… but missed (50%)" double roll; invisibility gets a
little weaker (disadvantage ≈ −4 instead of halving the hits), Blur
Displacement about the same.

**CN7. Exhaustion.** NWN today: none. 2024: levels 1–6, −2 per level to every
d20 test, −5 ft speed per level, death at 6; a long rest removes one.
Recommendation: build it with the Phase 5 rest rework (2024 sources are
mostly forced marches, no rest, a few spells); nothing in the campaign applies
it yet. Both modes the same.

**CN8. Keep as NWN** (no change, both modes): Charmed and Dominated (NWN's
"becomes friendly / controlled" is close to 2024; on the player both are a
daze, by NWN's design), Petrified (same as paralysis plus the stone look; no
damage resistance — later decision), Dazed (= 2024 Incapacitated), Confused
(random AI), Grappled (no grapple in NWN; Phase 6). *Revised 2026-10-19:*
Deafened — faithful drops the 20% arcane spell failure; NWN mode keeps it.

## Phase 1c/1d and racial bonuses — decided by the owner, 2026-10-18

Walked through one by one (issue, what NWN does, what 2024 does, what to
expect). Engine facts: RE-NOTES "Saving throws" and "Two-weapon fighting and
attacks of opportunity". Owner: "both versions, faithful as default" for
each, except where noted.

**R1. Racial combat bonuses.** NWN: Battle Training (+1 to hit vs orcs and
goblinoids for dwarves, vs reptilians for gnomes), halfling Good Aim (+1
thrown) and +1 with slings, Defensive Training (+4 AC vs giants). Faithful:
all removed (the attack then always matches the sheet); the 2024 species
traits replace them (the save ones with C3, the rest in Phase 3). NWN mode:
kept.

**C1. Which save.** Faithful: Fortitude → Con, Reflex → Dex, Will → Wis by
default, plus a per-spell table switching to the 2024 spell's ability
(Banishment → Cha, Feeblemind → Int, Entangle → Str…), filled as spells are
converted (Phase 4); needs the spell behind a save (recon). NWN mode: the
three saves with the default mapping only (computed the 2024 way). The
original 3e saves stay available by switching the save rule off.

**C2. Save proficiency.** Faithful: characters and henchmen — the two saves
of their first class (2024; later class features add more: Monk 14, Rogue
15); creatures — ability modifier only (most 2024 monsters have none); NPCs
built with real classes count as characters. Bosses get Legendary
Resistance with the monster conversion (Phase 8). NWN mode: characters the
same; creatures proficient in every save their NWN tables make "good".

**C3. 3e save bonuses.** Faithful: flat feat and racial bonuses dropped;
Great Fortitude / Lightning Reflexes / Iron Will → proficiency in Con / Dex
/ Wis saves (like 2024 Resilient); Divine Grace and Dark Blessing → Aura of
Protection (Charisma modifier, min +1, to saves of the paladin and allies
within 3 m, from paladin level 6; the Blackguard counts as a paladin);
Luck of Heroes and the creature blueprint save bonus dropped; species get
2024 advantages by save type — dwarf vs poison, halfling vs fear plus
rerolling natural 1s on d20 tests (Luck), elf vs charm (and sleep immunity
as Trance), gnome on Wis (later Int/Cha) saves. Needs advantage on the save
roll's d20. NWN mode: the same base and advantages, plus NWN's flat feat and
racial bonuses and the blueprint bonus; Divine Grace as NWN (self, from
level 2).

**C4. Natural 1 / 20 on saves.** Faithful: no automatic success or failure.
NWN mode: NWN's — a natural 20 always saves, a natural 1 fails when the game
option "Saving Throw Automatic Failure On 1" is on.

**C5. Spell DC.** Faithful: class casters 8 + proficiency bonus + casting
modifier (spell level and Spell Focus no longer count); spells from items
use the 2024 spell-scroll DCs by spell level (13 for 0–2, 15 for 3–4, 17 for
5–6, 18 for 7–8, 19 for 9); monster abilities 8 + their CR proficiency bonus
+ their Charisma modifier (instead of NWN's fixed 3). NWN mode: the same,
with Spell Focus / Greater / Epic as +1 / +2 / +3.

**C6. Other DCs** (traps, poisons, diseases, item powers, scripted monster
abilities, module scripts) — both modes: converted when the save is rolled,
new DC = 10 + (old DC − 10) / 2, rounded, kept within 10–30 (14 → 12, 20 →
15, 26 → 18, 30 → 20, 40 → 25); spell DCs (C5) excluded. Phase 8 replaces
the formula with exact 2024 values for the main sources.

**C7. Bonuses inside the save roll.** Faithful: item save bonuses kept
(balance pass in Phase 8); spell effects kept until each spell is converted
(Phase 4: Bless → +1d4, Protection from Evil → advantage…); feat and racial
"+2 vs X" dropped (C3's advantages replace them); immunities kept. NWN mode:
all kept.

**C8. Character sheet.** Both modes: the three numbers show the 2024 saves.
Faithful: the labels are swapped in memory ("Fortitude / Reflex / Will" →
"Constitution / Dexterity / Wisdom") wherever the game shows them, sheet and
combat log included — no game files changed; needs the text lookup (recon).
NWN mode: NWN's labels. All six saves: a `!saves` chat command prints them
with the proficient ones marked; three more rows on the sheet itself are
looked into in Phase 7.

**D1. Two-weapon fighting.** Faithful: no attack penalty on either hand; one
off-hand attack per round; off-hand damage without the ability modifier
unless negative (NWN's Two-Weapon Fighting feat = the 2024 fighting style,
adds it back); both weapons must be Light (club, dagger, handaxe, light
hammer, sickle, scimitar, shortsword, plus kukri and kama); Improved
Two-Weapon Fighting → Dual Wielder (any one-handed weapon, still one extra
attack); Ambidexterity does nothing; double weapons give the one extra
attack. NWN mode: NWN's rules unchanged (penalties, feats, Improved TWF's
second attack, half Strength).

*Built 0.8.5 (2026-10-18).* Same as decided. Found on the way, for a later
decision: NWN also adds 1.5 × Strength to damage with two-handed weapons and
+1 damage within 5 m with Point Blank Shot (both 3e; 2024 has neither).

**D2. Opportunity attacks.** Faithful: casting no longer provokes; moving
away still does, once per round per creature; Combat Reflexes gives no extra
ones and Spring Attack no longer protects (feat rework); Tumble stays as the
way out until Phase 6 adds Disengage. NWN mode: NWN's rules unchanged.

*Built 0.8.3 (2026-10-18), with what the recon found:* NWN already refills
each creature's one AoO at the end of every round, and 1.69 has no Combat
Reflexes, so "once per round" and "no extra ones" hold in both modes without
code. NWN also provokes on four things the decision didn't list — drinking
a potion and every ranged shot (the party is exempt from both at the easier
difficulties), unarmed attacks without Improved Unarmed Strike, and Disarm.
None provoke in 2024 (a potion is a Bonus Action; a ranged attack next to an
enemy has disadvantage instead, B5), so faithful removes
them too, in keeping with "only moving away provokes"; NWN mode keeps them.

**D3. Weapon mastery — who.** Faithful: the 2024 counts (Fighter 3/4/5/6 at
1/4/10/16, Barbarian 2/3/4 at 1/4/10, Paladin, Ranger, Rogue 2); chosen with
chat commands (`!mastery`, `!mastery <weapon>`, swap one after a long rest);
empty slots fill with the weapon kinds actually used (announced in chat);
Weapon Focus also gives mastery of its weapon for any class (2024 Weapon
Master stand-in); later the choice may move into level-up (Phase 3/7). NWN
mode: the mastery classes have mastery with every weapon they are
proficient with; Weapon Focus the same.

*Built 0.8.7 (2026-10-18).* Choices made on the way: several mastery classes
give the larger count, not the sum (like Extra Attack; prestige levels add to
the parent class); a swap is `!mastery <old> for <new>` (either order since
0.8.8: the weapon already in a slot is the one given up) and becomes available
when a rest finishes (also the ForceRest script command); filling an empty
slot by `!mastery <weapon>` is free; only weapons the character is proficient
with (baseitems.2da ReqFeat) can be chosen or auto-filled, unarmed never;
a magic staff counts as a quarterstaff; henchmen fill their slots by use only
(the player is told). The choices are local variables on the creature
(`nwn5e_mastery_1`…`6`, `nwn5e_mastery_swap`), so they live in the save game.

**D4. Weapon mastery — what.** Each NWN weapon gets its 2024 property
(`src/rules/weapons.cpp`; NWN-only weapons take the closest). Faithful: all
eight, automatic whenever their condition holds — Vex and Sap (advantage /
disadvantage), Graze (damage on a miss), Cleave (NWN's cleave attack on a
hit), Topple (Con save or knockdown, after C1–C4), Slow (−33% speed for a
round), Push (3 m knockback; last, possibly approximated); Nick waits for
the action economy (Phase 6); monsters don't use mastery. NWN mode: the
same without Push.

*Built 0.9.0–0.9.7 (2026-10-18).* Vex, Sap, Graze, Slow, Topple and Cleave
work and are verified in game (README has the evidence). Choices made on the
way: a "hit" is attack result 1/3/7/10 and Vex/Sap last in NWN combat rounds
(an NWN round is a 2024 turn); Graze, Slow and Topple effects land when the
swing does (queued like the engine's own hit damage); Topple's knockdown lasts
a round; Cleave's extra attack is the engine's own cleave attack, aimed
through the round's target as Great Cleave does. Known gap: Vex and Slow also
trigger on hits that deal 0 damage (immune targets; 2024 needs damage).

*Push parked (owner, 2026-10-18), with this design for later:* on a Push hit
(target Large or smaller, faithful only) the target plays NWN's knockdown
(falling) animation as a visual only — no knockdown effect, no prone, it gets
straight back up — and during the fall is moved up to 3 m straight away from
the attacker, through the engine's own jump (the path scripts'
JumpToLocation uses), onto walkable ground only and stopping short at walls.
Recon needed: playing the fall animation without the knockdown state, and the
jump / safe-location call.
*Built 0.9.13 (2026-10-19), option A of the recon:* on the hit's landing
(OnApplyDamage), the target moves up to 3 m (0.5 m steps) straight away onto
walkable ground with 0.5 m clear behind it, then plays the knockdown fall as a
visual through a 1.2 s round pause (shortened by as much, as the hit flinch
does); warhammer and heavy crossbow. To check in game: whether NPCs slide or
snap over the move, the fall's length and direction.
*0.9.14 after the owner's test (2026-10-19):* the one-step move made the
client slide the model after the PC had already reacted, losing the fall;
pushed goblins could end inside a fountain (placeables aren't in the tile
walkmesh). Now the fall starts at once and the target slides along the line
during it (AIUpdate steps), the line must also clear the area's static
objects (placeables), and — owner request — when another enemy is then
closer than the pushed one, the attacker switches to the nearest (in place
for the round, made to stick at its end). Known gap: closed doors are not
checked.
*0.9.15 after the second test (2026-10-19):* the fall sometimes did not
play (the round could not pause), the slide was cut short (the target walked
back toward the attacker) and retargeting never fired. Now the target cannot
move for the fall (an AI-state effect, as entangle uses), the fall plays even
without a round pause, and the retarget is a logged scan of the enemies (each
decision and the round-end check are in nwn5e.log).
*0.9.16 (owner's third test, 2026-10-19):* retargeting works (logged
"attacker now attacks the closer", kept at the round end); the slide is
0.25 s. The "AoO from the push" was the pushed goblin running back: NWN
provokes from every enemy a runner passes, also when running toward them;
D2's faithful rule ("only moving away provokes") now holds for that too.
*0.9.17-0.9.18:* slide 0.15 s (owner). The remaining "swing at the pushed one"
was not an AoO (none logged, no AoO text) but the round's next swing, aimed
before the hit landed; the retarget now happens at the attack roll (the
landing still moves the target).
*0.9.19:* at the roll the engine's attack-order fields are still empty
(round target and queued attack both 0x7F000000), so 0.9.18 never switched
and the PC walked after the pushed one (drawing AoOs from the other goblin).
Now the switch is decided at the roll from the attack's own target (round
target set, as after a kill), the order is rewritten again when the hit lands
(by then it names the pushed one), and the round-end check stays.
*0.9.20:* the 0.9.19 logs show the switches held, but twice the PC stood idle
after a push (once following with no switch, once after a switch) while the
goblin attacked it: its attack order was dropped. For 8 s after a push the
attacker is watched: its order changes are logged, and if it is left with no
order at all while its intended target stands, the attack is reissued (as a
click). Corpses are never chosen (the scan skips the dead).
*0.9.21:* 0.9.20 test: retargeting held on all 8 pushes, no reissue needed;
the PC lost its target once, after drinking a potion (outside the watch;
probably NWN clearing the attack order on item use, unverified). The fall
sometimes did not show though the server set it: now the target stops
moving at once (AI-state bit cleared directly as well as by the effect) and
the fall animation is put back after each of its AI updates for the fall
s length (the first override is logged with the animation that replaced it).
*0.9.22 (owner request, 2026-10-19):* after a potion mid-fight the PC goes back
to attacking (`resume_attack_after_potion`, both modes). The creature-update
hook (AIUpdate) moved to creature_update.cpp, shared by Push and this.
*Hazard test (owner, 2026-10-19, 0.9.22):* a Caltrops field (permanent area
of effect, on-enter 1 damage after 1.0-2.2 s, the same every round) — two
pushes into it gave 1 damage 1.4 s and 1.8 s later; per-round ticks 6 s
apart cannot explain both, so a pushed creature counts as entering an area
of effect (the trigger update after SetPosition). Web from the Cloak of
Arachnida lasts 1 round (caster level 3 / 2), too short to test with.

**E1. Two-handed Strength (owner, 2026-10-18).** NWN adds 1.5 x the Strength
modifier (truncated) to two-handed melee damage; 2024 adds it once. Faithful:
once; NWN mode: 1.5 x. Point Blank Shot's +1 damage stays with B3 (Phase 3).
*Built 0.9.8.*

**C1 table (owner, 2026-10-18): option A.** Faithful: the NWN spells that exist
in SRD 5.2 with another save ability use it — Strength (Entangle 53, Gust of
Wind 75, Evard's Black Tentacles 375), Intelligence (Feeblemind 55), Charisma
(Bane 449, Banishment 430, Lesser/Planar/Greater Planar Binding 96/128/69),
Constitution (Cone of Cold 25, Sunbeam 183, Sunburst 427, Inflict Minor to
Critical Wounds 431-435 and the Blackguard's 611/612). NWN-only spells
(Dismissal and the like) keep the default mapping; NWN mode has no table.
The spell is the running script's (OBJECT_SELF +0x188, an area of effect's
own +0x1CC), so lingering clouds use their own spell. Effects on the NWN save
(items, spells) still apply to the overridden one. *Built 0.9.8.*

### Build order

1. The mode switch (`mode=faithful|nwn`, per-rule keys override).
2. R1 — remove the racial combat bonuses (small).
3. Saves (0.8.x): save values (C1 default, C2, C3 numbers) → save roll (C4,
   C3 advantages, C6 conversion) → spell DC (C5) → `!saves` and labels (C8)
   → the per-spell save table scaffold (C1).
4. D2 — opportunity attacks (small).
5. D1 — two-weapon fighting.
6. D3 then D4 — mastery choice (`!mastery`), then Vex, Sap, Graze, Cleave,
   Topple, Slow, Push.
7. Follow-ups noted for later phases: Legendary Resistance (8), Disengage
   (6), sheet rows (7), species traits (3), spells (4).

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
