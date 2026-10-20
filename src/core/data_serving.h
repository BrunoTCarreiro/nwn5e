#pragma once

#include <stdint.h>

// Phase 3 CM0 (PLAN.md, RE-NOTES "Served data"): nwn5e's own 2da rows and
// strings. At start-up each table with rows in <dll>\data\rows (and the
// private layer's <dll>\data\private\rows) is rebuilt from the game's own
// copy + those rows into <dll>\served, and that folder is registered with
// the engine's resource manager (from the rules constructor's entry, before
// the tables load), where it outranks the override folder and the BIFs.
// The install is never touched.

// Strings for the private range: 2da cells name them (FEAT, DESCRIPTION...),
// the GetSimpleString hook serves them.
constexpr uint32_t kDataStringFirst = 16000000;
constexpr uint32_t kDataStringEnd = 16500000;

void InstallDataServing(const wchar_t* dll_directory);

// The text for a private string id, or nullptr.
const char* DataString(uint32_t strref);
bool HaveDataStrings();
