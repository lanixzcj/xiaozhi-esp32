#pragma once
#include "sdkconfig.h"
#ifndef CONFIG_USE_EMOTE_MESSAGE_STYLE
#include <lvgl.h>
#include <atomic>
#include "display.h"
#include "transport_game_session.h"

class TransportTouchGame {
public:
    explicit TransportTouchGame(Display* display) : display_(display) {}
    void Initialize();
    void RequestExit();
    bool IsVisible() const { return visible_.load(); }

private:
    Display* display_;
    lv_obj_t *entry_ = nullptr, *panel_ = nullptr, *hint_ = nullptr, *listen_ = nullptr;
    lv_obj_t *cards_[2] = {}, *images_[2] = {}, *rest_ = nullptr;
    TransportGameSession session_;
    std::atomic<bool> visible_{false}, poll_pending_{false}, action_pending_{false};
    uint32_t generation_ = 0, pressed_at_ = 0;
    size_t seed_ = 0;
    bool pressed_ = false;
    void Enter();
    void Exit();
    void Poll();
    void Action(uint32_t generation, size_t round, int side);
    void Restart();
    void UpdateView();
};
#endif
