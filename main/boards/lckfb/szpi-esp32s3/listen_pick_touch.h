#pragma once

#include "sdkconfig.h"

#ifndef CONFIG_USE_EMOTE_MESSAGE_STYLE

#include <lvgl.h>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <string_view>

#include "display.h"
#include "listen_pick_session.h"

// Full-screen overlay for one listen-and-pick game. The chat screen stays put.
// `session` must outlive this object (the board game owns both).
class ListenPickTouch {
public:
    using ImageFn = const lv_image_dsc_t* (*)(size_t index);
    using SoundFn = std::string_view (*)(size_t index);

    ListenPickTouch(Display* display, ListenPickSession& session, const char* label, int entry_x,
                    ImageFn image, SoundFn sound, const char* rest_detail);
    ListenPickTouch(const ListenPickTouch&) = delete;
    ListenPickTouch& operator=(const ListenPickTouch&) = delete;

    void Initialize();
    void RequestExit();
    bool IsVisible() const { return visible_.load(); }

private:
    Display* display_;
    ListenPickSession& session_;
    const char* label_;
    int entry_x_;
    ImageFn image_;
    SoundFn sound_;
    const char* rest_detail_;
    lv_obj_t* entry_ = nullptr;
    lv_obj_t* panel_ = nullptr;
    lv_obj_t* hint_ = nullptr;
    lv_obj_t* listen_ = nullptr;
    lv_obj_t* cards_[2] = {};
    lv_obj_t* images_[2] = {};
    lv_obj_t* rest_ = nullptr;
    std::atomic<bool> visible_{false};
    std::atomic<bool> poll_pending_{false};
    std::atomic<bool> action_pending_{false};
    uint32_t generation_ = 0;
    uint32_t pressed_at_ = 0;
    size_t seed_ = 0;
    bool pressed_ = false;

    void Enter();
    void Exit();
    void Poll();
    void Action(uint32_t generation, size_t round, int side);
    void Restart();
    void StartPrompt();
    void UpdateView();
    void Bounce(size_t side);
    void ClearBounce();
};

#endif  // !CONFIG_USE_EMOTE_MESSAGE_STYLE
