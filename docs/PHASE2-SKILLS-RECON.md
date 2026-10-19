# Phase 2 engine recon — skills, ability checks, spell attacks (NWN 1.69)

Read only, 2026-10-19 (no Ghidra: llvm-objdump + data parsing). **D** =
decoded in code/data, **I** = inferred. Addresses are nwmain.exe (single
player); the community names come from the 1.69 nwserver map.

## 1. Skill rank and skill checks

**CNWSCreatureStats::GetSkillRank(uint8 skill, CNWSObject* vs, int
bBaseRanksOnly)** — **0x6A2C00**, thiscall (this = stats), ret 0xC, returns
a signed char. Prologue `53 8b 5c 24 08 55 56 8b`. **D**:
- skill >= skill count → 0; stats +0x78 set (DM) → 127.
- ranks = stats +0x478 [skill] (int8). `bBaseRanksOnly` → ranks only.
- untrained-use flag off (skills.2da Untrained, rules +0xEC, stride 0x2C,
  +0x20) and 0 ranks → 0.
- + GetTotalEffectBonus(type 5 = skill, vs, ..., skill) (0x65E7D0) — item
  and spell skill bonuses/penalties.
- + key ability modifier (skills.2da KeyAbility: Str +0x3F9, Dex
  GetDexMod 0x6A1430, Con +0x3FD, Int +0x3FF, Wis +0x401, Cha +0x403).
- − 4 for Str/Dex skills when blind (0x6621D0) — 3e blindness rule.
- + armour + shield check penalty (stats +0x47C + +0x47D) when the skill's
  ArmorCheckPenalty flag is set (Hide, Move Silently, Parry, Pick Pocket,
  Set Trap, Tumble).
- + feats, per skill: Skill Focus +3, Epic Skill Focus +10, racial / skill
  affinity feats +2, Alertness +2 (Listen/Spot), others (long switch
  0x6A2D4E–0x6A34AC).
- clamp −127..127. No size modifier here.

**The roll:** there is no single skill-check function. Each engine use rolls
its own die, through CNWRules::RollDice 0x47E120 (`RollDice(1, 20)`) or the
CRT `rand() % 20 + 1` (0x84C0FC), and compares `rank + die` with a DC or an
opposing value. **No natural 1 / 20 rule** in any skill check found (plain
`total >= DC`). Script checks:
- **GetIsSkillSuccessful(oTarget, nSkill, nDC)** (command 689) → handler
  **0x70CC30** (prologue `6a ff 68 63 c5 89 00 64`): GetSkillRank(skill, 0,
  0) + RollDice(1,20) at **0x70CD73**, `total >= DC`, sends the skill-check
  feedback (skill name, roll, rank, DC). **D**
- **GetSkillRank(nSkill, oTarget, bBase)** (command 315) → handler around
  0x6F970F calls 0x6A2C00 (0x6F9773). **D**
- `d20()` in scripts is the VM's own random (command 102), not RollDice.

## 2. Engine uses of skills (by skill; skills.2da order verified, 28 skills — 1.69 adds **27 Ride**)

