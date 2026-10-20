# Phase 3 recon — character model (NWN 1.69)

Read only, 2026-10-19 (two recon passes: game data, engine code). **D** =
decoded from data or code, **I** = inferred, **UNSURE** = not confirmed.
Addresses are nwmain.exe (single player: client and server in one process).

## 1. Data (2da, shared by both modes)

**Base classes** (classes.2da, PlayerClass 1, no prerequisite table; D):

| Row | Class | HD (2024) | Skill pts | Attack | Caster | Primary | SRD 5.2 |
|---|---|---|---|---|---|---|---|
| 0 | Barbarian | d12 (d12) | 4 | ATK_1 | – | Str | Barbarian |
| 1 | Bard | d6 (**d8**) | 4 | ATK_2 | spgn+spkn | Cha | Bard |
| 2 | Cleric | d8 (d8) | 2 | ATK_2 | spgn | Wis | Cleric |
| 3 | Druid | d8 (d8) | 4 | ATK_2 | spgn | Wis | Druid |
| 4 | Fighter | d10 (d10) | 2 | ATK_1 | – | Str | Fighter |
| 5 | Monk | d8 (d8) | 4 | ATK_2 | – | Dex | Monk |
| 6 | Paladin | d10 (d10) | 2 | ATK_1 | spgn | **Wis** (2024 Str/Cha) | Paladin |
| 7 | Ranger | d10 (d10) | 4 | ATK_1 | spgn | Wis | Ranger |
| 8 | Rogue | d6 (**d8**) | 8 | ATK_2 | – | Dex | Rogue |
| 9 | Sorcerer | d4 (**d6**) | 2 | ATK_3 | spgn+spkn | Cha | Sorcerer |
| 10 | Wizard | d4 (**d6**) | 2 | ATK_3 | spgn | Int | Wizard |
| – | — | | | | | | **Warlock (d8): new** |

- XPPenalty 1 on every base class (3e multiclass penalty; favored class in
  racialtypes). MaxLevel 0, EpicLevel -1. Monster classes rows 11–26 and 38;
  **rows 39–40 are blank** (I: room for Warlock).
- Prestige classes (rows 27–37, 41): Shadowdancer, Harper Scout (max 5), Arcane
  Archer, Assassin, Blackguard, Champion of Torm, Weapon Master, Pale Master,
  Shifter, Dwarven Defender, Red Dragon Disciple, Purple Dragon Knight (max 5);
  EpicLevel 10; each cls_pres_* has a **VAR row** (X1_AllowArcher, X2_AllowWM,
  DLA_AllowPDK…, set by x0_d1_make* scripts on the PC). No prerequisite type
  is an ability score (the 2024 13-in-primary rule needs a hook).
- 1.69 NWScript has no CLASS_TYPE for a new class: no campaign gate can name a
  Warlock.

**Progression (D):** exptable.2da = 500·L·(L−1) (L20 190,000; cap 40). 2024:
0 / 300 / 900 / 2,700 / 6,500 / 14,000 / 23,000 / 34,000 / 48,000 / 64,000 /
85,000 / 100,000 / 120,000 / 140,000 / 165,000 / 195,000 / 225,000 / 265,000 /
305,000 / 355,000. Kill XP = xptable.2da (level × CR) × module Mod_XPScale (OC
and Prelude 10, SoU 15, HotU 150 then 100). Quest XP is scripted (553 scripts).
Hard-coded campaign XP: Prelude 1,000 and 3,000; HotU 105,000 (NWN level 15)
then forces level 15. cls_bfeat_* = bonus feat slots per class level (Fighter
1, 2, 4, 6…; Wizard 5, 10, 15, 20; Rogue 10, 13, 16, 19; Ranger favored
enemies 1, 5, 10…). General feats (level 1 and every 3 levels, + human) and
ability increases (every 4 levels) are counted by **total** level and are not
in data (engine).

**Class features (cls_feat List 3, D) vs 2024:** Barbarian (Rage ✓, Fast
Movement ✓, Uncanny Dodge ✗ Rogue-only in 2024, DR ✗); Bard (Bard Song ≈
Bardic Inspiration); Cleric (Turn Undead ≈ Channel Divinity; 19 domains at
level 1, no Life domain — 2024 Life Domain subclass at 3); Druid (Wild Shape ✓
at 2; companion ≈); Fighter (only bonus feats — Fighting Style, Second Wind,
Weapon Mastery, Action Surge, Indomitable all new); Monk (most have analogues;
Wholeness of Body and Quivering Palm are the SRD Open Hand subclass); Paladin
(Lay on Hands ✓, Smite ≈, Divine Grace ≈ Aura of Protection, Courage ✓);
Ranger (Dual-Wield → Fighting Style; Favored Enemy → Hunter's Mark; companion
= Beast Master, **PHB-only**); Rogue (Sneak Attack identical, Evasion ✓,
Uncanny Dodge ✓, Slippery Mind ✓, Skill Mastery ≈ Reliable Talent); Sorcerer
(metamagic as feats; familiar ✗); Wizard (Scribe Scroll, familiar, school at
1; 2024 Arcane Recovery, Scholar, Evoker at 3).

