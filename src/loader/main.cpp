// nwn5e.exe — starts nwmain.exe with nwn5e.dll loaded before any game code runs.
//
// The game files are never modified: launching nwmain.exe directly is still
// vanilla. The loader refuses to start an nwmain.exe whose SHA-256 differs
// from the build the function map was made for.
//
// Game directory: first argument "--game <dir>", else nwn5e.ini [loader]
// game_dir, else the BioWare registry key the GOG installer writes.

#include <windows.h>
#include <bcrypt.h>
#include <shellapi.h>
#include <shlwapi.h>
#include <stdio.h>
#include <string>
#include <vector>

#include "inject.h"

#pragma comment(lib, "bcrypt.lib")
#pragma comment(lib, "shlwapi.lib")

namespace {

// nwmain.exe 1.69 (GOG Diamond), linked 2008-07-03.
const char kExpectedSha256[] = "4d8aa22b1bc9a48c108f3c5d4dc08f49dcddad98d863b89dc3684968d2211c49";

void Fail(const wchar_t* message) {
  MessageBoxW(nullptr, message, L"nwn5e", MB_ICONERROR | MB_OK);
  ExitProcess(1);
}

std::wstring ModuleDirectory() {
  wchar_t path[MAX_PATH];
  GetModuleFileNameW(nullptr, path, MAX_PATH);
  PathRemoveFileSpecW(path);
  return path;
}

std::wstring RegistryGameDir() {
  wchar_t value[MAX_PATH] = {};
  DWORD size = sizeof(value);
  if (RegGetValueW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\BioWare\\NWN\\Neverwinter", L"Location",
                   RRF_RT_REG_SZ | RRF_SUBKEY_WOW6432KEY, nullptr, value, &size) != ERROR_SUCCESS) {
    return L"";
  }
  std::wstring dir = value;
  while (!dir.empty() && (dir.back() == L'\\' || dir.back() == L'/')) dir.pop_back();
  return dir;
}

std::wstring GameDir(int argc, wchar_t** argv, const std::wstring& ini) {
  for (int i = 1; i + 1 < argc; ++i) {
    if (lstrcmpiW(argv[i], L"--game") == 0) return argv[i + 1];
  }
  wchar_t value[MAX_PATH] = {};
  GetPrivateProfileStringW(L"loader", L"game_dir", L"", value, MAX_PATH, ini.c_str());
  if (value[0]) return value;
  return RegistryGameDir();
}

bool Sha256Hex(const std::wstring& file, std::string* hex) {
  HANDLE handle = CreateFileW(file.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
  if (handle == INVALID_HANDLE_VALUE) return false;
  BCRYPT_ALG_HANDLE alg = nullptr;
  BCRYPT_HASH_HANDLE hash = nullptr;
  bool ok = BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) == 0 &&
            BCryptCreateHash(alg, &hash, nullptr, 0, nullptr, 0, 0) == 0;
  std::vector<unsigned char> buffer(1 << 20);
  DWORD read = 0;
  while (ok && ReadFile(handle, buffer.data(), static_cast<DWORD>(buffer.size()), &read, nullptr) && read > 0) {
    ok = BCryptHashData(hash, buffer.data(), read, 0) == 0;
  }
  unsigned char digest[32] = {};
  ok = ok && BCryptFinishHash(hash, digest, sizeof(digest), 0) == 0;
  if (hash) BCryptDestroyHash(hash);
  if (alg) BCryptCloseAlgorithmProvider(alg, 0);
  CloseHandle(handle);
  if (!ok) return false;
  char text[65];
  for (int i = 0; i < 32; ++i) sprintf_s(text + i * 2, 3, "%02x", digest[i]);
  *hex = text;
  return true;
}

}  // namespace

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
  int argc = 0;
  wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
  const std::wstring here = ModuleDirectory();
  const std::wstring ini = here + L"\\nwn5e.ini";
  const std::wstring dll = here + L"\\nwn5e.dll";
  const std::wstring game = GameDir(argc, argv, ini);
  if (game.empty()) Fail(L"Could not find the Neverwinter Nights folder. Set [loader] game_dir in nwn5e.ini.");
  const std::wstring exe = game + L"\\nwmain.exe";

  if (GetFileAttributesW(dll.c_str()) == INVALID_FILE_ATTRIBUTES) Fail(L"nwn5e.dll is missing next to nwn5e.exe.");
  std::string sha;
  if (!Sha256Hex(exe, &sha)) Fail((L"Could not read " + exe).c_str());
  if (sha != kExpectedSha256) {
    Fail(L"This nwmain.exe is not the NWN Diamond 1.69 build nwn5e was made for. Nothing was started.");
  }

  // Pass the game's own arguments through (everything except --game <dir>).
  std::wstring command = L"\"" + exe + L"\"";
  for (int i = 1; i < argc; ++i) {
    if (lstrcmpiW(argv[i], L"--game") == 0) { ++i; continue; }
    command += L" \"";
    command += argv[i];
    command += L"\"";
  }

  PROCESS_INFORMATION process = {};
  if (!StartWithDll(exe, command, game, dll, &process)) Fail(L"Could not start nwmain.exe with nwn5e.dll.");
  CloseHandle(process.hThread);
  CloseHandle(process.hProcess);
  LocalFree(argv);
  return 0;
}
