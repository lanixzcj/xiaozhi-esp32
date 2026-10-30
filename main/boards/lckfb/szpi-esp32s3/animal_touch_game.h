#pragma once

#include "sdkconfig.h"

#ifndef CONFIG_USE_EMOTE_MESSAGE_STYLE

#include "animal_game_session.h"
#include "listen_pick_touch.h"

// Owned by the board for the firmware lifetime. The chat screen is never deleted
// or replaced; this UI is a separately owned top-layer overlay.
class AnimalTouchGame {
public:
    explicit AnimalTouchGame(Display* display);
    void Initialize() { ui_.Initialize(); }
    void RequestExit() { ui_.RequestExit(); }
    bool IsVisible() const { return ui_.IsVisible(); }

private:
    AnimalGameSession session_;
    ListenPickTouch ui_;
};

#endif  // !CONFIG_USE_EMOTE_MESSAGE_STYLE
