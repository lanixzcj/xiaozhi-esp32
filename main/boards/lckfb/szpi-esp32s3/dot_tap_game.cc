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
// Translation-only transitions avoid five intermediate transformed layers.
constexpr int kScatterScale = LV_SCALE_NONE;
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
    entry_ = Chip(layer, "轻点", 240, 4, 72, 38);
    lv_obj_set_style_text_font(entry_, &animal_game_font, 0);
    UpdateEntryPosition();

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
        lv_obj_add_flag(dot, LV_OBJ_FLAG_PRESS_LOCK);
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
            self->drag_index_ = -1;
            self->dragged_ = false;
            self->suppress_tap_ = false;
            auto input = lv_event_get_indev(event);
            if (self->visible_.load() && input &&
                self->session_.page() != DotTapSession::Page::Rest) {
                for (int i = 0; i < DotTapSession::kDots; ++i) {
                    if (target != self->dots_[i])
                        continue;
                    self->drag_index_ = i;
                    lv_indev_get_point(input, &self->press_point_);
                    self->drag_offset_.x = self->press_point_.x - lv_obj_get_x(target);
                    self->drag_offset_.y = self->press_point_.y - lv_obj_get_y(target);
                    break;
                }
            }
            self->pressed_.store(true);
            self->pressed_at_.store(now);
            self->fallback_latched_.store(false);
            return;
        }
        if (code == LV_EVENT_PRESSING) {
            self->Drag(lv_event_get_indev(event));
            return;
        }
        if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
            if (self->dragged_)
                self->suppress_tap_ = true;
            self->drag_index_ = -1;
            self->dragged_ = false;
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
        if (self->fallback_latched_.exchange(false) || self->suppress_tap_ ||
            !self->visible_.load()) {
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

    Application::GetInstance().SetDotRoundHandler([this](uint32_t id, const std::string& json) {
        if (!visible_.load()) {
            return;
        }
        if (!rounds_.OnReply(id, json.c_str()) && rounds_.failures() < DotRoundQueue::kMaxTries) {
            RequestRound(lv_tick_get());
        }
    });

    lv_timer_create(
        [](lv_timer_t* timer) {
            auto* self = static_cast<DotTapGame*>(lv_timer_get_user_data(timer));
            if (!self->visible_.load()) {
                self->UpdateEntryPosition();
            } else if (!self->poll_pending_.exchange(true)) {
                Application::GetInstance().Schedule([self]() {
                    self->Poll();
                    self->poll_pending_.store(false);
                });
            }
        },
        33, this);
}

