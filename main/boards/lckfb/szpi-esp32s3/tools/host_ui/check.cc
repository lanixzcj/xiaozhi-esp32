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
    auto photo = lv_obj_get_child(card, 0);
    auto mode = lv_obj_get_child(panel, 4);
    auto rest = lv_obj_get_child(panel, 5);
    auto restart = lv_obj_get_child(rest, 2);
    auto& app = Application::GetInstance();
    const char* names[] = {"cat", "dog", "duck", "chicken", "cow", "sheep"};
    auto hold = [](lv_obj_t* button, unsigned duration) {
        lv_obj_send_event(button, LV_EVENT_PRESSED, nullptr);
        Advance(duration);
        lv_obj_send_event(button, LV_EVENT_RELEASED, nullptr);
    };
    assert(!lv_obj_has_flag(photo, LV_OBJ_FLAG_HIDDEN));  // photos default
    hold(mode, 1400);
    assert(!lv_obj_has_flag(photo, LV_OBJ_FLAG_HIDDEN));
    for (int i = 0; i < 6; i++) {
        Snapshot(names[i]);
        hold(mode, 1500);
        assert(lv_obj_has_flag(photo, LV_OBJ_FLAG_HIDDEN));
        char cartoon[64];
        snprintf(cartoon, sizeof(cartoon), "%s-cartoon", names[i]);
        Snapshot(cartoon);
        hold(mode, 1500);
        assert(!lv_obj_has_flag(photo, LV_OBJ_FLAG_HIDDEN));
        assert(app.sounds == i);  // changing display mode never plays
        lv_obj_send_event(card, LV_EVENT_PRESSED, nullptr);
        lv_obj_send_event(card, LV_EVENT_LONG_PRESSED_REPEAT, nullptr);
        assert(app.sounds == i);  // hold/repeat never play
        lv_obj_send_event(card, LV_EVENT_SHORT_CLICKED, nullptr);
        assert(app.sounds == i + 1);
        for (int n = 0; n < 10; n++)
            lv_obj_send_event(card, LV_EVENT_SHORT_CLICKED, nullptr);
        assert(app.sounds == i + 1);
        Advance(2700);
        assert(app.sounds == i + 1);  // busy can't advance or replay
        app.audio.busy = false;
        Advance(100);
        if (i == 2 || i == 5) {
            assert(!lv_obj_has_flag(rest, LV_OBJ_FLAG_HIDDEN));
            Snapshot(i == 2 ? "rest-first-group" : "rest");
            Advance(100000);
            assert(app.sounds == i + 1);
            hold(restart, 1400);
            assert(!lv_obj_has_flag(rest, LV_OBJ_FLAG_HIDDEN));
            hold(restart, 1500);
            assert(lv_obj_has_flag(rest, LV_OBJ_FLAG_HIDDEN));
        }
    }
    // Restart cycles deterministically back to the first group.
    assert(std::string(lv_label_get_text(lv_obj_get_child(panel, 0))) == "小猫");
    game.RequestExit();
    assert(!game.IsVisible());
    assert(lv_screen_active() == chat);
    assert(!app.active);
    // All six descriptors and drawing paths have been warmed above. Mode/resource
    // changes and repeated entry/exit must not grow the LVGL heap or object tree.
    lv_mem_monitor_t before{}, after{};
    lv_mem_monitor(&before);
    for (int n = 0; n < 300; n++) {
        lv_obj_send_event(entry, LV_EVENT_SHORT_CLICKED, nullptr);
        for (int animal = 0; animal < 6; ++animal) {
            hold(mode, 1500);
            hold(mode, 1500);
            lv_obj_send_event(card, LV_EVENT_SHORT_CLICKED, nullptr);
            app.audio.busy = false;
            Advance(2800);
            if (animal == 2)
                hold(restart, 1500);
        }
        lv_obj_invalidate(chat);  // unrelated screen/resource repaint
        Advance(100);
        game.RequestExit();
        Advance(100);
    }
    lv_mem_monitor(&after);
    assert(after.free_size >= before.free_size);
    assert(lv_mem_test() == LV_RESULT_OK);
    assert(lv_obj_get_child_count(layer) == 2);
    assert(lv_obj_get_child_count(panel) == 6);
    assert(lv_screen_active() == chat);
    // Main-task queue coalescing and a stale callback after exit.
    app.deferred = true;
    lv_obj_send_event(entry, LV_EVENT_SHORT_CLICKED, nullptr);
    assert(!game.IsVisible());
    app.Flush();
    assert(game.IsVisible());
    auto sounds_before = app.sounds;
    for (int i = 0; i < 1000; ++i)
        lv_obj_send_event(card, LV_EVENT_SHORT_CLICKED, nullptr);
    assert(app.pending.size() == 1);
    app.Flush();
    assert(app.sounds == sounds_before + 1);
    // Regression: Poll is queued while the cat is busy, then the child taps
    // that still-listening card, then audio drains before the main queue runs.
    // The busy tap must never turn into a new-animal tap after Poll advances.
    Advance(2800);
    lv_obj_send_event(card, LV_EVENT_SHORT_CLICKED, nullptr);
    assert(app.pending.size() == 1);  // only Poll; the busy tap was discarded
    app.audio.busy = false;
    app.Flush();
    assert(app.sounds == sounds_before + 1);
    assert(std::string(lv_label_get_text(lv_obj_get_child(panel, 0))) == "小狗");
    lv_obj_send_event(card, LV_EVENT_SHORT_CLICKED, nullptr);  // genuinely fresh tap
    app.Flush();
    assert(app.sounds == sounds_before + 2);
    for (int i = 0; i < 100; ++i)
        Advance(100);
    assert(app.pending.size() == 1);
    app.audio.busy = false;
    app.Flush();  // third animal is Ready
    game.RequestExit();
    lv_obj_send_event(entry, LV_EVENT_SHORT_CLICKED, nullptr);  // a later reopen
    lv_obj_send_event(card, LV_EVENT_SHORT_CLICKED, nullptr);   // captures old session
    app.Flush();
    assert(game.IsVisible() && app.active && app.sounds == sounds_before + 2);
    assert(std::string(lv_label_get_text(lv_obj_get_child(panel, 0))) == "小猫");
    game.RequestExit();
    app.Flush();
    assert(!game.IsVisible() && !app.active);
    app.deferred = false;
    printf(
        "LVGL UI checks passed: six photos/cartoons, two three-round groups, 300 cycles, deferred "
        "queue guards, free %zu "
        "-> %zu bytes\n",
        before.free_size, after.free_size);
}
