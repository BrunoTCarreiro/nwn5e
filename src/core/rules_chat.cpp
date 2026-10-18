// Chat commands (PLAN.md C8, D3). A player character typing `!saves` in any
// chat channel gets a server message with all six 2024 saves (DescribeSaves,
// rules_saves.cpp); `!mastery` shows or changes their weapon masteries
// (MasteryCommand, rules_mastery.cpp). The line itself still goes through as
// normal chat. TellPlayer sends a server message on its own (mastery
// announcements).

#include <ctype.h>
#include <stdint.h>
#include <string.h>

#include <string>
#include <vector>

#include "config.h"
#include "creature.h"
#include "engine_queries.h"
#include "hooks.h"
#include "log.h"
#include "nwmain_1_69.h"

std::string DescribeSaves(void* stats);                                            // rules_saves.cpp
std::vector<std::string> MasteryCommand(void* creature, const std::string& args);  // rules_mastery.cpp

namespace {

// CExoString, passed by value (the callee destroys it).
struct ExoString {
  char* text;
  uint32_t length;
};

using SendChatFn = uint32_t(__fastcall*)(void* message, void* edx, uint32_t channel, uint32_t sender,
                                         ExoString text, uint32_t a5, void* a6);
using ServerTellFn = int(__fastcall*)(void* message, void* edx, uint32_t player, ExoString text);
using ExoStringFromTextFn = void*(__fastcall*)(ExoString* self, void* edx, const char* text);
using PlayerIdFn = uint32_t(__fastcall*)(void* app, void* edx, uint32_t object);
using GetNwsMessageFn = void*(__fastcall*)(void* app, void* edx);

SendChatFn g_send_chat = nullptr;
const auto g_server_tell = reinterpret_cast<ServerTellFn>(nwmain::kSendServerToPlayerChatServerTell);
const auto g_exo_string_from_text = reinterpret_cast<ExoStringFromTextFn>(nwmain::kExoStringFromText);
const auto g_player_id = reinterpret_cast<PlayerIdFn>(nwmain::kGetPlayerIdByGameObjectId);
const auto g_get_nws_message = reinterpret_cast<GetNwsMessageFn>(nwmain::kGetNwsMessage);

constexpr uint32_t kNoPlayer = 0xFFFFFFFE;

enum class Command { kNone, kSaves, kMastery };

// "!saves" or "!mastery <args>" (command in any case, leading spaces
// ignored), read before the engine frees the text.
Command ParseCommand(const ExoString& text, std::string* args) {
  if (!text.text || text.length == 0) return Command::kNone;
  std::string line(text.text, strnlen(text.text, text.length));
  const size_t start = line.find_first_not_of(' ');
  if (start == std::string::npos || line[start] != '!') return Command::kNone;
  line = line.substr(start);
  const size_t word_end = line.find(' ');
  std::string word = line.substr(0, word_end);
  for (char& c : word) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
  if (word_end != std::string::npos) {
    const size_t args_start = line.find_first_not_of(' ', word_end);
    if (args_start != std::string::npos) *args = line.substr(args_start);
  }
  if (word == "!saves") return Command::kSaves;
  if (word == "!mastery") return Command::kMastery;
  return Command::kNone;
}

void Reply(void* message, uint32_t player, const std::string& line) {
  ExoString text = {nullptr, 0};
  g_exo_string_from_text(&text, nullptr, line.c_str());  // engine-allocated: ServerTell destroys it
  g_server_tell(message, nullptr, player, text);
}

uint32_t __fastcall SendServerToPlayerChatMessage(void* message, void* edx, uint32_t channel, uint32_t sender,
                                                  ExoString text, uint32_t a5, void* a6) {
  std::string args;
  const Command command = ParseCommand(text, &args);
  const uint32_t result = g_send_chat(message, edx, channel, sender, text, a5, a6);
  if (command == Command::kNone) return result;
  void* creature = engine::GetCreature(sender);
  void* stats = creature ? Field<void*>(creature, nwmain::kCreatureStats) : nullptr;
  void* app = engine::ServerExoApp();
  if (!stats || !app || Field<uint32_t>(stats, nwmain::kStatsIsPlayer) == 0) return result;
  const uint32_t player = g_player_id(app, nullptr, sender);
  if (player == kNoPlayer) {
    Log("chat: no player id for 0x%08X; command not answered", sender);
    return result;
  }
  const std::vector<std::string> lines =
      command == Command::kSaves ? std::vector<std::string>{DescribeSaves(stats)} : MasteryCommand(creature, args);
  for (const std::string& line : lines) Reply(message, player, line);
  if (g_config.log_attacks && !lines.empty()) {
    Log("chat: %s for 0x%08X: %s%s", command == Command::kSaves ? "!saves" : "!mastery", sender, lines[0].c_str(),
        lines.size() > 1 ? " ..." : "");
  }
  return result;
}

}  // namespace

// A server message to the player controlling `object_id` (a player character).
void TellPlayer(uint32_t object_id, const std::string& line) {
  void* app = engine::ServerExoApp();
  if (!app) return;
  const uint32_t player = g_player_id(app, nullptr, object_id);
  void* message = player == kNoPlayer ? nullptr : g_get_nws_message(app, nullptr);
  if (message) Reply(message, player, line);
}

void InstallChatCommands() {
  InstallHook({"CNWSMessage::SendServerToPlayerChatMessage (chat commands)", nwmain::kSendServerToPlayerChatMessage,
               nwmain::kSendServerToPlayerChatMessagePrologue, sizeof(nwmain::kSendServerToPlayerChatMessagePrologue),
               reinterpret_cast<void*>(&SendServerToPlayerChatMessage), reinterpret_cast<void**>(&g_send_chat)});
}
