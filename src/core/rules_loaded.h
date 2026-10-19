#pragma once

// The rules object (XP table, classes, feats, races) has finished loading —
// at start-up, or again in CNWRules::ReloadAll (module load / unload with
// haks, RE-NOTES "XP, level cap and multiclassing"), which re-reads every
// table. In-memory rules edits are made here, so a reload doesn't undo them.
// Handlers run in registration order, after the engine's own loaders;
// `reload` is false the first time.
using RulesLoadedHandler = void (*)(void* rules_object, bool reload);

// Registers `handler`. The first call installs the two call redirects; false
// when they could not be installed (a different nwmain.exe build).
bool OnRulesLoaded(RulesLoadedHandler handler);
