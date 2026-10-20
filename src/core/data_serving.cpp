// Phase 3 CM0 (data_serving.h). Engine side (RE-NOTES "Served data"):
// CNWRules' constructor 0x47A440 loads every rules table; at its entry the
// resource manager [0x92DC3C] and the alias list [[0x92DC34]+0xC] exist, so
// an alias "NWN5E" -> <dll>\served and AddResourceDirectory("NWN5E:") make
// our folder the newest directory: GetKeyEntry 0x5CC470 searches it before
// the override folder and every BIF, now and in every ReloadAll. Only haks
// (and premium module images) rank higher; no official module ships a 2da.

#include "data_serving.h"

#include <windows.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include <map>
#include <string>
#include <vector>

#include "config.h"
#include "creature.h"
#include "data/game_resources.h"
#include "data/twoda.h"
#include "hooks.h"
#include "log.h"
#include "nwmain_1_69.h"
#include "rules_loaded.h"

namespace {

struct ExoString {  // CExoString: {char*, length}
  char* text;
  uint32_t length;
};

using RulesConstructorFn = void*(__fastcall*)(void* rules_object, void* edx);
using ExoStringFromTextFn = void*(__fastcall*)(ExoString* self, void* edx, const char* text);
using AliasAddFn = int(__fastcall*)(void* alias_list, void* edx, ExoString alias, ExoString path);
using AddResourceDirectoryFn = int(__fastcall*)(void* resman, void* edx, const ExoString* name);
using GetSimpleStringFn = int(__fastcall*)(void* table, void* edx, uint32_t strref, void* result, int gender);

RulesConstructorFn g_rules_constructor = nullptr;
const auto g_exo_string_from_text = reinterpret_cast<ExoStringFromTextFn>(nwmain::kExoStringFromText);
const auto g_alias_add = reinterpret_cast<AliasAddFn>(nwmain::kExoAliasListAdd);
const auto g_add_resource_directory = reinterpret_cast<AddResourceDirectoryFn>(nwmain::kAddResourceDirectory);

std::map<uint32_t, std::string> g_strings;
std::string g_served_directory;  // ANSI, for the engine
bool g_registered = false;
int g_probe_row = -1;  // feat.2da row of the probe (served_data_probe)

constexpr const char* kAlias = "NWN5E";
constexpr uint32_t kProbeName = kDataStringFirst;
constexpr uint32_t kProbeDescription = kDataStringFirst + 1;

std::string Narrow(const std::wstring& wide) {
  char buffer[MAX_PATH * 2] = {};
  WideCharToMultiByte(CP_ACP, 0, wide.c_str(), -1, buffer, sizeof(buffer) - 1, nullptr, nullptr);
  return buffer;
}

bool ReadText(const std::wstring& path, std::string* text) {
  FILE* file = nullptr;
  if (_wfopen_s(&file, path.c_str(), L"rb") != 0 || !file) return false;
  char buffer[4096];
  size_t read = 0;
  text->clear();
  while ((read = fread(buffer, 1, sizeof(buffer), file)) > 0) text->append(buffer, read);
  fclose(file);
  return true;
}

bool WriteText(const std::wstring& path, const std::string& text) {
  FILE* file = nullptr;
  if (_wfopen_s(&file, path.c_str(), L"wb") != 0 || !file) return false;
  const size_t written = fwrite(text.data(), 1, text.size(), file);
  fclose(file);
  return written == text.size();
}

std::vector<std::wstring> FilesIn(const std::wstring& directory, const wchar_t* pattern) {
  std::vector<std::wstring> names;
  WIN32_FIND_DATAW found;
  HANDLE handle = FindFirstFileW((directory + L"\\" + pattern).c_str(), &found);
  if (handle == INVALID_HANDLE_VALUE) return names;
  do {
    if (!(found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) names.push_back(found.cFileName);
  } while (FindNextFileW(handle, &found));
  FindClose(handle);
  return names;
}

// "<id> <text>" per line ('#' comments); "\n" in the text is a line break.
void LoadStrings(const std::wstring& path, const char* label) {
  std::string text;
  if (!ReadText(path, &text)) return;
  int count = 0;
  size_t start = 0;
  while (start < text.size()) {
    size_t end = text.find('\n', start);
    if (end == std::string::npos) end = text.size();
    std::string line = text.substr(start, end - start);
    start = end + 1;
    if (!line.empty() && line.back() == '\r') line.pop_back();
    const size_t first = line.find_first_not_of(" \t");
    if (first == std::string::npos || line[first] == '#') continue;
    char* rest = nullptr;
    const unsigned long id = strtoul(line.c_str() + first, &rest, 10);
    if (id < kDataStringFirst || id >= kDataStringEnd) {
      Log("data: %s: string id %lu outside %u..%u, skipped", label, id, kDataStringFirst, kDataStringEnd - 1);
      continue;
    }
    std::string value = rest;
    value.erase(0, value.find_first_not_of(" \t"));
    for (size_t at = value.find("\\n"); at != std::string::npos; at = value.find("\\n", at + 1)) value.replace(at, 2, "\n");
    g_strings[static_cast<uint32_t>(id)] = value;
    ++count;
  }
  Log("data: %d strings from %s", count, label);
}

// The probe (served_data_probe): a copy of feat 0 named by our strings that no
// class can take, so the log can show the served table and strings arrived.
void AddProbe(data::TwoDA* feat) {
  if (feat->rows.empty()) return;
  std::vector<std::string> row = feat->rows[0];
  for (size_t c = 0; c < feat->columns.size(); ++c) {
    const std::string& column = feat->columns[c];
    if (_stricmp(column.c_str(), "LABEL") == 0) row[c] = "NWN5E_PROBE";
    if (_stricmp(column.c_str(), "FEAT") == 0) row[c] = std::to_string(kProbeName);
    if (_stricmp(column.c_str(), "DESCRIPTION") == 0) row[c] = std::to_string(kProbeDescription);
    if (_stricmp(column.c_str(), "ALLCLASSESCANUSE") == 0) row[c] = "0";
  }
  g_probe_row = static_cast<int>(feat->rows.size());
  feat->rows.push_back(row);
  g_strings[kProbeName] = "nwn5e probe feat";
  g_strings[kProbeDescription] = "Served by nwn5e (served_data_probe).";
}

// Builds <dll>\served\<table>.2da for every table with rows. Returns the count.
int BuildServedTables(const std::wstring& dll, const std::wstring& served) {
  std::map<std::string, std::vector<std::pair<std::wstring, std::string>>> rows;  // table -> (file, label)
  const std::pair<std::wstring, const char*> sources[] = {{dll + L"\\data\\rows", "data"},
                                                           {dll + L"\\data\\private\\rows", "private"}};
  for (const auto& source : sources) {
    for (const std::wstring& name : FilesIn(source.first, L"*.txt")) {
      std::string table = Narrow(name.substr(0, name.size() - 4));
      for (char& c : table) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
      rows[table].push_back({source.first + L"\\" + name, std::string(source.second) + "\\rows\\" + Narrow(name)});
    }
  }
  if (g_config.debug_served_data_probe) rows["feat"];  // the probe needs feat.2da even without rows
  CreateDirectoryW(served.c_str(), nullptr);
  for (const std::wstring& old : FilesIn(served, L"*.2da")) DeleteFileW((served + L"\\" + old).c_str());
  if (rows.empty()) return 0;

  wchar_t host[MAX_PATH];
  GetModuleFileNameW(nullptr, host, MAX_PATH);
  std::wstring game = host;
  game = game.substr(0, game.find_last_of(L'\\'));
  data::GameResources resources(game);
  int built = 0;
  for (const auto& entry : rows) {
    std::string bytes, source, error;
    if (!resources.Read(entry.first, data::kResourceType2DA, &bytes, &source)) {
      Log("data: SKIP %s.2da: not in the game's files", entry.first.c_str());
      continue;
    }
    data::TwoDA table;
    if (!data::ParseTwoDA(bytes, &table, &error)) {
      Log("data: SKIP %s.2da from %s: %s", entry.first.c_str(), source.c_str(), error.c_str());
      continue;
    }
    const size_t original = table.rows.size();
    std::string applied;
    bool ok = true;
    for (const auto& file : entry.second) {
      std::string text;
      int changed = 0;
      if (!ReadText(file.first, &text) || !data::ApplyRows(&table, text, &error, &changed)) {
        Log("data: SKIP %s.2da: %s: %s", entry.first.c_str(), file.second.c_str(), error.c_str());
        ok = false;
        break;
      }
      applied += " + " + std::to_string(changed) + " rows (" + file.second + ")";
    }
    if (!ok) continue;
    if (entry.first == "feat" && g_config.debug_served_data_probe) {
      AddProbe(&table);
      applied += " + the probe (row " + std::to_string(g_probe_row) + ")";
    }
    if (!WriteText(served + L"\\" + std::wstring(entry.first.begin(), entry.first.end()) + L".2da",
                   data::WriteTwoDA(table))) {
      Log("data: SKIP %s.2da: can't write to served", entry.first.c_str());
      continue;
    }
    ++built;
    Log("data: served %s.2da: %u rows from %s%s -> %u rows", entry.first.c_str(), static_cast<unsigned>(original),
        source.c_str(), applied.c_str(), static_cast<unsigned>(table.rows.size()));
  }
  return built;
}

ExoString EngineString(const char* text) {
  ExoString s = {nullptr, 0};
  g_exo_string_from_text(&s, nullptr, text);
  return s;
}

// At the rules constructor's entry (before any table loads): our folder joins
// the resource manager.
void* __fastcall RulesConstructor(void* rules_object, void* edx) {
  if (!g_registered) {
    g_registered = true;
    auto* base = *reinterpret_cast<uint8_t**>(nwmain::kExoBase);
    void* aliases = base ? *reinterpret_cast<void**>(base + nwmain::kExoBaseAliasList) : nullptr;
    void* resman = *reinterpret_cast<void**>(nwmain::kExoResMan);
    if (aliases && resman) {
      g_alias_add(aliases, nullptr, EngineString(kAlias), EngineString(g_served_directory.c_str()));
      ExoString name = EngineString((std::string(kAlias) + ":").c_str());
      const int added = g_add_resource_directory(resman, nullptr, &name);
      Log("data: %s: = %s registered with the resource manager (%d)", kAlias, g_served_directory.c_str(), added);
    } else {
      Log("data: SKIP registering the served folder: resource manager %p, aliases %p", resman, aliases);
    }
  }
  return g_rules_constructor(rules_object, edx);
}

// served_data_probe: after loading, read the probe back from the engine's feat
// table and fetch its name through the string table (our label hook).
void CheckProbe(void* rules_object, bool reload) {
  if (reload || g_probe_row < 0) return;
  const int count = Field<uint16_t>(rules_object, nwmain::kRulesFeatCount);
  auto* feats = Field<uint8_t*>(rules_object, nwmain::kRulesFeats);
  if (!feats || g_probe_row >= count) {
    Log("data: probe FAILED - the engine loaded %d feats, the probe is row %d", count, g_probe_row);
    return;
  }
  const uint32_t name = Field<uint32_t>(feats + g_probe_row * nwmain::kFeatStride, nwmain::kFeatName);
  char result[0x40] = {};
  void* tlk = *reinterpret_cast<void**>(nwmain::kTlkTable);
  const auto get_simple_string = reinterpret_cast<GetSimpleStringFn>(nwmain::kTlkGetSimpleString);
  const int found = tlk ? get_simple_string(tlk, nullptr, name, result, 0) : 0;
  const char* text = *reinterpret_cast<char**>(result);
  Log("data: probe %s - the engine loaded %d feats; row %d's name is string %u = \"%s\"",
      name == kProbeName && text && strcmp(text, g_strings[kProbeName].c_str()) == 0 ? "OK" : "FAILED", count,
      g_probe_row, name, found && text ? text : "(none)");
}

}  // namespace

void InstallDataServing(const wchar_t* dll_directory) {
  const std::wstring dll = dll_directory;
  LoadStrings(dll + L"\\data\\strings.txt", "data\\strings.txt");
  LoadStrings(dll + L"\\data\\private\\strings.txt", "private\\strings.txt");
  if (!g_config.served_data) {
    Log("data: served_data=0 - nwn5e's rows are not served");
    return;
  }
  const std::wstring served = dll + L"\\served";
  if (BuildServedTables(dll, served) == 0) {
    Log("data: no tables to serve");
    return;
  }
  g_served_directory = Narrow(served);
  InstallHook({"CNWRules::CNWRules (serve nwn5e's tables)", nwmain::kRulesConstructor,
               nwmain::kRulesConstructorPrologue, sizeof(nwmain::kRulesConstructorPrologue),
               reinterpret_cast<void*>(&RulesConstructor), reinterpret_cast<void**>(&g_rules_constructor)});
  if (g_config.debug_served_data_probe) OnRulesLoaded(&CheckProbe);
}

const char* DataString(uint32_t strref) {
  if (strref < kDataStringFirst || strref >= kDataStringEnd) return nullptr;
  const auto found = g_strings.find(strref);
  return found == g_strings.end() ? nullptr : found->second.c_str();
}

bool HaveDataStrings() { return !g_strings.empty(); }
