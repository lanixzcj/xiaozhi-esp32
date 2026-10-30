#include "transport_touch_game.h"

#ifndef CONFIG_USE_EMOTE_MESSAGE_STYLE

#include "transport_game_assets.h"

static_assert(transport_game_assets::kCount == TransportGameSession::kCount);

TransportTouchGame::TransportTouchGame(Display* display)
    : session_(),
      ui_(display, session_, "交通", 240, transport_game_assets::GetImage,
          transport_game_assets::GetSound, nullptr) {}

#endif