void DotTapGame::UpdateEntryPosition() {
    if (!entry_)
        return;
    const int y = display_->GetTopBarBottom() + 4 + 2 * 46;
    if (lv_obj_get_y(entry_) != y)
        lv_obj_set_y(entry_, y);
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
    tilt_origin_samples_ = 0;
    tilt_origin_sum_x_ = tilt_origin_sum_y_ = tilt_origin_sum_z_ = 0;
    drag_index_ = -1;
    dragged_ = false;
    suppress_tap_ = false;
    pressed_.store(false);
    fallback_latched_.store(false);
    visible_.store(true);
    rounds_.Reset();
    lv_obj_add_flag(entry_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(panel_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(panel_);
    ShowPage(DotTapSession::Page::One);
    RequestRound(lv_tick_get());
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
    drag_index_ = -1;
    dragged_ = false;
    suppress_tap_ = false;
    pressed_.store(false);
    visible_.store(false);
    rounds_.Reset();
    lv_obj_add_flag(panel_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(rest_, LV_OBJ_FLAG_HIDDEN);
    UpdateEntryPosition();
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
    int16_t ax = 0;
    int16_t ay = 0;
    int16_t az = 0;
    bool have_accel = false;
    bool shook = false;
    if (watch_imu) {
        if (!accel_->ReadAccel(ax, ay, az)) {
            ++read_failures_;
        } else {
            read_failures_ = 0;
            have_accel = true;
            const bool shake_page = page == DotTapSession::Page::Mixed ||
                                    page == DotTapSession::Page::Custom;
            if (shake_.Feed(ax, ay, az, now) && shake_page) {
                shook = true;
            }
        }
    }
    // A deliberate hold always works, even if a present IMU is hard to shake.
    DisplayLockGuard lock(display_);
    if (!lock || !visible_.load()) {
        return;
    }
    if (shook) {
        OnShake(now);
    } else if ((session_.page() == DotTapSession::Page::Mixed ||
                session_.page() == DotTapSession::Page::Custom) &&
               pressed_.load() && !dragged_ && !fallback_latched_.load() &&
               uint32_t(now - pressed_at_.load()) >= kFallbackHoldMs) {
        fallback_latched_.store(true);
        OnShake(now);
    }
    if (session_.Tick(now)) {
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
    int pull_x = 0;
    int pull_y = 0;
    if (have_accel) {
        if (tilt_origin_samples_ < 4 && !DotRoll::IsGravitySample(ax, ay, az)) {
            tilt_origin_samples_ = 0;
            tilt_origin_sum_x_ = tilt_origin_sum_y_ = tilt_origin_sum_z_ = 0;
        } else if (tilt_origin_samples_ < 4) {
            tilt_origin_sum_x_ += ax;
            tilt_origin_sum_y_ += ay;
            tilt_origin_sum_z_ += az;
            if (++tilt_origin_samples_ == 4) {
                tilt_origin_x_ = int16_t(tilt_origin_sum_x_ / 4);
                tilt_origin_y_ = int16_t(tilt_origin_sum_y_ / 4);
                tilt_origin_z_ = int16_t(tilt_origin_sum_z_ / 4);
            }
        } else {
            DotRoll::MapRelative(ax, ay, az, tilt_origin_x_, tilt_origin_y_, tilt_origin_z_, pull_x,
                                 pull_y);
        }
    }
    if (rounds_.TimedOut(now)) {
        rounds_.Expire();
        if (rounds_.failures() < DotRoundQueue::kMaxTries) {
            RequestRound(now);
        }
    }
    ApplyTilt(now, pull_x, pull_y);
}

bool DotTapGame::SubmitRoundJson(const char* json) { return rounds_.Offer(json); }

void DotTapGame::RequestRound(uint32_t now) {
    const uint32_t id = rounds_.Begin(now);
    if (id == 0) {
        return;
    }
    if (!Application::GetInstance().SendDotRoundRequest(id)) {
        rounds_.AbortSend();
    }
}

void DotTapGame::Drag(lv_indev_t* input) {
    if (!input || drag_index_ < 0 || !visible_.load())
        return;
    const auto page = session_.page();
    if (page == DotTapSession::Page::Scattering || page == DotTapSession::Page::Settling ||
        page == DotTapSession::Page::Rest)
        return;
    lv_point_t point;
    lv_indev_get_point(input, &point);
    const int dx = point.x - press_point_.x;
    const int dy = point.y - press_point_.y;
    if (!dragged_ && dx * dx + dy * dy < 36)
        return;
    dragged_ = true;
    auto dot = dots_[drag_index_];
    lv_anim_delete(dot, SetX);
    lv_anim_delete(dot, SetY);
    roll_.DragTo(drag_index_, point.x - drag_offset_.x, point.y - drag_offset_.y);
    const auto& body = roll_.body(drag_index_);
    lv_obj_set_pos(dot, int(body.x), int(body.y));
    pose_lock_until_ = lv_tick_get();
}

void DotTapGame::ApplyTilt(uint32_t now, int pull_x_mg, int pull_y_mg) {
    if (drag_index_ >= 0) {
        last_tilt_ms_ = now;
        return;
    }
    const auto page = session_.page();
    const bool tilt = page == DotTapSession::Page::One || page == DotTapSession::Page::Five ||
                      page == DotTapSession::Page::Mixed || page == DotTapSession::Page::Custom ||
                      page == DotTapSession::Page::Settled;
    if (!tilt || now < pose_lock_until_) {
        tilt_synced_ = false;
        return;
    }
    if (!tilt_synced_) {
        for (int index = 0; index < DotTapSession::kDots; ++index) {
            if (!roll_.body(index).alive) {
                continue;
            }
            lv_anim_delete(dots_[index], SetX);
            lv_anim_delete(dots_[index], SetY);
        }
        tilt_synced_ = true;
        last_tilt_ms_ = now;
        return;
    }
    const uint32_t dt = uint32_t(now - last_tilt_ms_);
    if (dt == 0) {
        return;
    }
    last_tilt_ms_ = now;
    roll_.Step(pull_x_mg, pull_y_mg, dt);
    for (int index = 0; index < DotTapSession::kDots; ++index) {
        const auto& body = roll_.body(index);
        if (!body.alive) {
            continue;
        }
        lv_obj_set_pos(dots_[index], int(body.x + 0.5f), int(body.y + 0.5f));
    }
}

void DotTapGame::Action(uint32_t generation, int index) {
    DisplayLockGuard lock(display_);
    auto& app = Application::GetInstance();
    if (!lock || !visible_.load() || generation != generation_ || !app.IsLocalInteractionActive()) {
        return;
    }
    const auto before = session_.page();
    const bool advancing = before == DotTapSession::Page::One ||
                           before == DotTapSession::Page::Five ||
                           before == DotTapSession::Page::Settled;
    if (advancing && rounds_.Has()) {
        const uint32_t now = lv_tick_get();
        if (!session_.EnterCustom(now)) {
            ShowPage(session_.page());
            return;
        }
        DotRound round;
        rounds_.Take(round);
        ShowCustom(round);
        RequestRound(now);
        PlayCue(CueFor(round.dots[0].color == DotRound::Color::Red
                           ? kRed
                           : round.dots[0].color == DotRound::Color::Blue ? kBlue
                                                                          : kYellow));
        return;
    }
    if (!session_.Tap(lv_tick_get(), index)) {
        return;
    }
    const auto after = session_.page();
    if (after == DotTapSession::Page::Scattering) {
        PlayCue(static_cast<int>(dot_tap_sounds::Cue::Scatter));
        ShowPage(after);
        return;
    }
    if (after == DotTapSession::Page::Rest) {
        ShowPage(after);
        app.GetAudioService().ResetDecoder();
        return;
    }
    if ((before == DotTapSession::Page::Mixed || before == DotTapSession::Page::Custom) &&
        after == before) {
        if (before == DotTapSession::Page::Custom && index >= 0 && index < custom_.count) {
            const auto color = custom_.dots[index].color;
            PlayCue(CueFor(color == DotRound::Color::Red
                               ? kRed
                               : color == DotRound::Color::Blue ? kBlue
                                                                : kYellow));
            Bounce(index);
            return;
        }
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
    tilt_origin_samples_ = 0;
    tilt_origin_sum_x_ = tilt_origin_sum_y_ = tilt_origin_sum_z_ = 0;
    drag_index_ = -1;
    dragged_ = false;
    suppress_tap_ = false;
    pressed_.store(false);
    fallback_latched_.store(false);
    ShowPage(DotTapSession::Page::One);
    RequestRound(lv_tick_get());
}

void DotTapGame::OnShake(uint32_t now) {
    if (!session_.Shake(now)) {
        return;
    }
    PlayCue(static_cast<int>(dot_tap_sounds::Cue::Scatter));
    ShowPage(DotTapSession::Page::Scattering);
}

void DotTapGame::ShowCustom(const DotRound& round) {
    custom_ = round;
    tilt_synced_ = false;
    lv_obj_add_flag(rest_, LV_OBJ_FLAG_HIDDEN);
    int rank[DotRound::kMaxDots] = {};
    for (int i = 0; i < round.count; ++i) {
        for (int j = 0; j < round.count; ++j) {
            const bool before = round.dots[j].order < round.dots[i].order ||
                                (round.dots[j].order == round.dots[i].order && j < i);
            if (before) {
                ++rank[i];
            }
        }
    }
    uint32_t last_delay = 0;
    for (int i = 0; i < DotTapSession::kDots; ++i) {
        if (i >= round.count) {
            HideDot(i);
            continue;
        }
        const auto& dot = round.dots[i];
        const uint32_t color = dot.color == DotRound::Color::Red
                                   ? kRed
                                   : dot.color == DotRound::Color::Blue ? kBlue
                                                                        : kYellow;
        const uint32_t delay = uint32_t(rank[i]) * uint32_t(round.gap_ms);
        if (delay > last_delay) {
            last_delay = delay;
        }
        ShowDot(i, dot.x, dot.y, color, Motion::Pop, LV_SCALE_NONE, 0, dot.size, delay);
    }
    pose_lock_until_ = lv_tick_get() + last_delay + 220;
}

void DotTapGame::ShowPage(DotTapSession::Page page) {
    tilt_synced_ = false;
    uint32_t lock_ms = 0;
    if (page == DotTapSession::Page::Rest) {
        for (int index = 0; index < DotTapSession::kDots; ++index) {
            HideDot(index);
        }
        lv_obj_remove_flag(rest_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(rest_);
        pose_lock_until_ = lv_tick_get();
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
        pose_lock_until_ = lv_tick_get();
        return;
    }
    if (page == DotTapSession::Page::Five) {
        for (int index = 0; index < DotTapSession::kDots; ++index) {
            const auto& place = kFive[index];
            const auto motion = index == DotTapSession::kCenter ? Motion::Place : Motion::Pop;
            ShowDot(index, place.x, place.y, place.color, motion, LV_SCALE_NONE, 0);
        }
        pose_lock_until_ = lv_tick_get();
        return;
    }
    const Place* places = nullptr;
    auto motion = Motion::Glide;
    int scale = LV_SCALE_NONE;
    uint32_t ms = kMixMs;
    if (page == DotTapSession::Page::Mixed) {
        places = kMixed;
        lock_ms = kMixMs;
    } else if (page == DotTapSession::Page::Scattering) {
        places = kScattered;
        scale = kScatterScale;
        ms = DotTapSession::kScatterMs;
        lock_ms = ms;
    } else if (page == DotTapSession::Page::Settling) {
        places = kCross;
        motion = Motion::Arrive;
        ms = DotTapSession::kSettleMs;
        lock_ms = ms;
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
    pose_lock_until_ = lv_tick_get() + lock_ms;
}

void DotTapGame::ShowDot(int index, int x, int y, uint32_t color, Motion motion, int scale,
                         uint32_t ms, int dot_size, uint32_t pop_delay) {
    if (dot_size < DotRound::kMinSize) {
        dot_size = DotRound::kMinSize;
    }
    roll_.NoteHome(index, float(x), float(y), true, float(dot_size));
    auto* dot = dots_[index];
    lv_anim_delete(dot, SetX);
    lv_anim_delete(dot, SetY);
    lv_anim_delete(dot, SetScale);
    lv_obj_remove_flag(dot, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_size(dot, dot_size, dot_size);
    lv_obj_set_style_transform_pivot_x(dot, dot_size / 2, 0);
    lv_obj_set_style_transform_pivot_y(dot, dot_size / 2, 0);
    lv_obj_set_style_bg_color(dot, lv_color_hex(color), 0);
    if (motion == Motion::Pop) {
        lv_obj_set_pos(dot, x, y);
        scale_[index] = LV_SCALE_NONE;
        SetScale(dot, 96);
        const uint32_t delay = pop_delay == 0xFFFFFFFFu ? uint32_t(index) * 40 : pop_delay;
        Animate(dot, SetScale, 96, LV_SCALE_NONE, 220, delay, lv_anim_path_ease_out);
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
    roll_.NoteHome(index, 0, 0, false);
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
