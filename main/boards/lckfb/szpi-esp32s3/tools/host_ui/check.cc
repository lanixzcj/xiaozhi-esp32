#include <cassert>
#include <cstdio>
#include <vector>
#include "animal_touch_game.h"
#include "application.h"

static const char* output_dir = ".";

void Snapshot(const char* name) {
    lv_obj_update_layout(lv_layer_top());
    auto image = lv_snapshot_take(lv_layer_top(), LV_COLOR_FORMAT_RGB888);
    assert(image);
    char path[256];
    snprintf(path, sizeof(path), "%s/%s.ppm", output_dir, name);
    FILE* f = fopen(path, "wb");
    assert(f);
    fprintf(f, "P6\n%u %u\n255\n", image->header.w, image->header.h);
    for (unsigned y = 0; y < image->header.h; y++) {
        auto row = image->data + y * image->header.stride;
        for (unsigned x = 0; x < image->header.w; x++) {
            fputc(row[x * 3 + 2], f);
            fputc(row[x * 3 + 1], f);
            fputc(row[x * 3], f);
        }
    }
    fclose(f);
    lv_draw_buf_destroy(image);
}
void Advance(unsigned ms) {
    lv_tick_inc(ms);
    lv_timer_handler();
}
int main(int argc, char** argv) {
    if (argc > 1)
        output_dir = argv[1];
    lv_init();
    auto disp = lv_display_create(320, 240);
    static uint32_t pixels[320 * 240];
    lv_display_set_buffers(disp, pixels, nullptr, sizeof(pixels), LV_DISPLAY_RENDER_MODE_FULL);
    lv_display_set_flush_cb(
        disp, [](lv_display_t* d, const lv_area_t*, uint8_t*) { lv_display_flush_ready(d); });
    Display display;
    AnimalTouchGame game(&display);
    auto chat = lv_screen_active();
    game.Initialize();
    auto layer = lv_layer_top();
    auto entry = lv_obj_get_child(layer, 0);
    auto panel = lv_obj_get_child(layer, 1);
    lv_obj_send_event(entry, LV_EVENT_SHORT_CLICKED, nullptr);
    assert(game.IsVisible());
    assert(lv_screen_active() == chat);
    auto card = lv_obj_get_child(panel, 2);
    auto rest = lv_obj_get_child(panel, 4);
    auto restart = lv_obj_get_child(rest, 2);
    auto& app = Application::GetInstance();
    const char* names[] = {"cat", "dog", "duck"};
    for (int i = 0; i < 3; i++) {
        Snapshot(names[i]);
        lv_obj_send_event(card, LV_EVENT_PRESSED, nullptr);
        lv_obj_send_event(card, LV_EVENT_LONG_PRESSED_REPEAT, nullptr);
        assert(app.sounds == i);  // hold/repeat never play
        lv_obj_send_event(card, LV_EVENT_SHORT_CLICKED, nullptr);
        assert(app.sounds == i + 1);
        for (int n = 0; n < 10; n++)
            lv_obj_send_event(card, LV_EVENT_SHORT_CLICKED, nullptr);
        assert(app.sounds == i + 1);
        Advance(1000);
        assert(app.sounds == i + 1);  // busy can't advance
        app.audio.busy = false;
        Advance(100);
    }
    assert(!lv_obj_has_flag(rest, LV_OBJ_FLAG_HIDDEN));
    Snapshot("rest");
    Advance(100000);
    assert(app.sounds == 3);
    lv_obj_send_event(restart, LV_EVENT_PRESSED, nullptr);
    Advance(1400);
    lv_obj_send_event(restart, LV_EVENT_RELEASED, nullptr);
    assert(!lv_obj_has_flag(rest, LV_OBJ_FLAG_HIDDEN));
    lv_obj_send_event(restart, LV_EVENT_PRESSED, nullptr);
    Advance(1500);
    lv_obj_send_event(restart, LV_EVENT_RELEASED, nullptr);
    assert(lv_obj_has_flag(rest, LV_OBJ_FLAG_HIDDEN));
    game.RequestExit();
    assert(!game.IsVisible());
    assert(lv_screen_active() == chat);
    assert(!app.active);
    for (int n = 0; n < 30; n++) {
        lv_obj_send_event(entry, LV_EVENT_SHORT_CLICKED, nullptr);
        game.RequestExit();
    }
    assert(lv_obj_get_child_count(layer) == 2);
    puts("LVGL UI checks passed");
}
