#pragma once
#include "sdkconfig.h"
#ifndef CONFIG_USE_EMOTE_MESSAGE_STYLE
#include <lvgl.h>
#include <atomic>
#include <ctime>
#include "display.h"

// Read-only overlay: the system clock is already set by the OTA time response,
// including its local-time offset. Do not add another timezone or change TZ.
class ClockTool {
public:
    using Now = time_t (*)();
    explicit ClockTool(Display* display, Now now = nullptr) : display_(display), now_(now) {}
    void Initialize();
    void RequestExit();
    bool IsVisible() const { return visible_.load(); }

private:
    Display* display_;
    Now now_;
    lv_obj_t* entry_ = nullptr;
    lv_obj_t* panel_ = nullptr;
    lv_obj_t* time_ = nullptr;
    lv_obj_t* date_ = nullptr;
    lv_obj_t* waiting_ = nullptr;
    std::atomic<bool> visible_{false};
    time_t displayed_ = -1;
    void Enter();
    void Exit();
    void Tick();
};
#endif
