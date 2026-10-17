#pragma once

#include "sdkconfig.h"

#ifndef CONFIG_USE_EMOTE_MESSAGE_STYLE

#include <lvgl.h>
#include <atomic>
#include <cstdint>

#include "animal_game_session.h"
#include "display.h"

// Owned by the board for the firmware lifetime. The chat screen is never deleted
// or replaced; this UI is a separately owned, opaque top-layer overlay.
class AnimalTouchGame {
public:
    explicit AnimalTouchGame(Display* display) : display_(display) {}
    void Initialize();   // Main task, after Display::SetupUI().
    void RequestExit();  // Any task; BOOT uses this during a game.
    bool IsVisible() const { return visible_.load(); }

private:
    Display* display_;
    lv_obj_t* entry_ = nullptr;
    lv_obj_t* panel_ = nullptr;
    lv_obj_t* card_ = nullptr;
    lv_obj_t* title_ = nullptr;
    lv_obj_t* hint_ = nullptr;
    lv_obj_t* rest_ = nullptr;
    lv_obj_t* restart_ = nullptr;
    lv_timer_t* timer_ = nullptr;
    AnimalGameSession session_;
    std::atomic<bool> visible_{false};
    std::atomic<bool> poll_pending_{false};
    std::atomic<bool> tap_pending_{false};
    uint32_t restart_pressed_at_ = 0;
    bool restart_pressed_ = false;

    void Enter();
    void Exit();
    void Tap();
    void Poll();
    void Restart();
    void UpdateView();  // Caller holds the LVGL/display lock.
    static void DrawAnimal(lv_event_t* event);
};

#endif  // !CONFIG_USE_EMOTE_MESSAGE_STYLE
