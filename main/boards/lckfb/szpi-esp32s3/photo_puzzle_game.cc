#include "photo_puzzle_game.h"
#ifndef CONFIG_USE_EMOTE_MESSAGE_STYLE
#include <algorithm>
#include <cstdlib>
#include "application.h"
#include "puzzle_game_assets.h"
LV_FONT_DECLARE(animal_game_font);
namespace {
constexpr int kW = 148, kH = 162, kX = 12, kY = 58;
constexpr uint32_t kPaper = 0xFFF8EA;
lv_obj_t* Box(lv_obj_t* parent, int x, int y, int w, int h, uint32_t color) {
    auto obj = lv_obj_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, w, h);
    lv_obj_set_style_bg_color(obj, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_CLICKABLE);
    return obj;
}
lv_obj_t* Label(lv_obj_t* parent, const char* text) {
    auto obj = lv_label_create(parent);
    lv_label_set_text(obj, text);
    lv_obj_set_style_text_color(obj, lv_color_hex(0x41382F), 0);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_CLICKABLE);
    return obj;
}
lv_obj_t* Button(lv_obj_t* parent, const char* text, int x, int y, int w) {
    auto obj = Box(parent, x, y, w, 38, 0xE2DFC4);
    lv_obj_set_style_radius(obj, 12, 0);
    lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
    auto label = Label(obj, text);
    lv_obj_center(label);
    return obj;
}
}  // namespace
void PhotoPuzzleGame::Initialize() {
    if (entry_ || !display_ || !display_->IsSetupUICalled())
        return;
    DisplayLockGuard lock(display_);
    if (!lock || !lv_layer_top())
        return;
    entry_ = Button(lv_layer_top(), "拼图", 240, 4, 72);
    lv_obj_set_style_text_font(entry_, &animal_game_font, 0);
    UpdateEntryPosition();
    panel_ = Box(lv_layer_top(), 0, 0, 320, 240, kPaper);
    lv_obj_set_style_text_font(panel_, &animal_game_font, 0);
    ghost_ = lv_image_create(panel_);
    lv_obj_set_pos(ghost_, kX, kY);
    lv_obj_set_style_outline_color(ghost_, lv_color_hex(0xC9B887), 0);
    lv_obj_set_style_outline_width(ghost_, 2, 0);
    lv_obj_set_style_image_opa(ghost_, 50, 0);
    lv_obj_remove_flag(ghost_, LV_OBJ_FLAG_CLICKABLE);
    title_ = Label(panel_, "拖一拖");
    lv_obj_set_pos(title_, 100, 12);
    for (int i = 0; i < 2; ++i) {
        pieces_[i] = Box(panel_, kX, kY, kW, kH, kPaper);
        lv_obj_add_flag(pieces_[i], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_flag(pieces_[i], LV_OBJ_FLAG_PRESS_LOCK);
        images_[i] = lv_image_create(pieces_[i]);
        lv_obj_set_pos(images_[i], -i * kW, 0);
        lv_obj_remove_flag(images_[i], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(pieces_[i], Event, LV_EVENT_ALL, this);
    }
    auto back = Button(panel_, "返回", 8, 6, 72);
    next_ = Button(panel_, "再玩", 216, 6, 96);
    preview_ = lv_image_create(panel_);
    lv_obj_set_pos(preview_, 108, 4);
    lv_obj_set_size(preview_, 80, 44);
    // Native-size preview avoids indexed-image scaling on the embedded renderer.
    lv_obj_remove_flag(preview_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(entry_, Event, LV_EVENT_SHORT_CLICKED, this);
    lv_obj_add_event_cb(
        back,
        [](lv_event_t* event) {
            static_cast<PhotoPuzzleGame*>(lv_event_get_user_data(event))->RequestExit();
        },
        LV_EVENT_SHORT_CLICKED, this);
    lv_obj_add_event_cb(next_, Event, LV_EVENT_SHORT_CLICKED, this);
    lv_obj_add_flag(panel_, LV_OBJ_FLAG_HIDDEN);
    lv_timer_create(
        [](lv_timer_t* timer) {
            auto self = static_cast<PhotoPuzzleGame*>(lv_timer_get_user_data(timer));
            self->UpdateEntryPosition();
            if (self->visible_.load() && !Application::GetInstance().IsLocalInteractionActive())
                self->RequestExit();
        },
        500, this);
}
void PhotoPuzzleGame::UpdateEntryPosition() {
    if (entry_)
        lv_obj_set_y(entry_, display_->GetTopBarBottom() + 4 + 3 * 46);
}
void PhotoPuzzleGame::Enter() {
    auto& app = Application::GetInstance();
    if (visible_.load() || app.IsLocalInteractionActive() || !app.BeginLocalInteraction())
        return;
    DisplayLockGuard lock(display_);
    if (!lock) {
        app.EndLocalInteraction();
        return;
    }
    visible_.store(true);
    round_ = 0;
    lv_obj_add_flag(entry_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(panel_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(panel_);
    Round();
}
void PhotoPuzzleGame::Round() {
    auto photo = puzzle_game_assets::GetImage(round_ % puzzle_game_assets::kCount);
    lv_image_set_src(ghost_, photo);
    lv_obj_set_style_outline_color(ghost_, lv_color_hex(0xC9B887), 0);
    lv_image_set_src(preview_, puzzle_game_assets::GetPreview(round_ % puzzle_game_assets::kCount));
    lv_obj_remove_flag(preview_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(title_, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(title_, "拖一拖");
    lv_obj_add_flag(next_, LV_OBJ_FLAG_HIDDEN);
    drag_ = -1;
    for (int i = 0; i < 2; ++i) {
        placed_[i] = false;
        lv_image_set_src(images_[i], photo);
        lv_obj_set_pos(pieces_[i], kX + (1 - i) * kW, kY);
        lv_obj_set_style_outline_color(pieces_[i], lv_color_hex(0xC9B887), 0);
        lv_obj_set_style_outline_width(pieces_[i], 2, 0);
        lv_obj_add_flag(pieces_[i], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_move_foreground(pieces_[i]);
    }
}
void PhotoPuzzleGame::RequestExit() {
    Application::GetInstance().Schedule([this]() { Exit(); });
}
void PhotoPuzzleGame::Exit() {
    DisplayLockGuard lock(display_);
    if (!lock || !visible_.load())
        return;
    drag_ = -1;
    visible_.store(false);
    lv_obj_add_flag(panel_, LV_OBJ_FLAG_HIDDEN);
    UpdateEntryPosition();
    lv_obj_remove_flag(entry_, LV_OBJ_FLAG_HIDDEN);
    Application::GetInstance().EndLocalInteraction();
}
void PhotoPuzzleGame::Move(lv_indev_t* input) {
    if (!visible_.load() || drag_ < 0 || !input)
        return;
    lv_point_t point;
    lv_indev_get_point(input, &point);
    lv_obj_set_pos(pieces_[drag_], std::clamp(int(point.x - offset_.x), 0, 320 - kW),
                   std::clamp(int(point.y - offset_.y), 0, 240 - kH));
}
void PhotoPuzzleGame::Release() {
    if (drag_ < 0)
        return;
    const int i = drag_;
    drag_ = -1;
    auto piece = pieces_[i];
    const int target = kX + i * kW;
    // Generous snap tolerance; an incomplete attempt stays where released.
    if (std::abs(lv_obj_get_x(piece) - target) > 52 || std::abs(lv_obj_get_y(piece) - kY) > 40)
        return;
    placed_[i] = true;
    lv_obj_set_pos(piece, target, kY);
    lv_obj_remove_flag(piece, LV_OBJ_FLAG_CLICKABLE);
    // Keep the unplaced half reachable even when it is under the matched one.
    lv_obj_move_to_index(piece, lv_obj_get_index(ghost_) + 1);
    lv_obj_set_style_outline_color(piece, lv_color_hex(0x58965B), 0);
    lv_obj_set_style_outline_width(piece, 2, 0);
    if (placed_[0] && placed_[1]) {
        // The complete picture has no internal success border covering it.
        for (auto part : pieces_)
            lv_obj_set_style_outline_width(part, 0, 0);
        lv_obj_set_style_outline_color(ghost_, lv_color_hex(0x58965B), 0);
        lv_obj_add_flag(preview_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(title_, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(title_, "找到了");
        lv_obj_remove_flag(next_, LV_OBJ_FLAG_HIDDEN);
    }
}
void PhotoPuzzleGame::Event(lv_event_t* event) {
    auto self = static_cast<PhotoPuzzleGame*>(lv_event_get_user_data(event));
    auto target = static_cast<lv_obj_t*>(lv_event_get_target(event));
    auto code = lv_event_get_code(event);
    if (target == self->entry_ && code == LV_EVENT_SHORT_CLICKED) {
        Application::GetInstance().Schedule([self]() { self->Enter(); });
        return;
    }
    if (!self->visible_.load())
        return;
    if (target == self->next_ && code == LV_EVENT_SHORT_CLICKED) {
        Application::GetInstance().Schedule([self]() {
            DisplayLockGuard lock(self->display_);
            if (lock && self->visible_.load() && self->placed_[0] && self->placed_[1]) {
                ++self->round_;
                self->Round();
            }
        });
        return;
    }
    if (code == LV_EVENT_PRESSED) {
        auto input = lv_event_get_indev(event);
        if (!input)
            return;
        for (int i = 0; i < 2; ++i) {
            if (target != self->pieces_[i] || self->placed_[i])
                continue;
            self->drag_ = i;
            lv_point_t point;
            lv_indev_get_point(input, &point);
            self->offset_ = {point.x - lv_obj_get_x(target), point.y - lv_obj_get_y(target)};
            lv_obj_move_foreground(target);
        }
    } else if (code == LV_EVENT_PRESSING) {
        self->Move(lv_event_get_indev(event));
    } else if (code == LV_EVENT_RELEASED) {
        self->Move(lv_event_get_indev(event));
        self->Release();
    } else if (code == LV_EVENT_PRESS_LOST) {
        self->drag_ = -1;
    }
}
#endif
