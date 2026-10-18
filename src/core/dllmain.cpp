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
void InstallRetargetRules();
void InstallSheetRules();

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
  Log("nwn5e 0.7.0 loaded");
  if (!HostIsMappedBuild()) {
    Log("host is not the mapped nwmain.exe 1.69 build; no hooks installed");
    return;
  }
  ConfigLoad(directory);
  if (!HooksBegin()) return;
  InstallCritRules();
  InstallCritConfirmRules();
  InstallAttackRules();
  InstallExtraAttackRules();
  InstallAttackTrace();
  InstallAdvantageRules();
  InstallRetargetRules();
  InstallSheetRules();
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