| Skill | Where (engine) | Mechanic | |
|---|---|---|---|
| 0 Animal Empathy | action 0x687E50 (0x68814E) | rand d20 + rank vs DC | D |
| 1 Concentration | OnApplyDamage 0x6686C0 (d20 0x66897E) | on damage while casting (DC from damage, I) | D/I |
| 1 Concentration | cast action 0x67C070 (0x67D24E, 0x67D50F) | d20 + rank vs computed DC (defensive casting, I) and vs **15** (entangled, I) | D/I |
| 1 Concentration | taunt 0x688787 | the taunted creature's opposed roll | D |
| 2 Disable Trap / 15 Set Trap | trap actions 0x677F2A, 0x678480, 0x678F10, 0x679830, 0x679DE0 | rand d20 (take 20 out of combat or with **Skill Mastery** feat 225) + rank vs trap DC | D |
| 3 Discipline | special attacks 0x742BC0 (0x742CFC…), 0x740650 | after a knockdown-type hit, defender (≤ Large) rand d20 + Discipline **vs the attacker's attack total** (data +0xE d20 + +0x10 modifier); lower → knockdown effect | D |
| 4 Heal | heal-kit action 0x68A010 | RollDice(n, 20) heal amount uses the rank (I) | D/I |
| 5 Hide / 17 Spot, 8 Move Silently / 6 Listen | perception: rolls cached per creature **every 6 s** at 0x65304D–0x653112 (+0xA28 Hide d20, +0xA29 Move Silently d20, +0xA2A Spot d20 in detect mode else d10 (Keen Sense feat 240), +0xA2B Listen d20); comparisons 0x653989 (Spot vs Hide) and 0x65330C (Listen vs Move Silently): rank + cached roll + area modifier (+0x14C) − 10 if in combat (+0x4BC) ± movement ±5 − distance + target size (−8 tiny … +8 huge) − the other side's skill; > 0 detects | D |
| 7 Lore | 0x65DBAB | passive (identify: rank vs SkillVsItemCost.2da) | D/I |
| 9 Open Lock | lock actions 0x72F0C0, 0x72FC60 (and 0x6FB728) | take 20 out of combat (+0x4BC clear) or with Skill Mastery, else rand d20; + rank + thieves' tools bonus (0x71E990) vs lock DC (+0x25B) | D |
| 10 Parry | ResolveAttackRoll 0x744390 (0x74464C) | parry mode: d20 + Parry vs the attack (RE-NOTES) | D |
| 13 Pick Pocket / 17 Spot | action 0x689310 | d20 + Pick Pocket vs DC; victim's d20 + Spot to notice; d100 | D |
| 14 Search | 0x6485E0 (6 sites) | d20 + Search vs trap/door DC in detect mode (+0x4A8 == 1), **d10** when not searching; DC > 35 needs a feat | D |
| 16 Spellcraft | saving throw roll 0x749230 (0x7497D7) | save bonus vs spells (passive) | D |
| 16 Spellcraft | 0x640AB8 (rand), 0x654E90 | identify / counter a spell being cast (I) | I |
| 18 Taunt | action 0x688787 | rand d20 + Taunt vs rand d20 + Concentration | D |
| 19 Use Magic Device | 0x68EDA0, 0x766210 | passive (rank vs SkillVsItemCost.2da SkillReq_Class / _Race) | D/I |
| 21 Tumble | AoO broadcast 0x655E70 (0x656268) | d20 + Tumble vs a DC from the attacker, to avoid the AoO | D |
| 21 Tumble | 0x6925E0 (GetArmorClassVersus), GetTotalACSkillMod 0x6B29A0 | AC bonus from base ranks (passive) | D |
| 11 Perform, 12 Persuade, 20 Appraise, 22–26 Craft, 23 Bluff, 24 Intimidate, 27 Ride | scripts only (bard song, dialogs, stores via scripts, crafting, horses) | — | D |

Other stats code (level-up validation, sheet message 0x62BD30, skill-point
checks) reads base ranks (0x6A01F0, 0x6A7A70, 0x6A88B0, 0x6A9120…). **D**

## 3. Ability checks

- **Engine:** the only raw ability check found is **initiative** — function
  **0x655AF0** (SEH prologue `64 a1 00 00 00 00 6a ff`): RollDice(1,20)
  (0x655B26) + Dex modifier (0x6A1430) + 8 (feat 753, Epic Superior
  Initiative) + 4 (feat 377, Improved Initiative). Combat log "Initiative
  Roll : 13 : (11 + 2 = 13)". **D**. No Strength checks (doors and
  placeables are bashed with damage vs hardness), no grapple.
- **Scripts:** dialogs gate lines on raw ability **scores**, not rolls —
  `nw_d2_intl / intn / inth` (Int low/normal/high) are in 488 / 479 / 34
  conversations; `CheckCharisma*` similar. Real rolled ability checks are
  rare: 2 scripts (`nw_i0_plot` Strength helper, `x0_s0_bigby2`).

## 4. Spell attacks (touch attacks)

- TouchAttackMelee / TouchAttackRanged (commands 146 / 147) share handler
  **0x6F1170** (prologue `6a ff 68 eb b0 89 00 64`). **D**
- They do **not** go through ResolveAttackRoll: own d20 (RollDice(1,20) at
  **0x6F12E1** melee path, **0x6F1481** ranged path), attack bonus
  **GetMeleeAttackBonus(0, 1, 1)** (0x694520) / **GetRangedAttackBonus(1,
  1)** (0x694B00), vs **GetArmorClassVersus(attacker, touch = 1)**
  (0x6925E0), the concealment roll **0x746EF0**, a natural-20 threat with its
  own confirmation roll (0x6F1385 / 0x6F14B9). **D**
