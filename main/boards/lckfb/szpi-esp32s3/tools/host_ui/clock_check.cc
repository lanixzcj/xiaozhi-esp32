#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <string>
#include "application.h"
#include "clock_tool.h"
static time_t fake_time = 0;
static time_t Now() { return fake_time; }
static lv_point_t finger{272, 24};
static lv_indev_state_t finger_state = LV_INDEV_STATE_RELEASED;
void Advance(unsigned ms) {
    lv_tick_inc(ms);
    lv_timer_handler();
}
void Click(lv_obj_t* obj) { lv_obj_send_event(obj, LV_EVENT_SHORT_CLICKED, nullptr); }
void Snapshot(const char* path) {
    auto image = lv_snapshot_take(lv_layer_top(), LV_COLOR_FORMAT_RGB888);
    assert(image);
    auto file = fopen(path, "wb");
    assert(file);
    fprintf(file, "P6\n%u %u\n255\n", image->header.w, image->header.h);
    for (unsigned y = 0; y < image->header.h; ++y)
        for (unsigned x = 0; x < image->header.w; ++x) {
            auto p = image->data + y * image->header.stride + x * 3;
            fputc(p[2], file);
            fputc(p[1], file);
            fputc(p[0], file);
        }
    fclose(file);
    lv_draw_buf_destroy(image);
}
int main(int argc, char** argv) {
    setenv("TZ", "UTC", 1);
    tzset();
    lv_init();
    auto driver = lv_display_create(320, 240);
    static uint32_t pixels[320 * 240];
    lv_display_set_buffers(driver, pixels, nullptr, sizeof(pixels), LV_DISPLAY_RENDER_MODE_FULL);
    lv_display_set_flush_cb(
        driver, [](lv_display_t* d, const lv_area_t*, uint8_t*) { lv_display_flush_ready(d); });
    Display display;
    ClockTool clock(&display, Now);
    clock.Initialize();
    auto& app = Application::GetInstance();
    auto layer = lv_layer_top(), entry = lv_obj_get_child(layer, 0),
         panel = lv_obj_get_child(layer, 1);
    auto digits = lv_obj_get_child(panel, 2), date = lv_obj_get_child(panel, 3),
         waiting = lv_obj_get_child(panel, 4);
    app.active = true;
    Click(entry);
    assert(!clock.IsVisible());
    app.active = false;
    Click(entry);
    Advance(200);
    assert(clock.IsVisible() && !app.active);
    assert(std::string(lv_label_get_text(digits)) == "--:--:--");
    assert(!lv_obj_has_flag(waiting, LV_OBJ_FLAG_HIDDEN));
    fake_time = 1767225599;
    Advance(200);
    assert(std::string(lv_label_get_text(digits)) == "23:59:59");
    assert(std::string(lv_label_get_text(date)) == "2025-12-31");
    ++fake_time;
    Advance(1000);
    assert(std::string(lv_label_get_text(digits)) == "00:00:00");
    assert(std::string(lv_label_get_text(date)) == "2026-01-01");
    for (int i = 1; i <= 60; ++i) {
        ++fake_time;
        Advance(1000);
    }
    assert(std::string(lv_label_get_text(digits)) == "00:01:00");
    // A network correction jumps directly to current wall time, with no local counter drift.
    fake_time = 1794614400 + 12 * 3600 + 34 * 60 + 56;
    Advance(200);
    assert(std::string(lv_label_get_text(digits)) == "12:34:56");
    assert(lv_obj_has_flag(waiting, LV_OBJ_FLAG_HIDDEN));
    lv_obj_update_layout(layer);
    if (argc > 1)
        Snapshot(argv[1]);
    auto input = lv_indev_create();
    lv_indev_set_type(input, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(input, [](lv_indev_t*, lv_indev_data_t* d) {
        d->point = finger;
        d->state = finger_state;
    });
    finger_state = LV_INDEV_STATE_PRESSED;
    lv_indev_read(input);
    Advance(20);
    finger_state = LV_INDEV_STATE_RELEASED;
    lv_indev_read(input);
    Advance(40);
    assert(!clock.IsVisible() && !app.active && !lv_obj_has_flag(entry, LV_OBJ_FLAG_HIDDEN));
    lv_indev_delete(input);
    lv_mem_monitor_t before{}, after{};
    lv_mem_monitor(&before);
    for (int i = 0; i < 20; ++i) {
        Click(entry);
        ++fake_time;
        Advance(1000);
        clock.RequestExit();
    }
    lv_mem_monitor(&after);
    assert(after.free_size >= before.free_size);
    assert(lv_mem_test() == LV_RESULT_OK);
    puts(
        "clock: unsynced state, per-second tick, midnight/date rollover, wall-time correction, "
        "pointer return and heap checks passed");
}
