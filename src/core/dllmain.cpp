// nwn5e.dll — loaded into nwmain.exe by nwn5e.exe before the game's entry point.

#include <windows.h>
#include <shlwapi.h>

#include "config.h"
#include "hooks.h"
#include "log.h"

#pragma comment(lib, "shlwapi.lib")

void InstallCritRules();
void InstallCritConfirmRules();
void InstallAttackRules();
void InstallExtraAttackRules();
void InstallAttackTrace();
void InstallAdvantageRules();
void InstallSaveRules();
void InstallSaveRollRules();
void InstallChatCommands();
void InstallOpportunityRules();
void InstallTwoWeaponRules();
void InstallMasteryRules();
void InstallLabelRules();
void InstallConditionRules();
void InstallPotionRules();
void InstallSkillRules();
void InstallPerceptionRules();
void InstallSkillDcRules();
void InstallSpellAttackRules();
void InstallCastingRules();
void InstallShoveRules();
void InstallInitiativeRules();
void InstallNwnOnlySkillRules();
void InstallProgressionRules();
void InstallAlignmentRules();
void InstallHitPointRules();
void InstallDataServing(const wchar_t* dll_directory);
void InstallFeatCadenceRules();
void InstallAbilityRules();
void InstallCreatureUpdate();
void InstallRetargetRules();
void InstallSheetRules();
#ifdef NWN5E_PRIVATE
void InstallPrivateRules();  // private/src (docs/PRIVATE-LAYER.md)
#endif

namespace {

// PE header identity of the mapped nwmain.exe; the loader checks the full SHA-256.
constexpr DWORD kTimeDateStamp = 0x486CFADC;
constexpr DWORD kSizeOfImage = 0x05E6D000;

bool HostIsMappedBuild() {
  const auto base = reinterpret_cast<const BYTE*>(GetModuleHandleW(nullptr));
  const auto dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
  const auto nt = reinterpret_cast<const IMAGE_NT_HEADERS32*>(base + dos->e_lfanew);
  Log("host: TimeDateStamp=0x%08X SizeOfImage=0x%08X", nt->FileHeader.TimeDateStamp, nt->OptionalHeader.SizeOfImage);
  return nt->FileHeader.TimeDateStamp == kTimeDateStamp && nt->OptionalHeader.SizeOfImage == kSizeOfImage;
}

void Start(HMODULE self) {
  wchar_t directory[MAX_PATH];
  GetModuleFileNameW(self, directory, MAX_PATH);
  PathRemoveFileSpecW(directory);
  LogOpen(directory);
  Log("nwn5e 0.9.53 loaded");
#ifdef NWN5E_PRIVATE
  Log("private layer: compiled in (personal use only; never share this DLL)");
#endif
  if (!HostIsMappedBuild()) {
    Log("host is not the mapped nwmain.exe 1.69 build; no hooks installed");
    return;
  }
  ConfigLoad(directory);
  if (!HooksBegin()) return;
  InstallDataServing(directory);  // first: the label hook serves its strings
  InstallCritRules();
  InstallCritConfirmRules();
  InstallAttackRules();
  InstallExtraAttackRules();
  InstallAttackTrace();
  InstallAdvantageRules();
  InstallSaveRules();
  InstallSaveRollRules();
  InstallChatCommands();
  InstallOpportunityRules();
  InstallTwoWeaponRules();
  InstallMasteryRules();
  InstallLabelRules();
  InstallConditionRules();
  InstallPotionRules();
  InstallSkillRules();
  InstallPerceptionRules();
  InstallSkillDcRules();
  InstallSpellAttackRules();
  InstallCastingRules();
  InstallShoveRules();
  InstallInitiativeRules();
  InstallNwnOnlySkillRules();
  InstallProgressionRules();
  InstallAlignmentRules();
  InstallHitPointRules();
  InstallFeatCadenceRules();
  InstallAbilityRules();
  InstallCreatureUpdate();
  InstallRetargetRules();
  InstallSheetRules();
#ifdef NWN5E_PRIVATE
  InstallPrivateRules();
#endif
  Log(HooksCommit() ? "hooks active" : "hooks NOT active");
}

}  // namespace

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
  if (reason == DLL_PROCESS_ATTACH) {
    DisableThreadLibraryCalls(module);
    Start(module);
  }
  return TRUE;
}
