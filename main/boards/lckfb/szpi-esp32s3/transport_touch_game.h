#pragma once

#include "sdkconfig.h"

#ifndef CONFIG_USE_EMOTE_MESSAGE_STYLE

#include "listen_pick_touch.h"
#include "transport_game_session.h"

class TransportTouchGame {
public:
    explicit TransportTouchGame(Display* display);
    void Initialize() { ui_.Initialize(); }
    void RequestExit() { ui_.RequestExit(); }
    bool IsVisible() const { return ui_.IsVisible(); }

private:
    TransportGameSession session_;
    ListenPickTouch ui_;
};

#endif
