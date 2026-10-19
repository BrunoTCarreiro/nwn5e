// Decision SK3 (PLAN.md), faithful: NWN's fixed skill DCs on the 2024 scale.
// Skill totals are 2024's (the GetSkillRank hook, SK2), so every engine check
// that compares one with a 3e DC converts the DC — 10 + (DC - 10) / 2, a DC
// of 1 or less kept (rules::ConvertNwnSkillDc): GetIsSkillSuccessful (the
// scripts' checks), Open Lock (unlock, lock, the scripts' "can it be
// unlocked"), the five trap actions, Search, Heal kits, Pick Pocket, Animal
// Empathy, Tumble against attacks of opportunity, Spellcraft's identify-spell
// checks and the Examine Trap message. Each DC is converted where it is
// loaded, before the combat log reads it, so the log shows the 2024 DC.
// Lore identification compares the total with SkillVsItemCost.2da rows (a
// skill value, not a DC): the 2024 total is scaled to the 3e range there (2 x,
// as for scripts, SK3 a). A trap a character sets stores its DCs as the
// trap's base + the setter's skill: that skill is scaled the same way, so the
// later conversion lands right. Sites, bytes and registers: nwmain_1_69.h
// kSkillDcSites (decoded 2026-10-19, RE-NOTES "Skill DCs").
//
// Each site jumps to a small trampoline built here: the replaced instructions,
// copied as they are (jumped to, not called, so their stack offsets hold),
// with the conversion of one register or stack slot spliced in, then a jump
// back. The conversion helpers keep every other register and the flags.

#include <windows.h>

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <string>

#include "config.h"
#include "creature.h"
#include "engine_queries.h"
#include "hooks.h"
#include "log.h"
#include "log_once.h"
#include "nwmain_1_69.h"
#include "rules/skills.h"

namespace {

volatile int32_t g_site = -1;  // the site whose trampoline is converting (set by the trampoline)
LogOnce g_logged(300);

int __stdcall ConvertAtSite(int value) {
  const int index = g_site;
  if (index < 0 || index >= static_cast<int>(sizeof(nwmain::kSkillDcSites) / sizeof(nwmain::kSkillDcSites[0]))) {
    return value;
  }
  const nwmain::SkillDcSite& site = nwmain::kSkillDcSites[index];
  const int converted = site.kind == nwmain::kSkillValueRow ? rules::ScriptSkillValue2024(value)
                                                            : rules::ConvertNwnSkillDc(value);
  if (g_config.log_attacks && g_logged.First(LogOnce::Key(static_cast<uint32_t>(index), value, converted))) {
    Log("skill DC: %s: 3e %d -> 2024 %d%s", site.what, value, converted,
        site.kind == nwmain::kSkillValueRow ? " (a skill value, scaled to the 3e table)" : "");
  }
  return converted;
}

// reg = ConvertAtSite(reg); every other register and the flags kept.
__declspec(naked) void ConvEax() {
  __asm {
    pushfd
    push ecx
    push edx
    push eax
    call ConvertAtSite
    pop edx
    pop ecx
    popfd
    ret
  }
}
__declspec(naked) void ConvEcx() {
  __asm {
    pushfd
    push eax
    push edx
    push ecx
    call ConvertAtSite
    mov ecx, eax
    pop edx
    pop eax
    popfd
    ret
  }
}
__declspec(naked) void ConvEdx() {
  __asm {
    pushfd
    push eax
    push ecx
    push edx
    call ConvertAtSite
    mov edx, eax
    pop ecx
    pop eax
    popfd
    ret
  }
}
__declspec(naked) void ConvEbx() {
  __asm {
    pushfd
    push eax
    push ecx
    push edx
    push ebx
    call ConvertAtSite
    mov ebx, eax
    pop edx
    pop ecx
    pop eax
    popfd
    ret
  }
}
__declspec(naked) void ConvEbp() {
  __asm {
    pushfd
    push eax
    push ecx
    push edx
    push ebp
    call ConvertAtSite
    mov ebp, eax
    pop edx
    pop ecx
    pop eax
    popfd
    ret
  }
}
__declspec(naked) void ConvEsi() {
  __asm {
    pushfd
    push eax
    push ecx
    push edx
    push esi
    call ConvertAtSite
    mov esi, eax
    pop edx
    pop ecx
    pop eax
    popfd
    ret
  }
}
__declspec(naked) void ConvEdi() {
  __asm {
    pushfd
    push eax
    push ecx
    push edx
    push edi
    call ConvertAtSite
    mov edi, eax
    pop edx
    pop ecx
    pop eax
    popfd
    ret
  }
}

// By x86 register number (eax 0, ecx 1, edx 2, ebx 3, esp 4, ebp 5, esi 6, edi 7).
void* const kConverters[8] = {&ConvEax, &ConvEcx, &ConvEdx, &ConvEbx, nullptr, &ConvEbp, &ConvEsi, &ConvEdi};

class Emitter {
 public:
  explicit Emitter(uint8_t* at) : p_(at) {}
  uint8_t* at() const { return p_; }
  void Bytes(const uint8_t* bytes, size_t n) {
    memcpy(p_, bytes, n);
    p_ += n;
  }
  void Byte(uint8_t b) { *p_++ = b; }
  void Dword(uint32_t v) {
    memcpy(p_, &v, sizeof(v));
    p_ += sizeof(v);
  }
  void Rel32(uint8_t opcode, const void* target) {  // call (E8) / jmp (E9)
    Byte(opcode);
    Dword(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(target) - (reinterpret_cast<uintptr_t>(p_) + 4)));
  }