- The mod already hooks GetMeleeAttackBonus / GetRangedAttackBonus (sheet,
  `attack_proficiency`), so a touch attack today is **PB + Str (melee) / Dex
  (ranged)** + effects — 2024 wants **PB + spellcasting modifier**. Our
  advantage / disadvantage and the crit changes do not reach touch attacks
  (separate d20 and crit code). In faithful mode the concealment gate patch
  (inside 0x746EF0) already removes their miss chance, but the
  disadvantage that replaces it isn't applied (no touch-attack d20 hook).
  **D/I**
- Scripts: TouchAttackMelee in 16, TouchAttackRanged in 43 scripts (spells,
  rays, breath "bolts").

## 5. Script usage in the official campaigns

4,130 scripts scanned (latest copy per name); 1,711 conversations (keys + 21
official modules), 563 of them call a skill / ability-check condition.

- **Conversation checks:** Persuade dominates — `nw_d2_persmed / perseasy /
  pershigh` in 125 / 123 / 58 dialogs; Int-score gates in ~1,000 nodes (see
  3); other skills almost never appear in dialogs (`x0_d2_sk*` generic
  conditions: 1–2 uses).
- `AutoDC(DC_x, skill, target)` (`nw_i0_plot`, used by 59 scripts):
  `GetSkillRank + d20() >= levelled DC + 10` (DC from the NPC's hit dice,
  charmed NPCs easier for Persuade / Bluff / Intimidate).
- Scripts naming each skill: Search 16, Hide 12, Open Lock 12, Persuade 11,
  Pick Pocket 9, Set Trap 9, Disable Trap 8, Move Silently 8, Listen 7, Lore
  7, Perform 5, Discipline 4 (equip scripts), Parry 4, Spot 4, Appraise 4,
  Craft Armor / Weapon 4, Ride 4, Heal 3, Spellcraft 3, Taunt 3, Intimidate
  2, Animal Empathy / Concentration 0–1, UMD 1, Tumble 1, Bluff 1, Craft Trap
  1. `GetIsSkillSuccessful` 7 scripts, `GetSkillRank` 32,
  `GetAbilityModifier` 50, `GetAbilityScore` 45.

## Summary — what is engine, script, data

- **Data (2da/tlk, ports to EE):** the skill list and key abilities
  (skills.2da), class skill lists (`cls_skill_*`), skill points
  (classes.2da SkillPointBase), Skill Focus and similar feats (feat.2da),
  Lore / UMD tables (SkillVsItemCost.2da).
- **Scripts (port):** conversation checks (AutoDC, Persuade, Int gates),
  Perform (bard song), Appraise, Craft, Bluff / Intimidate, Ride, most
  "ability checks".
- **Engine (hooks, 1.69-only):** the rank total (GetSkillRank), perception
  (cached rolls + comparisons), Search, Open Lock / traps (take 20), Tumble
  vs AoO, Discipline vs knockdown, Concentration (damage, casting), Parry,
  Taunt, Pick Pocket, Heal, initiative, touch attacks.

**Hook points we'd need:**
1. `GetSkillRank` **0x6A2C00** — one hook converts every value (engine and
   scripts): proficiency (PB if proficient / expertise ×2) instead of ranks,
   ability modifier kept, drop the 3e feat / armour / blind terms as decided.
2. Per-site d20 replacements (as the attack and save d20s) for advantage /
   disadvantage on checks: GetIsSkillSuccessful 0x70CD73; perception rolls
   0x65304D–0x653112 (2024: passive Perception = 10 + mod unless searching);
   Search 0x648A20 / 0x648A76 (+2 more pairs); Open Lock 0x72F802,
   0x7303D1; traps 0x6781B7, 0x67881B, 0x6791BF, 0x679B3C, 0x67A44F;
   Discipline 0x742D04 (+2), 0x74074D (+1); Tumble 0x65629B; Concentration
   0x66897E, 0x67D24E, 0x67D50F; initiative 0x655B26.
3. Touch attacks: the handler 0x6F1170 — d20 sites 0x6F12E1 / 0x6F1481
   (advantage, crit), and the attack bonus (PB + casting modifier via
   GetCastingAbilityModifier, already mapped for spell DCs).
4. Contested checks (2024 Shove / Grapple) have no engine counterpart;
   Discipline-vs-knockdown is the nearest (Topple/Shove could reuse it).