**Feats (feat.2da, D):** 1,116 rows in the 1.69 copy (xp2patch.bif, which the engine loads; corrected 2026-10-20 by CM0 — the count was 1,083); ALLCLASSESCANUSE 502 (210 non-epic);
MinLevel / MinLevelClass exist (usable for subclass feats at class level 3).
Background feats (Artist, Blooded, Bullheaded, Courteous Magocracy, Luck of
Heroes, Silver Palm, Snake Blood, Strong Soul; Stealthy and Thug without
MaxLevel) are ordinary level-1 general feats — no background step. SRD 5.2
feats (verified): Origin — Alert (≈ Improved Initiative), Magic Initiate,
Savage Attacker, Skilled; General — Ability Score Improvement, Grappler;
Fighting Style — Archery, Defense, Great Weapon Fighting, Two-Weapon Fighting;
Epic Boons — Combat Prowess, Dimensional Travel, Fate, Irresistible Offense,
Spell Recall, the Night Spirit, Truesight. Most have no NWN counterpart.
**PHB-only** analogues of NWN feats: Tough (Toughness), Mobile (Mobility),
Great Weapon Master (Power Attack), War Caster (Combat Casting), Mounted
Combatant, Dual Wielder (Ambidexterity), Sharpshooter (Rapid Shot).

**Races (racialtypes.2da, D):** Dwarf, Elf, Gnome, Halfling, Half-Elf,
Half-Orc, Human (Quick to Master). SRD 5.2 species: Dragonborn, Dwarf, Elf,
Gnome, Goliath, Halfling, Human, Orc, Tiefling — missing in NWN: Dragonborn,
Goliath, Orc (row 14 "Orc" is the monster type), Tiefling; Half-Elf and
Half-Orc have no SRD species. Ability adjustments look runtime-applied
(pregens of different races store identical scores; I, strong).