 private:
  uint8_t* p_;
};

// Builds the trampoline for site `index` at `at`; returns its end.
uint8_t* BuildTrampoline(int index, uint8_t* at) {
  const nwmain::SkillDcSite& site = nwmain::kSkillDcSites[index];
  Emitter e(at);
  e.Bytes(site.original, site.split);
  if (site.target != nwmain::kNoConversion) {
    e.Byte(0xC7);  // mov dword ptr [g_site], index
    e.Byte(0x05);
    e.Dword(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(&g_site)));
    e.Dword(static_cast<uint32_t>(index));
    if (site.target == nwmain::kStackSlot) {
      const uint8_t disp = static_cast<uint8_t>(site.slot + 4);  // past the pushed eax
      e.Byte(0x50);                                              // push eax
      const uint8_t load[4] = {0x8B, 0x44, 0x24, disp};          // mov eax, [esp+disp]
      e.Bytes(load, sizeof(load));
      e.Rel32(0xE8, &ConvEax);
      const uint8_t store[4] = {0x89, 0x44, 0x24, disp};  // mov [esp+disp], eax
      e.Bytes(store, sizeof(store));
      e.Byte(0x58);  // pop eax
    } else {
      e.Rel32(0xE8, kConverters[site.target]);
    }
  }
  e.Bytes(site.original + site.split, site.length - site.split);
  e.Bytes(site.extra, site.extra_length);
  e.Rel32(0xE9, reinterpret_cast<const void*>(site.address + site.length));
  return e.at();
}

}  // namespace

