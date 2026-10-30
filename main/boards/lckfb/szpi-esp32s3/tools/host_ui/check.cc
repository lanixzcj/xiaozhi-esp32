#include <array>
#include <cassert>
#include <cstdio>
#include <string>

#include "animal_game_photos.h"
#include "animal_game_sounds.h"
#include "animal_touch_game.h"
#include "application.h"
#include "listen_pick_cheer.h"

static const char* output_dir = ".";

void Snapshot(const char* name) {
    lv_obj_update_layout(lv_layer_top());
    auto image = lv_snapshot_take(lv_layer_top(), LV_COLOR_FORMAT_RGB888);
    assert(image);
    char path[256];
    snprintf(path, sizeof(path), "%s/%s.ppm", output_dir, name);
    FILE* file = fopen(path, "wb");
    assert(file);
    fprintf(file, "P6\n%u %u\n255\n", image->header.w, image->header.h);
    for (unsigned y = 0; y < image->header.h; y++) {
        auto row = image->data + y * image->header.stride;
        for (unsigned x = 0; x < image->header.w; x++) {
            fputc(row[x * 3 + 2], file);
            fputc(row[x * 3 + 1], file);
            fputc(row[x * 3], file);
        }
    }
    fclose(file);
    lv_draw_buf_destroy(image);
}

void Advance(unsigned ms) {
    lv_tick_inc(ms);
    lv_timer_handler();
}

void Click(lv_obj_t* object) { lv_obj_send_event(object, LV_EVENT_SHORT_CLICKED, nullptr); }

void Hold(lv_obj_t* button, unsigned duration) {
    lv_obj_send_event(button, LV_EVENT_PRESSED, nullptr);
    Advance(duration);
    lv_obj_send_event(button, LV_EVENT_RELEASED, nullptr);
}

size_t AnswerSide(size_t seed, size_t round) { return (seed + round) % 2; }