**Backgrounds (SRD 5.2, verified):** Acolyte (Int / Wis / Cha, Magic Initiate
(Cleric), Insight + Religion, Calligrapher's Supplies), Criminal (Dex / Con /
Int, Alert, Sleight of Hand + Stealth, Thieves' Tools), Sage (Con / Int / Wis,
Magic Initiate (Wizard), Arcana + History, Calligrapher's Supplies), Soldier
(Str / Dex / Con, Savage Attacker, Athletics + Intimidation, a Gaming Set).

**Henchmen (D):** OC henchmen swap to pre-built blueprints (cap 14). SoU / HotU
level through LevelUpHenchman with packages (packages.2da: 130 rows), taking
prestige classes (Deekin RDD, Valen WM, Tomi SD, Nathyrra Assassin, Aribeth
Blackguard); HotU's prologue levels the PC to 15 the same way. A failed level
falls back to the other class, then "Level Up Failed For …".

**Campaign checks (keys + the 14 modules' 14,830 scripts):** GetLevelByClass
408 scripts (~230 dialogs, mostly "> 0"), GetRacialType 226 (~119; Half-Elf
25, Half-Orc 23 scripts), GetAbilityScore 213 (~142; Int, Cha), GetHasFeat 83,
GetHitDice 227, GiveXP / SetXP 110.

## 2. Engine

- **Rules object** [0x92DC64]: +0x1C XP[41] (exptable, built at 0x47A65B),
  +0xE4 classes (stride 0x268: +0x5C HitDie, +0x5D SkillPointBase, +0x1ED+lvl
  bonus feats, +0x250 PlayerClass, +0x258 MaxLevel, +0x25C XPPenalty, +0x266
  EpicLevel), +0xE8 races (stride 0x34: +0x1A Int adjustment, +0x1F favored
  class). Server options = [[0x92DC50]+4] vtable +0x54: +0xB0 MinCharLevel,
  +0xB4 MaxCharLevel, +0xF0 Enforce Legal Characters (off in single player),
  +0x118 Max Hit Points.
- **Stats:** +0x28/+0x2C level history (CNWLevelStats: +0xF0 feats, +0x102
  ability bump (6 = none), +0x103 HP gained, +0x104 class), +0x70 XP, +0xA1
  class count, class slots at +0xA4 + i·0x118 (+0x1B2 id, +0x1B3 levels),
  +0x476 unspent skill points.
- **XP → level:** XP never sets the level; CanLevelUp 0x6A8440 (PC: XP >=
  XP[level], total < 20 / 40 with HotU, < MaxCharLevel). AddExperience
  0x6A2030 (multiclass penalty: −20 % per class ≥ 2 levels below the highest,
  favored class excepted) → SetExperience 0x6A1C90 (level loss replays the
  history). GiveXPToCreature 0x6FF3F0 → AddExperience; SetXP 0x6FF4A0 →
  SetExperience (no penalty). Kill XP 0x651020 / 0x6515A0.
- **Level-up:** HandlePlayerToServerLevelUpMessage 0x6D3860; the message has
  the ability byte only when (level + 1) % 4 == 0 — on both sides (client
  writer 0x4B2550). ValidateLevelUp 0x6A7A70 (stats, domains, school; ret 0x10)
  checks class availability, skills (overspend only), feats (more than NWN's
  count: 66222; prerequisites: 66182; not whether a feat is on a class list),
  HP <= hit die (3109), ability bump off the 4-level cadence (63761); on success
  it applies LevelUp 0x6A9120 (base HP += +0x103, ability, skills, AddFeat
  0x6A3500, spells). Rejection = a chat line + Confirmation(0), no disconnect.
  Henchmen: LevelUpHenchman 0x70D890 → LevelUpAutomatic 0x6AA0B0 → the same
  validator. Fewer feats, no ability bump, any HP up to the die and unspent
  skill points are all accepted.
- **Feat counts:** general = 1 at total level 1 and every 3rd (+ human at 1),
  hard-coded at 0x6A8034–0x6A806E (server) and in 0x4F2CF0 (client, ret 8);
  bonus = CNWClass::GetBonusFeats(class level) 0x53B6E0 = byte +0x1ED+lvl (both
  sides read it).
- **Hit points:** the client rolls the level's HP (max at levels 1–3, then
  hd/2 + rand) unless Max Hit Points is set; creation and henchmen take the
  max. GetMaxHitPoints 0x65A560 (vtable +0x98) sums the history (max(1, die +
  Con)) + Toughness + Epic Toughness each time — nothing cached.
- **Classes:** CanLevelUpInClass 0x6A8660 (PlayerClass, expansion, alignment,
  MaxLevel, EpicLevel, cls_pres) drives both the client's class list and the
  validator. Max 3 classes is structural (client check).
- **Queries:** GetLevel 0x692520, GetClassLevel 0x699560, GetNumLevelsOfClass
  0x6B3D50, GetLevelStats 0x6995E0, GetIsClass 0x697CB0.
- **Decoded later (step 1, 2026-10-19):** see section 3.

## 3. Engine, step 1 (resource serving, feat lists, abilities, feats, point buy, alignment)

All thiscall; none of these functions is reached through a pointer or a
vtable; no jump lands inside the patch ranges (jumps_into).

- **Start-up (D):** WinMain 0x401EF0 → client init 0x40E600: CExoResMan at
  [0x92DC3C] (0x40E7DA), directories (OVERRIDE: at 0x40EBB9), key files
  (0x40EBEA), the tlk (CTlkTable::SetTlkFile 0x5CA0D0 — a raw file, not a
  resource; text through GetSimpleString 0x5C9C80); then 0x409D50 builds
  CNWRules (constructor 0x47A440, prologue 6A FF 68 37 0E 87 00): feats
  0x47AD70, the XP table inline (0x47A65A), classes 0x47C280 (cls_* through
  CNWClass 0x53BA20 / 0x53C270), races 0x47D170. Single player's server uses
  the same resman and rules ([0x92DC30] = 0).
- **ReloadAll 0x47AAC0** re-reads everything (11 callers: module load /
  unload with haks, client paths): in-memory rules edits must be re-applied
  after it (its last loader call 0x47AD4C, as CM1 does).
- **Resource lookup (D):** GetKeyEntry 0x5CC470 — encrypted haks, then
  modules / haks, then directories (newest first), then BIFs; Demand
  0x5CC010. No official .nwm / .mod holds a 2da. **Serving our files:** at the
  rules constructor's entry, add an alias (CExoAliasList::Add 0x5BE170, ecx =
  [[0x92DC34]+0xC], two CExoStrings by value, ret 0x10) and
  AddResourceDirectory("NWN5E:") 0x5CBBE0 (ecx = [0x92DC3C], ret 4): the
  folder outranks override and the BIFs, survives reloads and can hold new
  resrefs (listed once, when added — write generated files first). The mode
  is read at start-up, so the folder can differ per mode. UNSURE: trailing
  backslash on the alias path. Byte-level alternative: the getter calls in
  C2DA::Load2DArray 0x5CEFA0 (0x5CEFD9 data, 0x5CEFE5 size; existing resrefs
  only).