void InstallSkillDcRules() {
  if (g_config.skills != rules::RuleVariant::kFaithful) return;
  const int count = static_cast<int>(sizeof(nwmain::kSkillDcSites) / sizeof(nwmain::kSkillDcSites[0]));
  auto* code = static_cast<uint8_t*>(VirtualAlloc(nullptr, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
  if (!code) {
    Log("SKIP skill DCs: no memory for the trampolines");
    return;
  }
  uint8_t* next = code;
  int applied = 0;
  for (int i = 0; i < count; ++i) {
    const nwmain::SkillDcSite& site = nwmain::kSkillDcSites[i];
    uint8_t* trampoline = next;
    next = BuildTrampoline(i, trampoline);
    uint8_t jump[16];
    memset(jump, 0x90, sizeof(jump));
    jump[0] = 0xE9;
    const int32_t rel =
        static_cast<int32_t>(reinterpret_cast<uintptr_t>(trampoline) - (site.address + nwmain::kCallLength));
    memcpy(jump + 1, &rel, sizeof(rel));
    char name[112];
    sprintf_s(name, "skill DC: %s (faithful)", site.what);
    applied += PatchBytes(name, site.address, site.original, jump, site.length) ? 1 : 0;
  }
  FlushInstructionCache(GetCurrentProcess(), code, static_cast<SIZE_T>(next - code));
  for (const nwmain::SmallPatch& patch : nwmain::kSkillDcImmediates) {
    char name[112];
    sprintf_s(name, "skill DC: %s (faithful)", patch.what);
    applied += PatchBytes(name, patch.address, patch.original, patch.patched, patch.size) ? 1 : 0;
  }
  Log("skill DCs: %d of %d faithful patches applied (%d trampoline bytes)", applied,
      count + static_cast<int>(sizeof(nwmain::kSkillDcImmediates) / sizeof(nwmain::kSkillDcImmediates[0])),
      static_cast<int>(next - code));
  // Owner, 2026-10-19: no take 20, no trap synergy, no Rogue-only traps.
  int rules_applied = 0;
  int rules_count = 0;
  const auto apply = [&](const nwmain::SmallPatch* patches, size_t n) {
    for (size_t i = 0; i < n; ++i) {
      char name[112];
      sprintf_s(name, "skills: %s (faithful)", patches[i].what);
      rules_applied += PatchBytes(name, patches[i].address, patches[i].original, patches[i].patched, patches[i].size) ? 1 : 0;
      ++rules_count;
    }
  };
  apply(nwmain::kNoTake20Patches, sizeof(nwmain::kNoTake20Patches) / sizeof(nwmain::kNoTake20Patches[0]));
  apply(nwmain::kNoTrapSynergyPatches, sizeof(nwmain::kNoTrapSynergyPatches) / sizeof(nwmain::kNoTrapSynergyPatches[0]));
  apply(nwmain::kNoRogueTrapPatches, sizeof(nwmain::kNoRogueTrapPatches) / sizeof(nwmain::kNoRogueTrapPatches[0]));
  Log("skills: %d of %d patches applied (no take 20, no trap synergy, no Rogue-only traps)", rules_applied,
      rules_count);
  void InstallSkillCheckConditions();
  InstallSkillCheckConditions();
}

// SK11 (faithful): the d20 of a script's GetIsSkillSuccessful (0x70CD73) —
// disadvantage when the creature is poisoned or frightened; a deafened one
// listening or a blinded one spotting / searching rolls the lowest die.
#include "condition_effects.h"
using GetBlindForCheckFn = int(__fastcall*)(void* creature, void* edx);
using RollDiceForCheckFn = int(__fastcall*)(void* rules_object, void* edx, int count, int sides);
int g_check_lines = 0;

int __stdcall SkillCheckD20(void* creature, int skill) {
  void* rules_object = *reinterpret_cast<void**>(nwmain::kRulesObject);
  const auto roll = reinterpret_cast<RollDiceForCheckFn>(nwmain::kRollDice);
  if (!rules_object) return 10;
  const int first = roll(rules_object, nullptr, 1, 20) & 0xFFFF;
  if (!creature || g_config.conditions != rules::RuleVariant::kFaithful) return first;
  const bool blind = reinterpret_cast<GetBlindForCheckFn>(nwmain::kGetBlind)(creature, nullptr) != 0;
  const bool deaf = engine::HasEffectType(creature, nwmain::kEffectTypeDeaf);
  const char* sense = (skill == nwmain::kSkillListen && deaf) ? "deafened"
                      : ((skill == nwmain::kSkillSpot || skill == nwmain::kSkillSearch) && blind) ? "blinded"
                                                                                                  : nullptr;
  const std::string worse = CheckDisadvantage(creature);
  int kept = first;
  int second = 0;
  if (sense) {
    kept = 1;
  } else if (!worse.empty()) {
    second = roll(rules_object, nullptr, 1, 20) & 0xFFFF;
    kept = rules::KeptCheckD20(first, second, 0, 1);
  }
  if ((sense || second) && g_config.log_attacks && g_check_lines < 200) {
    ++g_check_lines;
    Log("skill check: 0x%08X skill %d: d20 %d%s%s -> %d (%s)", Field<uint32_t>(creature, nwmain::kCreatureObjectId),
        skill, first, second ? " / " : "", second ? std::to_string(second).c_str() : "", kept,
        sense ? (std::string(sense) + ": fails what needs that sense").c_str() : ("disadvantage: " + worse).c_str());
  }
  return kept;
}

// In place of RollDice(1, 20) at 0x70CD73: the creature is at [esp+0x24] and
// the skill at [esp+0x1C] on entry (the handler's [esp+0x14] / [esp+0x0C],
// + ebp pushed, + the two arguments, + our return address).
__declspec(naked) void SkillCheckD20Stub() {
  __asm {
    push dword ptr [esp + 0x1C]
    push dword ptr [esp + 0x28]
    call SkillCheckD20
    ret 8
  }
}

void InstallSkillCheckConditions() {
  if (g_config.skills != rules::RuleVariant::kFaithful || g_config.conditions != rules::RuleVariant::kFaithful) return;
  const auto* code = reinterpret_cast<const uint8_t*>(nwmain::kSkillCheckD20Call);
  int32_t rel = 0;
  memcpy(&rel, code + 1, sizeof(rel));
  if (code[0] != 0xe8 || nwmain::kSkillCheckD20Call + nwmain::kCallLength + rel != nwmain::kRollDice) {
    Log("SKIP skill checks: 0x%08X is not the mapped RollDice call", nwmain::kSkillCheckD20Call);
    return;
  }
  uint8_t expected[5];
  memcpy(expected, code, sizeof(expected));
  uint8_t call[5] = {0xe8};
  const int32_t new_rel = static_cast<int32_t>(reinterpret_cast<uintptr_t>(&SkillCheckD20Stub) -
                                               (nwmain::kSkillCheckD20Call + nwmain::kCallLength));
  memcpy(call + 1, &new_rel, sizeof(new_rel));
  PatchBytes("skill checks: GetIsSkillSuccessful d20 with conditions (faithful)", nwmain::kSkillCheckD20Call, expected,
             call, sizeof(call));
}
