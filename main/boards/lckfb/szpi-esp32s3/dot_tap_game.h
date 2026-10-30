#pragma once

#include "sdkconfig.h"

#ifndef CONFIG_USE_EMOTE_MESSAGE_STYLE

#include <atomic>
#include <cstdint>

#include <lvgl.h>

#include "accel_source.h"
#include "display.h"
#include "dot_tap_session.h"
#include "dot_tilt.h"
#include "shake_detector.h"

// Full-screen flat-dot game for a toddler and a parent. The chat screen stays
// underneath. Shake comes from `accel` when that chip is present; otherwise a
// long press on the mixed-color page stands in for it.
class DotTapGame {
public:
    // Held this long on the mixed page when the IMU is absent.
    static constexpr uint32_t kFallbackHoldMs = 700;

    DotTapGame(Display* display, AccelSource* accel);
    DotTapGame(const DotTapGame&) = delete;
    DotTapGame& operator=(const DotTapGame&) = delete;

    void Initialize();
    void RequestExit();
    bool IsVisible() const { return visible_.load(); }
    DotTapSession::Page page() const { return session_.page(); }

private:
    enum class Motion { Place, Pop, Glide, Arrive };

    Display* display_;
    AccelSource* accel_;
    DotTapSession session_;
    ShakeDetector shake_;
    lv_obj_t* entry_ = nullptr;
    lv_obj_t* panel_ = nullptr;
    lv_obj_t* dots_[DotTapSession::kDots] = {};
    lv_obj_t* rest_ = nullptr;
    lv_obj_t* restart_ = nullptr;
    lv_obj_t* back_ = nullptr;
    std::atomic<bool> visible_{false};
    std::atomic<bool> poll_pending_{false};
    std::atomic<bool> action_pending_{false};
    std::atomic<bool> pressed_{false};
    std::atomic<bool> fallback_latched_{false};
    std::atomic<uint32_t> pressed_at_{0};
    uint32_t generation_ = 0;
    int read_failures_ = 0;
    DotRoll roll_;
    uint32_t pose_lock_until_ = 0;
    uint32_t last_tilt_ms_ = 0;
    bool tilt_synced_ = false;
    int scale_[DotTapSession::kDots] = {
        LV_SCALE_NONE, LV_SCALE_NONE, LV_SCALE_NONE, LV_SCALE_NONE, LV_SCALE_NONE};

    void Enter();
    void Exit();
    void Poll();
    void Action(uint32_t generation, int index);
    void Restart();
    void OnShake(uint32_t now);
    void ApplyTilt(uint32_t now, int pull_x_mg, int pull_y_mg);
    void ShowPage(DotTapSession::Page page);
    void ShowDot(int index, int x, int y, uint32_t color, Motion motion, int scale, uint32_t ms);
    void HideDot(int index);
    void Bounce(int index);
    void PlayCue(int cue);
};

#endif  // !CONFIG_USE_EMOTE_MESSAGE_STYLE