- **Feats (D):** CNWFeat array at rules +0xC4, stride 0x70: AllClassesCanUse
  +0x24, prerequisites +0x30..+0x4C, MinLevel / MaxLevel / MinLevelClass
  +0x55 / +0x56 / +0x57. Class feat table CNWClass +0x1E8 (pointer) / +0x1EC
  (count), 0x10-byte entries: feat +0, GrantedOnLevel +2, list bits +3
  (cls_feat List 0→1, 1→3, 2→2, 3→0); IsBonusFeat 0x53CC00, IsNormalFeat
  0x53CC50 (falls back to AllClassesCanUse), IsGrantedFeat 0x53CCC0. Client
  "can choose this feat" 0x4F3270 (ret 0x10; prologue 83 EC 24 53 55), its
  prerequisite check 0x4F2290 (also MinLevel / MaxLevel / MinLevelClass /
  GrantedOnLevel). Server: ValidateLevelUp never checks class lists nor
  general vs bonus slots; its prerequisite check 0x6B1710 skips the level
  fields; henchmen use 0x6AB840 (LevelUpAutomatic). Hiding a feat in memory:
  MinLevel 0xFF; adding one to a bonus list: the list bits.
- **Ability scores (D):** base at stats +0x3F8 / +0x3FA / +0x3FC / +0x3FE /
  +0x400 / +0x402 (Str..Cha) — without the racial adjustment, with the
  level-up bumps (LevelUp increments them from history +0x102); a cached
  modifier byte follows each. The getters (Str 0x6A0FF0, Con 0x6A10A0, Int
  0x6A1150, Wis 0x6A1200, Cha 0x6A12B0, Dex 0x6A1360; no arguments, ret) add
  race +0x18..+0x1D and effects; they feed the sheet (0x62BD30) and scripts
  (0x6F8460). Bypassing them: the prerequisite check 0x6B1710,
  ValidateCharacter, every reader of the modifier bytes (refresh through
  SetXXXBase, e.g. 0x6A1510).
- **HasFeat 0x6A3630** (ret 4, prologue 51 53 56 57 8B F9): 779 direct
  callers, all server; GetHasFeat 0x6F94F0 needs it plus remaining uses
  (0x6A3850); feat uses 0x6A3760 / 0x6A4200 / 0x6A4B10 / 0x6A4B40 / 0x6A4B80;
  AddFeat 0x6A3500, RemoveFeat 0x6A4BC0. No client HasFeat: the UI shows the
  stored lists the server sends (0x62BD30) — one hook covers rules and
  scripts, not the screens.
- **Point buy (D, patchable immediates):** 30 points at client 0x4F0074,
  0x531A6E, 0x537F05 (last byte 0x1E) and server 0x617136 (B3 1E); 18 maximum
  at client 0x52F7E5, 0x52FAB5, 0x52FD85, 0x530075, 0x530345, 0x530675 and
  server 0x61728D; cost tiers: 30 compares on 0x0E and 30 on 0x10 in the 12
  button handlers. The server side runs only with Enforce Legal Characters
  (off in single player). 2024 (27, max 15, 13→14 costs 2) = 0x0E → 0x0D.
- **Class alignment limits (D):** CNWClass +0x21D AlignRestrict, +0x21E
  AlignRstrctType, +0x220 InvertRestrict, read by GetIsAlignmentAllowed
  0x53CD80 (ret 8) from CanLevelUpInClass (0x6A8736) and 14 times by the
  alignment-shift code near 0x6996E0; also +0x21E at 0x69A253. Lifting: clear
  AlignRestrict *and* InvertRestrict (Druid inverts). The creation panel
  ignores them and hard-codes class ids (0x52B0B0: Barbarian, Bard, Monk,
  Paladin, Druid): one patch at 0x52B2CD (3B C1 0F 84 3A 02 00 00 → E9 B8 02
  00 00 90 90 90); that its buttons then start enabled is I.
