#include "listen_pick_touch.h"

#ifndef CONFIG_USE_EMOTE_MESSAGE_STYLE

#include "application.h"
#include "listen_pick_cheer.h"

LV_FONT_DECLARE(animal_game_font);

namespace {
constexpr uint32_t kInk = 0x41382F;
constexpr uint32_t kPaper = 0xFFF8EA;
constexpr uint32_t kCard = 0xFFF6E4;
constexpr uint32_t kCorrect = 0xD8F3C4;
constexpr uint32_t kRim = 0xE2C48A;
constexpr uint32_t kYes = 0x2F8F4E;
constexpr int kCardX = 8;
constexpr int kCardY = 44;
constexpr int kCardW = 146;
constexpr int kCardH = 146;
constexpr int kCardGap = 12;

using Phase = ListenPickSession::Phase;

void SetCardScale(void* object, int32_t value) {
    lv_obj_set_style_transform_scale(static_cast<lv_obj_t*>(object), value, 0);
}

lv_obj_t* Box(lv_obj_t* parent, int x, int y, int width, int height, uint32_t color, int radius) {
    auto object = lv_obj_create(parent);
    lv_obj_remove_style_all(object);
    lv_obj_set_pos(object, x, y);
    lv_obj_set_size(object, width, height);
    lv_obj_set_style_bg_color(object, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(object, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(object, radius, 0);
    lv_obj_remove_flag(object, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(object, LV_OBJ_FLAG_CLICKABLE);
    return object;
}

lv_obj_t* Label(lv_obj_t* parent, const char* text) {
    auto label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_color(label, lv_color_hex(kInk), 0);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_remove_flag(label, LV_OBJ_FLAG_CLICKABLE);
    return label;
}

lv_obj_t* Button(lv_obj_t* parent, const char* text, int x, int y, int width, int height) {
    auto button = Box(parent, x, y, width, height, 0xE2DFC4, 12);
    lv_obj_add_flag(button, LV_OBJ_FLAG_CLICKABLE);
    auto label = Label(button, text);
    lv_obj_set_width(label, width);
    lv_obj_center(label);
    return button;
}

void SetButtonText(lv_obj_t* button, const char* text) {
    lv_label_set_text(lv_obj_get_child(button, 0), text);
}
}  // namespace

ListenPickTouch::ListenPickTouch(Display* display, ListenPickSession& session, const char* label,
                                 int entry_x, ImageFn image, SoundFn sound, const char* rest_detail)
    : display_(display),
      session_(session),
      label_(label),
      entry_x_(entry_x),
      image_(image),
      sound_(sound),
      rest_detail_(rest_detail) {}

void ListenPickTouch::Initialize() {
    if (entry_ || !display_->IsSetupUICalled()) {
        return;
    }
    DisplayLockGuard lock(display_);
    if (!lock || !lv_layer_top()) {
        return;
    }
    auto layer = lv_layer_top();
    entry_ = Button(layer, label_, entry_x_, 4, 72, 38);
    lv_obj_set_style_text_font(entry_, &animal_game_font, 0);
    lv_obj_add_event_cb(
        entry_,
        [](lv_event_t* event) {
            auto self = static_cast<ListenPickTouch*>(lv_event_get_user_data(event));
            Application::GetInstance().Schedule([self]() { self->Enter(); });
        },
        LV_EVENT_SHORT_CLICKED, this);

    panel_ = Box(layer, 0, 0, 320, 240, kPaper, 0);
    lv_obj_add_flag(panel_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(panel_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_text_font(panel_, &animal_game_font, 0);
    hint_ = Label(panel_, "听一听…");
    lv_obj_align(hint_, LV_ALIGN_TOP_LEFT, 12, 8);
    auto back = Button(panel_, "返回", 228, 2, 84, 40);
    lv_obj_add_event_cb(
        back,
        [](lv_event_t* event) {
            static_cast<ListenPickTouch*>(lv_event_get_user_data(event))->RequestExit();
        },
        LV_EVENT_SHORT_CLICKED, this);

    for (size_t side = 0; side < 2; ++side) {
        cards_[side] = Box(panel_, kCardX + static_cast<int>(side) * (kCardW + kCardGap), kCardY,
                           kCardW, kCardH, kCard, 18);
        lv_obj_add_flag(cards_[side], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_clip_corner(cards_[side], true, 0);
        lv_obj_set_style_transform_pivot_x(cards_[side], kCardW / 2, 0);
        lv_obj_set_style_transform_pivot_y(cards_[side], kCardH / 2, 0);
        lv_obj_set_style_outline_pad(cards_[side], 1, 0);
        images_[side] = lv_image_create(cards_[side]);
        lv_obj_remove_flag(images_[side], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_remove_flag(images_[side], LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_size(images_[side], kCardW - 8, kCardH - 8);
        lv_image_set_inner_align(images_[side], LV_IMAGE_ALIGN_CONTAIN);
        lv_image_set_antialias(images_[side], true);
        lv_obj_center(images_[side]);
        lv_obj_add_event_cb(
            cards_[side],
            [](lv_event_t* event) {
                auto self = static_cast<ListenPickTouch*>(lv_event_get_user_data(event));
                // Ignore presses while a sound is playing so a tap cannot queue.
                if (!self->visible_.load() || self->session_.phase() != Phase::Choosing) {
                    return;
                }
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

    listen_ = Button(panel_, "听一听…", 8, 196, 304, 40);
    lv_obj_set_style_opa(listen_, LV_OPA_50, LV_STATE_DISABLED);
    lv_obj_add_event_cb(
        listen_,
        [](lv_event_t* event) {
            auto self = static_cast<ListenPickTouch*>(lv_event_get_user_data(event));
            if (!self->visible_.load() || !self->session_.CanListen(lv_tick_get())) {
                return;
            }
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

    rest_ = Box(panel_, 0, 44, 320, 196, kPaper, 0);
    lv_obj_add_flag(rest_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(rest_, LV_OBJ_FLAG_HIDDEN);
    auto message = Label(rest_, "休息一下吧");
    lv_obj_set_width(message, 320);
    lv_obj_align(message, LV_ALIGN_TOP_MID, 0, 24);
    auto detail = Label(rest_, rest_detail_ ? rest_detail_ : "");
    lv_obj_set_width(detail, 300);
    lv_obj_align(detail, LV_ALIGN_TOP_MID, 0, 58);
    auto restart = Button(rest_, "家长长按再玩", 40, 108, 240, 52);
    lv_obj_add_event_cb(
        restart,
        [](lv_event_t* event) {
            auto self = static_cast<ListenPickTouch*>(lv_event_get_user_data(event));
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
                        self->pressed_ && uint32_t(lv_tick_get() - self->pressed_at_) >=
                                              ListenPickSession::kParentHoldMs;
                    self->pressed_ = false;
                    if (restart) {
                        Application::GetInstance().Schedule([self]() { self->Restart(); });
                    }
                    break;
                }
                default:
                    break;
            }
        },
        LV_EVENT_ALL, this);

    lv_timer_create(
        [](lv_timer_t* timer) {
            auto self = static_cast<ListenPickTouch*>(lv_timer_get_user_data(timer));
            if (self->visible_.load() && !self->poll_pending_.exchange(true)) {
                Application::GetInstance().Schedule([self]() {
                    self->Poll();
                    self->poll_pending_.store(false);
                });
            }
        },
        100, this);
}

void ListenPickTouch::Enter() {
    auto& app = Application::GetInstance();
    // The other game already owns local audio, or this device cannot start it.
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
    session_.Start(lv_tick_get(), seed_);
    pressed_ = false;
    visible_.store(true);
    lv_obj_add_flag(entry_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(panel_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(panel_);
    UpdateView();
    StartPrompt();
}

void ListenPickTouch::RequestExit() {
    Application::GetInstance().Schedule([this]() { Exit(); });
}

void ListenPickTouch::Exit() {
    DisplayLockGuard lock(display_);
    if (!lock || !visible_.load()) {
        return;
    }
    ++generation_;
    session_.Close();
    pressed_ = false;
    ClearBounce();
    visible_.store(false);
    lv_obj_add_flag(panel_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(entry_, LV_OBJ_FLAG_HIDDEN);
    Application::GetInstance().EndLocalInteraction();
}

void ListenPickTouch::Action(uint32_t generation, size_t round, int side) {
    DisplayLockGuard lock(display_);
    auto& app = Application::GetInstance();
    if (!lock || !visible_.load() || generation != generation_ || round != session_.round() ||
        !app.IsLocalInteractionActive()) {
        return;
    }
    if (side < 0) {
        StartPrompt();
        return;
    }
    const auto now = lv_tick_get();
    if (!session_.Choose(now, static_cast<size_t>(side))) {
        return;
    }
    if (session_.correct()) {
        // Cheer is feedback only. The next prompt starts after it finishes.
        app.PlayLocalInteractionSound(listen_pick_cheer::Get());
        Bounce(static_cast<size_t>(side));
    } else {
        ClearBounce();
        // Gentle retry: play the same prompt again, with no buzzer and no score.
        const auto prompt = sound_(session_.target());
        if (!prompt.empty()) {
            app.PlayLocalInteractionSound(prompt);
        }
    }
    UpdateView();
}

void ListenPickTouch::Poll() {
    auto& app = Application::GetInstance();
    if (!app.IsLocalInteractionActive()) {
        Exit();
        return;
    }
    DisplayLockGuard lock(display_);
    if (!lock || !visible_.load()) {
        return;
    }
    if (!session_.Tick(lv_tick_get(), app.GetAudioService().IsPlaybackIdle())) {
        return;
    }
    if (session_.phase() == Phase::Rest) {
        // Drop a prompt that started in the same moment the safety rest began.
        app.GetAudioService().ResetDecoder();
    }
    UpdateView();
    if (session_.phase() == Phase::Ready) {
        StartPrompt();
    }
}

void ListenPickTouch::Restart() {
    DisplayLockGuard lock(display_);
    if (!lock || !visible_.load() || session_.phase() != Phase::Rest ||
        !Application::GetInstance().IsLocalInteractionActive()) {
        return;
    }
    ++generation_;
    seed_ = (seed_ + 1) % session_.count();
    session_.Start(lv_tick_get(), seed_);
    pressed_ = false;
    ClearBounce();
    UpdateView();
    StartPrompt();
}

void ListenPickTouch::StartPrompt() {
    auto& app = Application::GetInstance();
    const auto now = lv_tick_get();
    if (!session_.CanListen(now)) {
        return;
    }
    const auto prompt = sound_(session_.target());
    if (prompt.empty() || !app.PlayLocalInteractionSound(prompt)) {
        UpdateView();
        return;
    }
    session_.Listen(now);
    UpdateView();
}

void ListenPickTouch::UpdateView() {
    const auto phase = session_.phase();
    const bool celebrate = phase == Phase::Feedback && session_.correct();
    if (!celebrate) {
        ClearBounce();
    }
    if (phase == Phase::Rest) {
        // Drop the celebration rim. It is drawn outside the card and would
        // otherwise show above the rest panel.
        for (size_t side = 0; side < 2; ++side) {
            lv_obj_set_style_outline_width(cards_[side], 0, 0);
            lv_obj_set_style_bg_color(cards_[side], lv_color_hex(kCard), 0);
        }
        lv_obj_remove_flag(rest_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(rest_);
        lv_label_set_text(hint_, label_);
        return;
    }
    lv_obj_add_flag(rest_, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(hint_, phase == Phase::Choosing   ? "选一选"
                             : phase == Phase::Feedback ? (session_.correct() ? "找到了" : "再试试")
                                                        : "听一听…");
    SetButtonText(listen_, phase == Phase::Ready ? "听一听…" : "再听一次");
    if (session_.CanListen(lv_tick_get())) {
        lv_obj_remove_state(listen_, LV_STATE_DISABLED);
    } else {
        lv_obj_add_state(listen_, LV_STATE_DISABLED);
    }
    for (size_t side = 0; side < 2; ++side) {
        lv_image_set_src(images_[side], image_(session_.choice(side)));
        lv_image_set_inner_align(images_[side], LV_IMAGE_ALIGN_CONTAIN);
        const bool match = celebrate && session_.choice(side) == session_.target();
        lv_obj_set_style_bg_color(cards_[side], lv_color_hex(match ? kCorrect : kCard), 0);
        lv_obj_set_style_outline_width(cards_[side], match ? 8 : 3, 0);
        lv_obj_set_style_outline_color(cards_[side], lv_color_hex(match ? kYes : kRim), 0);
    }
}

void ListenPickTouch::Bounce(size_t side) {
    ClearBounce();
    if (side >= 2) {
        return;
    }
    lv_anim_t anim;
    lv_anim_init(&anim);
    lv_anim_set_var(&anim, cards_[side]);
    lv_anim_set_exec_cb(&anim, SetCardScale);
    lv_anim_set_values(&anim, LV_SCALE_NONE, LV_SCALE_NONE + 80);
    lv_anim_set_duration(&anim, 240);
    lv_anim_set_reverse_duration(&anim, 320);
    lv_anim_set_path_cb(&anim, lv_anim_path_ease_out);
    lv_anim_start(&anim);
}

void ListenPickTouch::ClearBounce() {
    for (auto* card : cards_) {
        if (card == nullptr) {
            continue;
        }
        lv_anim_delete(card, SetCardScale);
        lv_obj_set_style_transform_scale(card, LV_SCALE_NONE, 0);
    }
}

#endif  // !CONFIG_USE_EMOTE_MESSAGE_STYLE
