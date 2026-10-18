// Chat commands (PLAN.md C8). A player character typing `!saves` in any chat
// channel gets a server message with all six 2024 saves (DescribeSaves,
// rules_saves.cpp). The line itself still goes through as normal chat.

#include <ctype.h>
#include <stdint.h>

#include <string>

#include "config.h"
#include "creature.h"
#include "engine_queries.h"
#include "hooks.h"
#include "log.h"
#include "nwmain_1_69.h"

std::string DescribeSaves(void* stats);  // rules_saves.cpp

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
using PlayerIdFn = uint32_t(__fastcall*)(void* server_internal, void* edx, uint32_t object);

SendChatFn g_send_chat = nullptr;
const auto g_server_tell = reinterpret_cast<ServerTellFn>(nwmain::kSendServerToPlayerChatServerTell);
const auto g_exo_string_from_text = reinterpret_cast<ExoStringFromTextFn>(nwmain::kExoStringFromText);
const auto g_player_id = reinterpret_cast<PlayerIdFn>(nwmain::kGetPlayerIdByGameObjectId);

constexpr uint32_t kNoPlayer = 0xFFFFFFFE;

// "!saves" (any case, leading spaces ignored), read before the engine frees it.
bool IsSavesCommand(const ExoString& text) {
  if (!text.text) return false;
  const char* c = text.text;
  for (int i = 0; i < 16 && *c == ' '; ++i) ++c;
  static const char kCommand[] = "!saves";
  for (int i = 0; kCommand[i]; ++i) {
    if (tolower(static_cast<unsigned char>(c[i])) != kCommand[i]) return false;
  }
  return c[6] == '\0' || c[6] == ' ';
}

void Reply(void* message, uint32_t player, const std::string& line) {
  ExoString text = {nullptr, 0};
  g_exo_string_from_text(&text, nullptr, line.c_str());  // engine-allocated: ServerTell destroys it
  g_server_tell(message, nullptr, player, text);
}

uint32_t __fastcall SendServerToPlayerChatMessage(void* message, void* edx, uint32_t channel, uint32_t sender,
                                                  ExoString text, uint32_t a5, void* a6) {
  const bool command = IsSavesCommand(text);
  const uint32_t result = g_send_chat(message, edx, channel, sender, text, a5, a6);
  if (!command) return result;
  void* creature = engine::GetCreature(sender);
  void* stats = creature ? Field<void*>(creature, nwmain::kCreatureStats) : nullptr;
  void* server_internal = engine::ServerInternal();
  if (!stats || !server_internal || Field<uint32_t>(stats, nwmain::kStatsIsPlayer) == 0) return result;
  const uint32_t player = g_player_id(server_internal, nullptr, sender);
  if (player == kNoPlayer) return result;
  const std::string line = DescribeSaves(stats);
  Reply(message, player, line);
  if (g_config.log_attacks) Log("chat: !saves for 0x%08X: %s", sender, line.c_str());
  return result;
}

}  // namespace

void InstallChatCommands() {
  InstallHook({"CNWSMessage::SendServerToPlayerChatMessage (chat commands)", nwmain::kSendServerToPlayerChatMessage,
               nwmain::kSendServerToPlayerChatMessagePrologue, sizeof(nwmain::kSendServerToPlayerChatMessagePrologue),
               reinterpret_cast<void*>(&SendServerToPlayerChatMessage), reinterpret_cast<void**>(&g_send_chat)});
}
