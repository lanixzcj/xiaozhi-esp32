#include "transport_touch_game.h"
#ifndef CONFIG_USE_EMOTE_MESSAGE_STYLE
#include "application.h"
#include "transport_game_assets.h"
LV_FONT_DECLARE(animal_game_font);
namespace {
using Phase = TransportGameSession::Phase;
static_assert(transport_game_assets::kCount == TransportGameSession::kCount);
lv_obj_t* Box(lv_obj_t* parent, int x, int y, int w, int h) {
    auto obj = lv_obj_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, w, h);
    lv_obj_set_style_bg_color(obj, lv_color_hex(0xFFF8EA), 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    return obj;
}
lv_obj_t* Label(lv_obj_t* parent, const char* text) {
    auto label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_color(label, lv_color_hex(0x41382F), 0);
    lv_obj_remove_flag(label, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_center(label);
    return label;
}
lv_obj_t* Button(lv_obj_t* parent, const char* text, int x, int y, int w, int h) {
    auto button = Box(parent, x, y, w, h);
    lv_obj_add_flag(button, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_color(button, lv_color_hex(0xE2DFC4), 0);
    lv_obj_set_style_radius(button, 10, 0);
    Label(button, text);
    return button;
}
}  // namespace
void TransportTouchGame::Initialize() {
    if (entry_ || !display_->IsSetupUICalled())
        return;
    DisplayLockGuard lock(display_);
    if (!lock || !lv_layer_top())
        return;
    auto layer = lv_layer_top();
    entry_ = Button(layer, "交通", 240, 4, 72, 38);
    lv_obj_set_style_text_font(entry_, &animal_game_font, 0);
    lv_obj_add_event_cb(
        entry_,
        [](lv_event_t* event) {
            auto self = static_cast<TransportTouchGame*>(lv_event_get_user_data(event));
            Application::GetInstance().Schedule([self]() { self->Enter(); });
        },
        LV_EVENT_SHORT_CLICKED, this);
    panel_ = Box(layer, 0, 0, 320, 240);
    lv_obj_add_flag(panel_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(panel_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_text_font(panel_, &animal_game_font, 0);
    hint_ = Label(panel_, "听一听…");
    lv_obj_align(hint_, LV_ALIGN_TOP_LEFT, 12, 12);
    auto back = Button(panel_, "返回", 240, 4, 72, 38);
    lv_obj_add_event_cb(
        back,
        [](lv_event_t* event) {
            static_cast<TransportTouchGame*>(lv_event_get_user_data(event))->RequestExit();
        },
        LV_EVENT_SHORT_CLICKED, this);
    for (size_t side = 0; side < 2; ++side) {
        cards_[side] = Box(panel_, 12 + int(side) * 156, 50, 140, 130);
        lv_obj_add_flag(cards_[side], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_border_post(cards_[side], true, 0);
        images_[side] = lv_image_create(cards_[side]);
        lv_obj_remove_flag(images_[side], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_center(images_[side]);
        lv_obj_add_event_cb(
            cards_[side],
            [](lv_event_t* event) {
                auto self = static_cast<TransportTouchGame*>(lv_event_get_user_data(event));
                if (!self->visible_.load() || self->session_.phase() != Phase::Choosing)
                    return;
                const int side = lv_event_get_target(event) == self->cards_[0] ? 0 : 1;
                const auto generation = self->generation_;
                const auto round = self->session_.round();
                if (!self->action_pending_.exchange(true)) {
                    Application::GetInstance().Schedule([self, generation, round, side]() {
                        self->Action(generation, round, side);
                        self->action_pending_.store(false);
                    });
                }
            },
            LV_EVENT_SHORT_CLICKED, this);
    }
    listen_ = Button(panel_, "听一听…", 70, 190, 180, 42);
    lv_obj_add_event_cb(
        listen_,
        [](lv_event_t* event) {
            auto self = static_cast<TransportTouchGame*>(lv_event_get_user_data(event));
            if (!self->visible_.load() || !self->session_.CanListen(lv_tick_get()))
                return;
            const auto generation = self->generation_;
            const auto round = self->session_.round();
            if (!self->action_pending_.exchange(true)) {
                Application::GetInstance().Schedule([self, generation, round]() {
                    self->Action(generation, round, -1);
                    self->action_pending_.store(false);
                });
            }
        },
        LV_EVENT_SHORT_CLICKED, this);
    rest_ = Box(panel_, 0, 46, 320, 194);
    lv_obj_add_flag(rest_, LV_OBJ_FLAG_HIDDEN);
    auto message = Label(rest_, "休息一下吧");
    lv_obj_align(message, LV_ALIGN_TOP_MID, 0, 28);
    auto restart = Button(rest_, "家长长按再玩", 52, 105, 216, 52);
    lv_obj_add_event_cb(
        restart,
        [](lv_event_t* event) {
            auto self = static_cast<TransportTouchGame*>(lv_event_get_user_data(event));
            switch (lv_event_get_code(event)) {
                case LV_EVENT_PRESSED:
                    self->pressed_ = true;
                    self->pressed_at_ = lv_tick_get();
                    break;
                case LV_EVENT_PRESS_LOST:
                    self->pressed_ = false;
                    break;
                case LV_EVENT_RELEASED: {
                    const bool restart =
                        self->pressed_ && uint32_t(lv_tick_get() - self->pressed_at_) >= 1500;
                    self->pressed_ = false;
                    if (restart)
                        Application::GetInstance().Schedule([self]() { self->Restart(); });
                    break;
                }
                default:
                    break;
            }
        },
        LV_EVENT_ALL, this);
    lv_timer_create(
        [](lv_timer_t* timer) {
            auto self = static_cast<TransportTouchGame*>(lv_timer_get_user_data(timer));
            if (self->visible_.load() && !self->poll_pending_.exchange(true)) {
                Application::GetInstance().Schedule([self]() {
                    self->Poll();
                    self->poll_pending_.store(false);
                });
            }
        },
        100, this);
}
void TransportTouchGame::Enter() {
    auto& app = Application::GetInstance();
    // Another game's overlay owns the local audio session; never share ownership.
    if (visible_.load() || app.IsLocalInteractionActive() || !app.BeginLocalInteraction())
        return;
    DisplayLockGuard lock(display_);
    if (!lock) {
        app.EndLocalInteraction();
        return;
    }
    ++generation_;
    session_.Start(lv_tick_get(), seed_);
    pressed_ = false;
    visible_.store(true);
    lv_obj_add_flag(entry_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(panel_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(panel_);
    UpdateView();
}
void TransportTouchGame::RequestExit() {
    Application::GetInstance().Schedule([this]() { Exit(); });
}
void TransportTouchGame::Exit() {
    DisplayLockGuard lock(display_);
    if (!lock || !visible_.load())
        return;
    ++generation_;
    session_.Close();
    pressed_ = false;
    visible_.store(false);
    lv_obj_add_flag(panel_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(entry_, LV_OBJ_FLAG_HIDDEN);
    Application::GetInstance().EndLocalInteraction();
}
void TransportTouchGame::Action(uint32_t generation, size_t round, int side) {
    DisplayLockGuard lock(display_);
    auto& app = Application::GetInstance();
    if (!lock || !visible_.load() || generation != generation_ || round != session_.round() ||
        !app.IsLocalInteractionActive())
        return;
    const auto now = lv_tick_get();
    if (side < 0) {
        if (session_.CanListen(now) &&
            app.PlayLocalInteractionSound(transport_game_assets::GetSound(session_.target()))) {
            session_.Listen(now);
        }
    } else {
        session_.Choose(now, size_t(side));
    }
    UpdateView();
}
void TransportTouchGame::Poll() {
    auto& app = Application::GetInstance();
    if (!app.IsLocalInteractionActive()) {
        Exit();
        return;
    }
    DisplayLockGuard lock(display_);
    if (!lock || !visible_.load())
        return;
    if (session_.Tick(lv_tick_get(), app.GetAudioService().IsPlaybackIdle())) {
        if (session_.phase() == Phase::Rest) {
            // Enforce the rest boundary even if a prompt started just before it.
            // Keep local ownership until explicit exit; only stop speaker work.
            app.GetAudioService().ResetDecoder();
        }
        UpdateView();
    }
}
void TransportTouchGame::Restart() {
    DisplayLockGuard lock(display_);
    if (!lock || !visible_.load() || session_.phase() != Phase::Rest ||
        !Application::GetInstance().IsLocalInteractionActive())
        return;
    ++generation_;
    seed_ = (seed_ + 1) % TransportGameSession::kCount;
    session_.Start(lv_tick_get(), seed_);
    UpdateView();
}
void TransportTouchGame::UpdateView() {
    const auto phase = session_.phase();
    if (phase == Phase::Rest) {
        lv_obj_remove_flag(rest_, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(hint_, "交通");
        return;
    }
    lv_obj_add_flag(rest_, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(hint_, phase == Phase::Choosing   ? "选一选"
                             : phase == Phase::Feedback ? (session_.correct() ? "找到了" : "再试试")
                                                        : "听一听…");
    lv_label_set_text(lv_obj_get_child(listen_, 0), phase == Phase::Ready ? "听一听…" : "再听一次");
    if (session_.CanListen(lv_tick_get()))
        lv_obj_remove_state(listen_, LV_STATE_DISABLED);
    else
        lv_obj_add_state(listen_, LV_STATE_DISABLED);
    for (size_t side = 0; side < 2; ++side) {
        lv_image_set_src(images_[side], transport_game_assets::GetImage(session_.choice(side)));
        lv_obj_set_style_border_width(cards_[side],
                                      phase == Phase::Feedback && session_.correct() &&
                                              session_.choice(side) == session_.target()
                                          ? 4
                                          : 0,
                                      0);
        lv_obj_set_style_border_color(cards_[side], lv_color_hex(0x44885A), 0);
    }
}
#endif
