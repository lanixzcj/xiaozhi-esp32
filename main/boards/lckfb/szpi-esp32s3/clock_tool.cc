#include "clock_tool.h"
#include "home_entry_layout.h"
#ifndef CONFIG_USE_EMOTE_MESSAGE_STYLE
#include <cstdio>
#include "application.h"
LV_FONT_DECLARE(animal_game_font);
LV_FONT_DECLARE(clock_digits_font);
namespace {
lv_obj_t* Box(lv_obj_t* parent, int x, int y, int w, int h, uint32_t color) {
    auto obj = lv_obj_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, w, h);
    lv_obj_set_style_bg_color(obj, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    return obj;
}
lv_obj_t* Text(lv_obj_t* parent, const char* text) {
    auto obj = lv_label_create(parent);
    lv_label_set_text(obj, text);
    lv_obj_set_style_text_color(obj, lv_color_hex(0x41382F), 0);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_CLICKABLE);
    return obj;
}
lv_obj_t* Button(lv_obj_t* parent, const char* text, int x, int y) {
    auto obj = Box(parent, x, y, 72, 38, 0xE2DFC4);
    lv_obj_set_style_radius(obj, 12, 0);
    lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
    auto label = Text(obj, text);
    lv_obj_center(label);
    return obj;
}
}  // namespace
void ClockTool::Initialize() {
    if (entry_ || !display_ || !display_->IsSetupUICalled())
        return;
    DisplayLockGuard lock(display_);
    if (!lock || !lv_layer_top())
        return;
    entry_ = Button(lv_layer_top(), "时钟", 240, 4);
    lv_obj_set_style_text_font(entry_, &animal_game_font, 0);
    panel_ = Box(lv_layer_top(), 0, 0, 320, 240, 0xFFF8EA);
    lv_obj_set_style_text_font(panel_, &animal_game_font, 0);
    auto title = Text(panel_, "时间");
    lv_obj_set_pos(title, 12, 12);
    auto back = Button(panel_, "返回", 236, 4);
    time_ = Text(panel_, "--:--:--");
    lv_obj_set_style_text_font(time_, &clock_digits_font, 0);
    lv_obj_set_width(time_, 304);
    lv_obj_set_pos(time_, 8, 83);
    lv_obj_set_style_text_align(time_, LV_TEXT_ALIGN_CENTER, 0);
    date_ = Text(panel_, "");
    lv_obj_set_style_text_font(date_, LV_FONT_DEFAULT, 0);
    lv_obj_set_width(date_, 304);
    lv_obj_set_pos(date_, 8, 158);
    lv_obj_set_style_text_align(date_, LV_TEXT_ALIGN_CENTER, 0);
    waiting_ = Text(panel_, "等待校时");
    lv_obj_set_width(waiting_, 304);
    lv_obj_set_pos(waiting_, 8, 154);
    lv_obj_set_style_text_align(waiting_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_add_flag(panel_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(
        entry_,
        [](lv_event_t* e) {
            auto self = static_cast<ClockTool*>(lv_event_get_user_data(e));
            Application::GetInstance().Schedule([self]() { self->Enter(); });
        },
        LV_EVENT_SHORT_CLICKED, this);
    lv_obj_add_event_cb(
        back,
        [](lv_event_t* e) { static_cast<ClockTool*>(lv_event_get_user_data(e))->RequestExit(); },
        LV_EVENT_CLICKED, this);
    Tick();
    lv_timer_create(
        [](lv_timer_t* timer) { static_cast<ClockTool*>(lv_timer_get_user_data(timer))->Tick(); },
        200, this);
}
void ClockTool::Tick() {
    PositionHomeEntry(entry_, display_, 4);
    if (!visible_.load())
        return;
    const time_t now = now_ ? now_() : std::time(nullptr);
    if (now == displayed_)
        return;
    displayed_ = now;
    std::tm parts{};
    if (!localtime_r(&now, &parts) || parts.tm_year < 2025 - 1900) {
        lv_label_set_text(time_, "--:--:--");
        lv_obj_add_flag(date_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(waiting_, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    char text[16]{};
    std::strftime(text, sizeof(text), "%H:%M:%S", &parts);
    lv_label_set_text(time_, text);
    char date[16]{};
    std::strftime(date, sizeof(date), "%Y-%m-%d", &parts);
    lv_label_set_text(date_, date);
    lv_obj_remove_flag(date_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(waiting_, LV_OBJ_FLAG_HIDDEN);
}
void ClockTool::Enter() {
    if (visible_.load() || Application::GetInstance().IsLocalInteractionActive())
        return;
    DisplayLockGuard lock(display_);
    if (!lock)
        return;
    // No local-game session: clock viewing leaves network time sync and ordinary
    // conversation running. It only covers the chat visually until return.
    visible_.store(true);
    displayed_ = -1;
    lv_obj_add_flag(entry_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(panel_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(panel_);
    Tick();
}
void ClockTool::RequestExit() {
    Application::GetInstance().Schedule([this]() { Exit(); });
}
void ClockTool::Exit() {
    DisplayLockGuard lock(display_);
    if (!lock || !visible_.load())
        return;
    visible_.store(false);
    lv_obj_add_flag(panel_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(entry_, LV_OBJ_FLAG_HIDDEN);
}
#endif
