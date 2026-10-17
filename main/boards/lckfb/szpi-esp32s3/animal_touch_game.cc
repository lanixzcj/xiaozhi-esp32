#include "animal_touch_game.h"

#ifndef CONFIG_USE_EMOTE_MESSAGE_STYLE

#include "animal_game_sounds.h"
#include "application.h"

LV_FONT_DECLARE(animal_game_font);

namespace {
constexpr uint32_t kInk = 0x41382F;
constexpr uint32_t kPaper = 0xFFF8EA;
constexpr uint32_t kCard = 0xF4E9D4;
constexpr const char* kNames[] = {"小猫", "小狗", "小鸭"};
static_assert(animal_game_assets::kSoundCount == AnimalGameSession::kAnimalCount);

lv_obj_t* Box(lv_obj_t* parent, int x, int y, int width, int height, uint32_t color,
              int radius = 0) {
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

lv_obj_t* Label(lv_obj_t* parent, const char* text, int x, int y, int width) {
    auto label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_pos(label, x, y);
    lv_obj_set_width(label, width);
    lv_obj_set_style_text_color(label, lv_color_hex(kInk), 0);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_remove_flag(label, LV_OBJ_FLAG_CLICKABLE);
    return label;
}

lv_obj_t* Button(lv_obj_t* parent, const char* text, int x, int y, int width, int height) {
    auto button = Box(parent, x, y, width, height, 0xE2DFC4, 10);
    lv_obj_add_flag(button, LV_OBJ_FLAG_CLICKABLE);
    auto label = Label(button, text, 0, 0, width);
    lv_obj_center(label);
    return button;
}

void Rect(lv_layer_t* layer, int x, int y, int width, int height, uint32_t color,
          int radius = LV_RADIUS_CIRCLE) {
    lv_draw_rect_dsc_t style;
    lv_draw_rect_dsc_init(&style);
    style.bg_color = lv_color_hex(color);
    style.bg_opa = LV_OPA_COVER;
    style.radius = radius;
    lv_area_t area = {x, y, x + width - 1, y + height - 1};
    lv_draw_rect(layer, &style, &area);
}

void Triangle(lv_layer_t* layer, int x1, int y1, int x2, int y2, int x3, int y3, uint32_t color) {
    lv_draw_triangle_dsc_t style;
    lv_draw_triangle_dsc_init(&style);
    style.p[0].x = x1;
    style.p[0].y = y1;
    style.p[1].x = x2;
    style.p[1].y = y2;
    style.p[2].x = x3;
    style.p[2].y = y3;
    style.color = lv_color_hex(color);
    style.opa = LV_OPA_COVER;
    lv_draw_triangle(layer, &style);
}
}  // namespace

void AnimalTouchGame::Initialize() {
    if (entry_ || !display_->IsSetupUICalled()) {
        return;
    }
    DisplayLockGuard lock(display_);
    if (!lock) {
        return;
    }
    auto layer = lv_layer_top();
    if (!layer) {
        return;
    }
    entry_ = Button(layer, "动物", 240, 196, 72, 40);
    lv_obj_set_style_text_font(entry_, &animal_game_font, 0);
    lv_obj_add_event_cb(
        entry_,
        [](lv_event_t* event) {
            auto self = static_cast<AnimalTouchGame*>(lv_event_get_user_data(event));
            Application::GetInstance().Schedule([self]() { self->Enter(); });
        },
        LV_EVENT_SHORT_CLICKED, this);

    panel_ = Box(layer, 0, 0, 320, 240, kPaper);
    lv_obj_add_flag(panel_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(panel_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_text_font(panel_, &animal_game_font, 0);
    title_ = Label(panel_, "小猫", 12, 10, 210);
    auto back = Button(panel_, "返回", 240, 4, 72, 38);
    lv_obj_add_event_cb(
        back,
        [](lv_event_t* event) {
            static_cast<AnimalTouchGame*>(lv_event_get_user_data(event))->RequestExit();
        },
        LV_EVENT_SHORT_CLICKED, this);

    card_ = Box(panel_, 12, 46, 296, 162, kCard, 24);
    lv_obj_add_flag(card_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(card_, DrawAnimal, LV_EVENT_DRAW_MAIN, this);
    lv_obj_add_event_cb(
        card_,
        [](lv_event_t* event) {
            auto self = static_cast<AnimalTouchGame*>(lv_event_get_user_data(event));
            if (!self->tap_pending_.exchange(true)) {
                Application::GetInstance().Schedule([self]() {
                    self->Tap();
                    self->tap_pending_.store(false);
                });
            }
        },
        LV_EVENT_SHORT_CLICKED, this);
    hint_ = Label(panel_, "轻轻点一下", 12, 212, 296);

    rest_ = Box(panel_, 0, 46, 320, 194, kPaper);
    lv_obj_add_flag(rest_, LV_OBJ_FLAG_HIDDEN);
    Label(rest_, "休息一下吧", 12, 22, 296);
    Label(rest_, "和爸爸妈妈说说小动物", 12, 62, 296);
    restart_ = Button(rest_, "家长长按再玩", 52, 118, 216, 52);
    lv_obj_add_event_cb(
        restart_,
        [](lv_event_t* event) {
            auto self = static_cast<AnimalTouchGame*>(lv_event_get_user_data(event));
            switch (lv_event_get_code(event)) {
                case LV_EVENT_PRESSED:
                    self->restart_pressed_ = true;
                    self->restart_pressed_at_ = lv_tick_get();
                    break;
                case LV_EVENT_PRESS_LOST:
                    self->restart_pressed_ = false;
                    break;
                case LV_EVENT_RELEASED: {
                    bool restart = self->restart_pressed_ &&
                                   uint32_t(lv_tick_get() - self->restart_pressed_at_) >=
                                       AnimalGameSession::kParentHoldMs;
                    self->restart_pressed_ = false;
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

    // A single outstanding poll bounds the main-task queue even during a stall.
    timer_ = lv_timer_create(
        [](lv_timer_t* timer) {
            auto self = static_cast<AnimalTouchGame*>(lv_timer_get_user_data(timer));
            if (self->visible_.load() && !self->poll_pending_.exchange(true)) {
                Application::GetInstance().Schedule([self]() {
                    self->Poll();
                    self->poll_pending_.store(false);
                });
            }
        },
        100, this);
}

void AnimalTouchGame::Enter() {
    if (visible_.load()) {
        return;
    }
    auto& app = Application::GetInstance();
    if (!app.BeginLocalInteraction()) {
        display_->ShowNotification("请稍等", 2000);
        return;
    }
    DisplayLockGuard lock(display_);
    if (!lock) {
        app.EndLocalInteraction();
        return;
    }
    session_.Start(lv_tick_get());
    restart_pressed_ = false;
    visible_.store(true);
    lv_obj_add_flag(entry_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(panel_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(panel_);
    UpdateView();
}

void AnimalTouchGame::RequestExit() {
    Application::GetInstance().Schedule([this]() { Exit(); });
}

void AnimalTouchGame::Exit() {
    DisplayLockGuard lock(display_);
    if (!lock || !visible_.load()) {
        return;
    }
    session_.Close();
    restart_pressed_ = false;
    visible_.store(false);
    lv_obj_add_flag(panel_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(entry_, LV_OBJ_FLAG_HIDDEN);
    Application::GetInstance().EndLocalInteraction();
}

void AnimalTouchGame::Tap() {
    DisplayLockGuard lock(display_);
    if (!lock || !visible_.load()) {
        return;
    }
    const auto now = lv_tick_get();
    if (session_.CanTap(now) && Application::GetInstance().PlayLocalInteractionSound(
                                    animal_game_assets::GetSound(session_.animal()))) {
        session_.AcceptTap(now);
        UpdateView();
    }
}

void AnimalTouchGame::Poll() {
    auto& app = Application::GetInstance();
    // Leave the overlay if provisioning/upgrade or another lifecycle transition
    // has explicitly ended the local interaction.
    if (!app.IsLocalInteractionActive()) {
        Exit();
        return;
    }
    DisplayLockGuard lock(display_);
    if (!lock || !visible_.load()) {
        return;
    }
    if (session_.Tick(lv_tick_get(), app.GetAudioService().IsPlaybackIdle())) {
        UpdateView();
    }
}

void AnimalTouchGame::Restart() {
    DisplayLockGuard lock(display_);
    if (!lock || !visible_.load() || session_.phase() != AnimalGameSession::Phase::Rest ||
        !Application::GetInstance().IsLocalInteractionActive()) {
        return;
    }
    session_.Start(lv_tick_get());
    UpdateView();
}

void AnimalTouchGame::UpdateView() {
    bool rest = session_.phase() == AnimalGameSession::Phase::Rest;
    if (rest) {
        lv_obj_remove_flag(rest_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(card_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(hint_, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(title_, "动物朋友");
    } else {
        lv_obj_add_flag(rest_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(card_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(hint_, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(title_, kNames[session_.animal()]);
        lv_label_set_text(hint_, session_.phase() == AnimalGameSession::Phase::Feedback
                                     ? "听一听…"
                                     : "轻轻点一下");
        lv_obj_invalidate(card_);
    }
}

void AnimalTouchGame::DrawAnimal(lv_event_t* event) {
    auto self = static_cast<AnimalTouchGame*>(lv_event_get_user_data(event));
    auto layer = lv_event_get_layer(event);
    lv_area_t bounds;
    lv_obj_get_coords(self->card_, &bounds);
    // Original vector drawings. Coordinates are local to the large touch card.
    const int x = bounds.x1 + 58;
    const int y = bounds.y1 + 8;
    const auto animal = self->session_.animal();
    if (animal == 0) {
        Triangle(layer, x + 25, y + 58, x + 24, y + 5, x + 70, y + 40, 0xDE9C5C);
        Triangle(layer, x + 109, y + 40, x + 156, y + 5, x + 155, y + 58, 0xDE9C5C);
        Triangle(layer, x + 32, y + 45, x + 32, y + 20, x + 53, y + 39, 0xEAB2A1);
        Triangle(layer, x + 126, y + 39, x + 148, y + 20, x + 148, y + 45, 0xEAB2A1);
        Rect(layer, x + 23, y + 32, 134, 113, 0xE8AD6D, 52);
        Rect(layer, x + 55, y + 90, 70, 46, 0xFFF0D9, 22);
        Rect(layer, x + 53, y + 70, 10, 15, kInk);
        Rect(layer, x + 117, y + 70, 10, 15, kInk);
        Triangle(layer, x + 81, y + 98, x + 99, y + 98, x + 90, y + 108, 0xB77871);
        Rect(layer, x + 88, y + 108, 4, 11, kInk, 2);
        Rect(layer, x + 7, y + 98, 35, 3, 0x947152, 1);
        Rect(layer, x + 8, y + 109, 34, 3, 0x947152, 1);
        Rect(layer, x + 138, y + 98, 35, 3, 0x947152, 1);
        Rect(layer, x + 138, y + 109, 34, 3, 0x947152, 1);
    } else if (animal == 1) {
        Rect(layer, x + 12, y + 27, 46, 107, 0x8D654D, 22);
        Rect(layer, x + 122, y + 27, 46, 107, 0x8D654D, 22);
        Rect(layer, x + 37, y + 22, 106, 124, 0xD9B38B, 48);
        Rect(layer, x + 51, y + 52, 32, 41, 0xAA7853, 18);
        Rect(layer, x + 64, y + 65, 9, 14, kInk);
        Rect(layer, x + 109, y + 65, 9, 14, kInk);
        Rect(layer, x + 56, y + 88, 69, 47, 0xFFF0D9, 24);
        Rect(layer, x + 77, y + 92, 27, 18, kInk, 9);
        Rect(layer, x + 88, y + 109, 4, 16, kInk, 2);
        Rect(layer, x + 81, y + 123, 18, 15, 0xD69992, 7);
    } else {
        Rect(layer, x + 21, y + 69, 132, 75, 0xEDC866, 37);
        Rect(layer, x + 80, y + 18, 80, 87, 0xF1D477, 39);
        Triangle(layer, x + 35, y + 82, x + 5, y + 61, x + 22, y + 112, 0xEDC866);
        Rect(layer, x + 53, y + 89, 66, 41, 0xDAB45B, 20);
        Rect(layer, x + 126, y + 46, 10, 14, kInk);
        Rect(layer, x + 139, y + 69, 38, 22, 0xD78B50, 10);
        Rect(layer, x + 145, y + 79, 27, 3, 0x9A653F, 1);
    }
}

#endif  // !CONFIG_USE_EMOTE_MESSAGE_STYLE
