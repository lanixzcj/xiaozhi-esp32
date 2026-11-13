#pragma once
#include "sdkconfig.h"
#ifndef CONFIG_USE_EMOTE_MESSAGE_STYLE
#include <lvgl.h>
#include <atomic>
#include "display.h"

// Two large photo halves. No timers, scores, automatic advance or tilt input.
// Reuses the licensed animal photos without allocating transformed image layers.
class PhotoPuzzleGame {
public:
    explicit PhotoPuzzleGame(Display* display) : display_(display) {}
    void Initialize();
    void RequestExit();
    bool IsVisible() const { return visible_.load(); }

private:
    Display* display_;
    lv_obj_t* entry_ = nullptr;
    lv_obj_t* panel_ = nullptr;
    lv_obj_t* ghost_ = nullptr;
    lv_obj_t* title_ = nullptr;
    lv_obj_t* next_ = nullptr;
    lv_obj_t* pieces_[2] = {};
    lv_obj_t* images_[2] = {};
    std::atomic<bool> visible_{false};
    bool placed_[2] = {};
    int drag_ = -1;
    lv_point_t offset_{};
    unsigned round_ = 0;
    void Enter();
    void Exit();
    void Round();
    void UpdateEntryPosition();
    void Move(lv_indev_t* input);
    void Release();
    static void Event(lv_event_t* event);
};
#endif
