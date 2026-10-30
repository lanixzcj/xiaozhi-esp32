#include "dot_tap_game.h"

#ifndef CONFIG_USE_EMOTE_MESSAGE_STYLE

#include "application.h"
#include "dot_tap_sounds.h"

LV_FONT_DECLARE(animal_game_font);

namespace {
constexpr uint32_t kCream = 0xFFF8EA;
constexpr uint32_t kInk = 0x41382F;
constexpr uint32_t kYellow = 0xF5C400;
constexpr uint32_t kRed = 0xE23B3B;
constexpr uint32_t kBlue = 0x2E7BD6;
constexpr int kSize = 64;
constexpr int kScatterScale = 320;
constexpr uint32_t kMixMs = 240;

struct Place {
    int x;
    int y;
    uint32_t color;
};

// 320x240, 64 px dots. Page one reuses the center of the five-dot layout.
constexpr Place kFive[DotTapSession::kDots] = {
    {24, 14, kYellow}, {232, 14, kYellow}, {128, 88, kYellow},
    {24, 162, kYellow}, {232, 162, kYellow},
};
constexpr Place kMixed[DotTapSession::kDots] = {
    {36, 22, kYellow}, {214, 18, kYellow}, {118, 158, kRed},
    {22, 118, kRed}, {214, 102, kBlue},
};
// Same colors as kMixed, flung toward the edges (some sit slightly off-screen).
constexpr Place kScattered[DotTapSession::kDots] = {
    {-18, -22, kYellow}, {246, 186, kYellow}, {124, 92, kRed},
    {236, -16, kRed}, {-20, 170, kBlue},
};
constexpr Place kCross[DotTapSession::kDots] = {
    {128, 6, kBlue}, {16, 88, kRed}, {128, 88, kYellow},
    {240, 88, kRed}, {128, 170, kBlue},
};

void SetX(void* object, int32_t value) {
    lv_obj_set_x(static_cast<lv_obj_t*>(object), value);
}
void SetY(void* object, int32_t value) {
    lv_obj_set_y(static_cast<lv_obj_t*>(object), value);
}
void SetScale(void* object, int32_t value) {
    lv_obj_set_style_transform_scale(static_cast<lv_obj_t*>(object), value, 0);
}

void Animate(lv_obj_t* dot, lv_anim_exec_xcb_t callback, int32_t from, int32_t to, uint32_t ms,
             uint32_t delay, lv_anim_path_cb_t path) {
    if (ms == 0 || from == to) {
        callback(dot, to);
        return;
    }
    lv_anim_t anim;
    lv_anim_init(&anim);
    lv_anim_set_var(&anim, dot);
    lv_anim_set_exec_cb(&anim, callback);
    lv_anim_set_values(&anim, from, to);
    lv_anim_set_duration(&anim, ms);
    lv_anim_set_delay(&anim, delay);
    lv_anim_set_path_cb(&anim, path);
    lv_anim_start(&anim);
}

lv_obj_t* Paper(lv_obj_t* parent, int width, int height) {
    auto object = lv_obj_create(parent);
    lv_obj_remove_style_all(object);
    lv_obj_set_pos(object, 0, 0);
    lv_obj_set_size(object, width, height);
    lv_obj_set_style_bg_color(object, lv_color_hex(kCream), 0);
    lv_obj_set_style_bg_opa(object, LV_OPA_COVER, 0);
    lv_obj_remove_flag(object, LV_OBJ_FLAG_SCROLLABLE);
    return object;
}

lv_obj_t* InkLabel(lv_obj_t* parent, const char* text) {
    auto label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_color(label, lv_color_hex(kInk), 0);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_remove_flag(label, LV_OBJ_FLAG_CLICKABLE);
    return label;
}

lv_obj_t* Chip(lv_obj_t* parent, const char* text, int x, int y, int width, int height) {
    auto button = lv_obj_create(parent);
    lv_obj_remove_style_all(button);
    lv_obj_set_pos(button, x, y);
    lv_obj_set_size(button, width, height);
    lv_obj_set_style_bg_color(button, lv_color_hex(0xE2DFC4), 0);
    lv_obj_set_style_bg_opa(button, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(button, 12, 0);
    lv_obj_add_flag(button, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(button, LV_OBJ_FLAG_SCROLLABLE);
    auto label = InkLabel(button, text);
    lv_obj_set_width(label, width);
    lv_obj_center(label);
    return button;
}

int CueFor(uint32_t color) {
    if (color == kRed) {
        return static_cast<int>(dot_tap_sounds::Cue::Red);
    }
    if (color == kBlue) {
        return static_cast<int>(dot_tap_sounds::Cue::Blue);
    }
    return static_cast<int>(dot_tap_sounds::Cue::Yellow);
}
}  // namespace

DotTapGame::DotTapGame(Display* display, AccelSource* accel) : display_(display), accel_(accel) {}

void DotTapGame::Initialize() {
    if (entry_ || display_ == nullptr || !display_->IsSetupUICalled()) {
        return;
    }
    DisplayLockGuard lock(display_);
    if (!lock || !lv_layer_top()) {
        return;
    }
    auto layer = lv_layer_top();
    entry_ = Chip(layer, "轻点", 84, 4, 72, 38);
    lv_obj_set_style_text_font(entry_, &animal_game_font, 0);

    panel_ = Paper(layer, 320, 240);
    lv_obj_add_flag(panel_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(panel_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_text_font(panel_, &animal_game_font, 0);
    lv_obj_set_style_text_color(panel_, lv_color_hex(kInk), 0);

    for (int index = 0; index < DotTapSession::kDots; ++index) {
        auto dot = lv_obj_create(panel_);
        lv_obj_remove_style_all(dot);
        lv_obj_set_size(dot, kSize, kSize);
        lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
        lv_obj_set_style_transform_pivot_x(dot, kSize / 2, 0);
        lv_obj_set_style_transform_pivot_y(dot, kSize / 2, 0);
        lv_obj_remove_flag(dot, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(dot, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_flag(dot, LV_OBJ_FLAG_HIDDEN);
        dots_[index] = dot;
    }

    rest_ = Paper(panel_, 320, 240);
    lv_obj_add_flag(rest_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(rest_, LV_OBJ_FLAG_HIDDEN);
    auto message = InkLabel(rest_, "休息一下吧");
    lv_obj_set_width(message, 320);
    lv_obj_align(message, LV_ALIGN_TOP_MID, 0, 36);
    auto detail = InkLabel(rest_, "和爸爸妈妈再点一点");
    lv_obj_set_width(detail, 300);
    lv_obj_align(detail, LV_ALIGN_TOP_MID, 0, 72);
    restart_ = Chip(rest_, "家长长按再玩", 40, 120, 240, 52);
    back_ = Chip(rest_, "返回", 228, 8, 80, 40);

    auto on_event = [](lv_event_t* event) {
        auto* self = static_cast<DotTapGame*>(lv_event_get_user_data(event));
        auto* target = static_cast<lv_obj_t*>(lv_event_get_target(event));
        const auto code = lv_event_get_code(event);
        const uint32_t now = lv_tick_get();
        if (code == LV_EVENT_PRESSED) {
            self->pressed_.store(true);
            self->pressed_at_.store(now);
            self->fallback_latched_.store(false);
            return;
        }
        if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
            const bool was_down = self->pressed_.exchange(false);
            const uint32_t held = uint32_t(now - self->pressed_at_.load());
            if (code == LV_EVENT_RELEASED && target == self->restart_ && was_down &&
                held >= DotTapSession::kParentHoldMs &&
                !self->action_pending_.exchange(true)) {
                Application::GetInstance().Schedule([self]() {
                    self->Restart();
                    self->action_pending_.store(false);
                });
            }
            return;
        }
        if (code != LV_EVENT_SHORT_CLICKED) {
            return;
        }
        if (target == self->entry_) {
            Application::GetInstance().Schedule([self]() { self->Enter(); });
            return;
        }
        if (target == self->back_) {
            self->RequestExit();
            return;
        }
        // The release that already counted as a shake is not also a tap.
        if (self->fallback_latched_.exchange(false) || !self->visible_.load()) {
            return;
        }
        int index = -1;
        bool on_dot = false;
        for (int dot = 0; dot < DotTapSession::kDots; ++dot) {
            if (target == self->dots_[dot]) {
                index = dot;
                on_dot = true;
                break;
            }
        }
        if (target == self->panel_) {
            if (self->session_.page() != DotTapSession::Page::Settled) {
                return;
            }
        } else if (!on_dot) {
            return;
        }
        const auto generation = self->generation_;
        if (!self->action_pending_.exchange(true)) {
            Application::GetInstance().Schedule([self, generation, index]() {
                self->Action(generation, index);
                self->action_pending_.store(false);
            });
        }
    };
    lv_obj_add_event_cb(entry_, on_event, LV_EVENT_ALL, this);
    lv_obj_add_event_cb(panel_, on_event, LV_EVENT_ALL, this);
    lv_obj_add_event_cb(restart_, on_event, LV_EVENT_ALL, this);
    lv_obj_add_event_cb(back_, on_event, LV_EVENT_ALL, this);
    for (auto* dot : dots_) {
        lv_obj_add_event_cb(dot, on_event, LV_EVENT_ALL, this);
    }

    lv_timer_create(
        [](lv_timer_t* timer) {
            auto* self = static_cast<DotTapGame*>(lv_timer_get_user_data(timer));
            if (self->visible_.load() && !self->poll_pending_.exchange(true)) {
                Application::GetInstance().Schedule([self]() {
                    self->Poll();
                    self->poll_pending_.store(false);
                });
            }
        },
        50, this);
}

void DotTapGame::Enter() {
    auto& app = Application::GetInstance();
    if (visible_.load() || app.IsLocalInteractionActive() || !app.BeginLocalInteraction()) {
        if (!visible_.load()) {
            display_->ShowNotification("请稍等", 2000);
        }
        return;
    }
    DisplayLockGuard lock(display_);
    if (!lock) {
        app.EndLocalInteraction();
        return;
    }
    ++generation_;
    session_.Start(lv_tick_get());
    shake_.Reset();
    read_failures_ = 0;
    pressed_.store(false);
    fallback_latched_.store(false);
    visible_.store(true);
    lv_obj_add_flag(entry_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(panel_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(panel_);
    ShowPage(DotTapSession::Page::One);
}

void DotTapGame::RequestExit() {
    Application::GetInstance().Schedule([this]() { Exit(); });
}

void DotTapGame::Exit() {
    DisplayLockGuard lock(display_);
    if (!lock || !visible_.load()) {
        return;
    }
    ++generation_;
    session_.Close();
    for (int index = 0; index < DotTapSession::kDots; ++index) {
        HideDot(index);
    }
    pressed_.store(false);
    visible_.store(false);
    lv_obj_add_flag(panel_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(rest_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(entry_, LV_OBJ_FLAG_HIDDEN);
    Application::GetInstance().EndLocalInteraction();
}

void DotTapGame::Poll() {
    auto& app = Application::GetInstance();
    if (!app.IsLocalInteractionActive()) {
        Exit();
        return;
    }
    const uint32_t now = lv_tick_get();
    const auto page = session_.page();
    const bool watch_imu = accel_ != nullptr && accel_->available() && read_failures_ < 8 &&
                           page != DotTapSession::Page::Closed && page != DotTapSession::Page::Rest;
    bool shook = false;
    if (watch_imu) {
        int16_t x = 0;
        int16_t y = 0;
        int16_t z = 0;
        if (!accel_->ReadAccel(x, y, z)) {
            ++read_failures_;
        } else {
            read_failures_ = 0;
            if (shake_.Feed(x, y, z, now) && page == DotTapSession::Page::Mixed) {
                shook = true;
            }
        }
    }
    const bool fallback = accel_ == nullptr || !accel_->available() || read_failures_ >= 8;
    DisplayLockGuard lock(display_);
    if (!lock || !visible_.load()) {
        return;
    }
    if (shook) {
        OnShake(now);
    } else if (fallback && session_.page() == DotTapSession::Page::Mixed && pressed_.load() &&
               !fallback_latched_.load() &&
               uint32_t(now - pressed_at_.load()) >= kFallbackHoldMs) {
        fallback_latched_.store(true);
        OnShake(now);
    }
    if (!session_.Tick(now)) {
        return;
    }
    const auto next = session_.page();
    if (next == DotTapSession::Page::Settling) {
        ShowPage(next);
    } else if (next == DotTapSession::Page::Settled) {
        ShowPage(next);
        PlayCue(static_cast<int>(dot_tap_sounds::Cue::Settle));
    } else if (next == DotTapSession::Page::Rest) {
        ShowPage(next);
        app.GetAudioService().ResetDecoder();
    }
}

void DotTapGame::Action(uint32_t generation, int index) {
    DisplayLockGuard lock(display_);
    auto& app = Application::GetInstance();
    if (!lock || !visible_.load() || generation != generation_ || !app.IsLocalInteractionActive()) {
        return;
    }
    const auto before = session_.page();
    if (!session_.Tap(lv_tick_get(), index)) {
        return;
    }
    const auto after = session_.page();
    if (after == DotTapSession::Page::Rest) {
        ShowPage(after);
        app.GetAudioService().ResetDecoder();
        return;
    }
    if (before == DotTapSession::Page::Mixed && after == DotTapSession::Page::Mixed) {
        if (index >= 0 && index < DotTapSession::kDots) {
            PlayCue(CueFor(kMixed[index].color));
            Bounce(index);
        }
        return;
    }
    if (before == DotTapSession::Page::One || before == DotTapSession::Page::Five ||
        after == DotTapSession::Page::One) {
        PlayCue(static_cast<int>(dot_tap_sounds::Cue::Yellow));
    }
    ShowPage(after);
}

void DotTapGame::Restart() {
    DisplayLockGuard lock(display_);
    if (!lock || !visible_.load() || session_.page() != DotTapSession::Page::Rest ||
        !Application::GetInstance().IsLocalInteractionActive()) {
        return;
    }
    ++generation_;
    session_.Start(lv_tick_get());
    shake_.Reset();
    read_failures_ = 0;
    pressed_.store(false);
    fallback_latched_.store(false);
    ShowPage(DotTapSession::Page::One);
}

void DotTapGame::OnShake(uint32_t now) {
    if (!session_.Shake(now)) {
        return;
    }
    PlayCue(static_cast<int>(dot_tap_sounds::Cue::Scatter));
    ShowPage(DotTapSession::Page::Scattering);
}

void DotTapGame::ShowPage(DotTapSession::Page page) {
    if (page == DotTapSession::Page::Rest) {
        for (int index = 0; index < DotTapSession::kDots; ++index) {
            HideDot(index);
        }
        lv_obj_remove_flag(rest_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(rest_);
        return;
    }
    lv_obj_add_flag(rest_, LV_OBJ_FLAG_HIDDEN);
    if (page == DotTapSession::Page::One) {
        for (int index = 0; index < DotTapSession::kDots; ++index) {
            if (index == DotTapSession::kCenter) {
                const auto& place = kFive[index];
                ShowDot(index, place.x, place.y, place.color, Motion::Place, LV_SCALE_NONE, 0);
            } else {
                HideDot(index);
            }
        }
        return;
    }
    if (page == DotTapSession::Page::Five) {
        for (int index = 0; index < DotTapSession::kDots; ++index) {
            const auto& place = kFive[index];
            const auto motion = index == DotTapSession::kCenter ? Motion::Place : Motion::Pop;
            ShowDot(index, place.x, place.y, place.color, motion, LV_SCALE_NONE, 0);
        }
        return;
    }
    const Place* places = nullptr;
    auto motion = Motion::Glide;
    int scale = LV_SCALE_NONE;
    uint32_t ms = kMixMs;
    if (page == DotTapSession::Page::Mixed) {
        places = kMixed;
    } else if (page == DotTapSession::Page::Scattering) {
        places = kScattered;
        scale = kScatterScale;
        ms = DotTapSession::kScatterMs;
    } else if (page == DotTapSession::Page::Settling) {
        places = kCross;
        motion = Motion::Arrive;
        ms = DotTapSession::kSettleMs;
    } else if (page == DotTapSession::Page::Settled) {
        places = kCross;
        motion = Motion::Place;
        ms = 0;
    } else {
        return;
    }
    for (int index = 0; index < DotTapSession::kDots; ++index) {
        ShowDot(index, places[index].x, places[index].y, places[index].color, motion, scale, ms);
    }
}

void DotTapGame::ShowDot(int index, int x, int y, uint32_t color, Motion motion, int scale,
                         uint32_t ms) {
    auto* dot = dots_[index];
    lv_anim_delete(dot, SetX);
    lv_anim_delete(dot, SetY);
    lv_anim_delete(dot, SetScale);
    lv_obj_remove_flag(dot, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_bg_color(dot, lv_color_hex(color), 0);
    if (motion == Motion::Pop) {
        lv_obj_set_pos(dot, x, y);
        scale_[index] = LV_SCALE_NONE;
        SetScale(dot, 96);
        Animate(dot, SetScale, 96, LV_SCALE_NONE, 220, uint32_t(index) * 40, lv_anim_path_ease_out);
        return;
    }
    if ((motion == Motion::Glide || motion == Motion::Arrive) && ms > 0) {
        const auto path =
            motion == Motion::Arrive ? lv_anim_path_ease_in_out : lv_anim_path_ease_out;
        Animate(dot, SetX, lv_obj_get_x(dot), x, ms, 0, path);
        Animate(dot, SetY, lv_obj_get_y(dot), y, ms, 0, path);
        Animate(dot, SetScale, scale_[index], scale, ms, 0, path);
        scale_[index] = scale;
        return;
    }
    lv_obj_set_pos(dot, x, y);
    scale_[index] = scale;
    SetScale(dot, scale);
}

void DotTapGame::HideDot(int index) {
    auto* dot = dots_[index];
    lv_anim_delete(dot, SetX);
    lv_anim_delete(dot, SetY);
    lv_anim_delete(dot, SetScale);
    scale_[index] = LV_SCALE_NONE;
    SetScale(dot, LV_SCALE_NONE);
    lv_obj_add_flag(dot, LV_OBJ_FLAG_HIDDEN);
}

void DotTapGame::Bounce(int index) {
    if (index < 0 || index >= DotTapSession::kDots) {
        return;
    }
    auto* dot = dots_[index];
    lv_anim_delete(dot, SetScale);
    lv_anim_t anim;
    lv_anim_init(&anim);
    lv_anim_set_var(&anim, dot);
    lv_anim_set_exec_cb(&anim, SetScale);
    lv_anim_set_values(&anim, LV_SCALE_NONE, LV_SCALE_NONE + 70);
    lv_anim_set_duration(&anim, 180);
    lv_anim_set_reverse_duration(&anim, 200);
    lv_anim_set_path_cb(&anim, lv_anim_path_ease_out);
    lv_anim_start(&anim);
}

void DotTapGame::PlayCue(int cue) {
    auto& app = Application::GetInstance();
    if (!app.IsLocalInteractionActive()) {
        return;
    }
    app.PlayLocalInteractionSound(dot_tap_sounds::Get(static_cast<dot_tap_sounds::Cue>(cue)));
}

#endif  // !CONFIG_USE_EMOTE_MESSAGE_STYLE