int main(int argc, char** argv) {
    if (argc > 1) {
        output_dir = argv[1];
    }
    lv_init();
    auto display_driver = lv_display_create(320, 240);
    static uint32_t pixels[320 * 240];
    lv_display_set_buffers(display_driver, pixels, nullptr, sizeof(pixels),
                           LV_DISPLAY_RENDER_MODE_FULL);
    lv_display_set_flush_cb(display_driver, [](lv_display_t* display, const lv_area_t*, uint8_t*) {
        lv_display_flush_ready(display);
    });
    Display display;
    AnimalTouchGame game(&display);
    auto chat = lv_screen_active();
    game.Initialize();
    auto layer = lv_layer_top();
    auto entry = lv_obj_get_child(layer, 0);
    auto panel = lv_obj_get_child(layer, 1);
    auto hint = lv_obj_get_child(panel, 0);
    auto cards = std::array<lv_obj_t*, 2>{lv_obj_get_child(panel, 2), lv_obj_get_child(panel, 3)};
    auto listen = lv_obj_get_child(panel, 4);
    auto rest = lv_obj_get_child(panel, 5);
    auto restart = lv_obj_get_child(rest, 2);
    auto& app = Application::GetInstance();

    auto& blocked = app;
    blocked.active = true;
    Click(entry);
    assert(!game.IsVisible());
    blocked.active = false;

    Click(entry);
    assert(game.IsVisible());
    assert(lv_screen_active() == chat);
    assert(app.sounds == 1);
    assert(app.last == animal_game_assets::GetSound(0));
    assert(std::string(lv_label_get_text(hint)) == "听一听…");
    assert(lv_image_get_src(lv_obj_get_child(cards[0], 0)) == animal_game_photo(0));
    Click(cards[0]);
    Click(cards[1]);
    Click(listen);
    lv_obj_send_event(cards[0], LV_EVENT_LONG_PRESSED_REPEAT, nullptr);
    assert(app.sounds == 1);  // playback ignores taps and does not queue them

    app.audio.busy = false;
    Advance(200);
    assert(app.sounds == 1);
    assert(std::string(lv_label_get_text(hint)) == "选一选");
    Snapshot("animal-choose");
    const auto before_replay = app.sounds;
    Click(listen);
    assert(app.sounds == before_replay + 1);
    assert(app.last == animal_game_assets::GetSound(0));
    app.audio.busy = false;
    Advance(200);

    // Wrong tap replays the same call and stays on this pair.
    Click(cards[1 - AnswerSide(0, 0)]);
    assert(app.sounds == before_replay + 2);
    assert(app.last == animal_game_assets::GetSound(0));
    assert(std::string(lv_label_get_text(hint)) == "再试试");
    Click(cards[AnswerSide(0, 0)]);
    assert(app.sounds == before_replay + 2);
    app.audio.busy = false;
    Advance(1000);
    assert(app.sounds == before_replay + 2);
    assert(lv_image_get_src(lv_obj_get_child(cards[0], 0)) == animal_game_photo(0));

    Click(cards[AnswerSide(0, 0)]);
    assert(app.last == listen_pick_cheer::Get());
    assert(std::string(lv_label_get_text(hint)) == "找到了");
    Snapshot("animal-correct");
    app.audio.busy = false;
    Advance(1000);
    assert(app.last == animal_game_assets::GetSound(1));

    for (size_t round = 1; round < 6; ++round) {
        app.audio.busy = false;
        Advance(200);
        Click(cards[AnswerSide(0, round)]);
        assert(app.last == listen_pick_cheer::Get());
        app.audio.busy = false;
        Advance(1000);
    }
    assert(!lv_obj_has_flag(rest, LV_OBJ_FLAG_HIDDEN));
    assert(std::string(lv_label_get_text(lv_obj_get_child(rest, 1))) == "和爸爸妈妈说说小动物");
    Snapshot("animal-rest");
    const auto sounds_at_rest = app.sounds;
    Advance(100000);
    assert(app.sounds == sounds_at_rest);
    Hold(restart, 1400);
    assert(!lv_obj_has_flag(rest, LV_OBJ_FLAG_HIDDEN));
    Hold(restart, 1500);
    assert(lv_obj_has_flag(rest, LV_OBJ_FLAG_HIDDEN));
    assert(app.sounds == sounds_at_rest + 1);
    assert(app.last == animal_game_assets::GetSound(1));  // seed advanced, first target is animal 1

    game.RequestExit();
    assert(!game.IsVisible());
    assert(lv_screen_active() == chat);
    assert(!app.active);

    lv_mem_monitor_t before{}, after{};
    lv_mem_monitor(&before);
    for (int n = 0; n < 40; ++n) {
        Click(entry);
        app.audio.busy = false;
        Advance(200);
        game.RequestExit();
        Advance(50);
    }
    lv_mem_monitor(&after);
    assert(after.free_size >= before.free_size);
    assert(lv_mem_test() == LV_RESULT_OK);
    assert(lv_obj_get_child_count(layer) == 2);
    assert(lv_obj_get_child_count(panel) == 6);
    assert(lv_screen_active() == chat);

    app.deferred = true;
    Click(entry);
    assert(!game.IsVisible());
    app.Flush();
    assert(game.IsVisible() && app.audio.busy);
    app.audio.busy = false;
    Advance(200);  // queues one Poll while deferred
    assert(app.pending.size() == 1);
    Click(cards[0]);
    Click(cards[1]);
    assert(app.pending.size() == 1);  // still Listening, so the taps were dropped
    app.Flush();
    assert(std::string(lv_label_get_text(hint)) == "选一选");
    const auto sounds_before = app.sounds;
    for (int i = 0; i < 1000; ++i) {
        Click(cards[0]);
    }
    assert(app.pending.size() == 1);
    app.Flush();
    assert(app.sounds == sounds_before + 1);

    app.audio.busy = false;
    game.RequestExit();
    Click(cards[0]);  // feedback is not Choosing, so this tap is not queued
    app.Flush();
    assert(!game.IsVisible() && !app.active);
    app.deferred = false;
    printf("LVGL listen-and-pick checks passed, free %zu -> %zu bytes\n", before.free_size,
           after.free_size);
}
