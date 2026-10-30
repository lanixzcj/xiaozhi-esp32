#include <cassert>
#include <cstdio>
#include <string>

#include "accel_source.h"
#include "application.h"
#include "dot_tap_game.h"
#include "dot_tap_sounds.h"

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

void Hold(lv_obj_t* object, unsigned duration) {
    lv_obj_send_event(object, LV_EVENT_PRESSED, nullptr);
    Advance(duration);
    lv_obj_send_event(object, LV_EVENT_RELEASED, nullptr);
}

bool Hidden(lv_obj_t* object) { return lv_obj_has_flag(object, LV_OBJ_FLAG_HIDDEN); }

bool ColorIs(lv_obj_t* object, uint32_t hex) {
    return lv_color_eq(lv_obj_get_style_bg_color(object, LV_PART_MAIN), lv_color_hex(hex));
}

class ScriptedAccel : public AccelSource {
public:
    bool available() const override { return on; }
    bool ReadAccel(int16_t& x, int16_t& y, int16_t& z) override {
        x = ax;
        y = ay;
        z = az;
        return ok;
    }
    bool on = false;
    bool ok = true;
    int16_t ax = 0;
    int16_t ay = 0;
    int16_t az = 8192;
};

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
    ScriptedAccel accel;
    DotTapGame game(&display, &accel);
    auto chat = lv_screen_active();
    game.Initialize();
    auto layer = lv_layer_top();
    auto entry = lv_obj_get_child(layer, 0);
    auto panel = lv_obj_get_child(layer, 1);
    lv_obj_t* dots[5];
    for (int index = 0; index < 5; ++index) {
        dots[index] = lv_obj_get_child(panel, index);
    }
    auto rest = lv_obj_get_child(panel, 5);
    auto restart = lv_obj_get_child(rest, 2);
    auto& app = Application::GetInstance();

    app.active = true;
    Click(entry);
    assert(!game.IsVisible());
    app.active = false;

    Click(entry);
    assert(game.IsVisible());
    assert(lv_screen_active() == chat);
    assert(game.page() == DotTapSession::Page::One);
    assert(app.sounds == 0);
    for (int index = 0; index < 5; ++index) {
        assert(Hidden(dots[index]) == (index != 2));
    }
    assert(ColorIs(dots[2], 0xF5C400));
    lv_obj_update_layout(panel);
    assert(lv_obj_get_width(dots[2]) >= 50 && lv_obj_get_width(dots[2]) <= 70);
    Snapshot("page1-one");

    Click(dots[2]);
    assert(game.page() == DotTapSession::Page::Five);
    assert(app.last == dot_tap_sounds::Get(dot_tap_sounds::Cue::Yellow));
    app.audio.busy = false;
    Advance(300);
    for (auto* dot : dots) {
        assert(!Hidden(dot));
        assert(ColorIs(dot, 0xF5C400));
    }
    Snapshot("page2-five");

    Click(dots[0]);
    assert(game.page() == DotTapSession::Page::Mixed);
    app.audio.busy = false;
    Advance(300);
    assert(ColorIs(dots[0], 0xF5C400));
    assert(ColorIs(dots[1], 0xF5C400));
    assert(ColorIs(dots[2], 0xE23B3B));
    assert(ColorIs(dots[3], 0xE23B3B));
    assert(ColorIs(dots[4], 0x2E7BD6));
    Snapshot("page3-mixed");

    const auto sounds_before = app.sounds;
    Click(dots[4]);
    assert(game.page() == DotTapSession::Page::Mixed);
    assert(app.sounds == sounds_before + 1);
    assert(app.last == dot_tap_sounds::Get(dot_tap_sounds::Cue::Blue));
    app.audio.busy = false;
    Click(dots[2]);
    assert(app.last == dot_tap_sounds::Get(dot_tap_sounds::Cue::Red));
    app.audio.busy = false;

    Hold(panel, 200);
    assert(game.page() == DotTapSession::Page::Mixed);
    lv_obj_send_event(panel, LV_EVENT_PRESSED, nullptr);
    while (game.page() == DotTapSession::Page::Mixed) {
        Advance(50);
    }
    assert(game.page() == DotTapSession::Page::Scattering);
    assert(app.last == dot_tap_sounds::Get(dot_tap_sounds::Cue::Scatter));
    lv_obj_send_event(panel, LV_EVENT_RELEASED, nullptr);
    Click(panel);  // the hold's release is not a tap
    assert(game.page() == DotTapSession::Page::Scattering);
    Advance(400);
    assert(game.page() == DotTapSession::Page::Scattering);
    lv_obj_update_layout(panel);
    assert(lv_obj_get_x(dots[0]) < 0);
    Snapshot("page4-scattered");
    app.audio.busy = false;
    while (game.page() != DotTapSession::Page::Settled) {
        Advance(50);
    }
    lv_obj_update_layout(panel);
    assert(ColorIs(dots[0], 0x2E7BD6));
    assert(ColorIs(dots[1], 0xE23B3B));
    assert(ColorIs(dots[2], 0xF5C400));
    assert(ColorIs(dots[3], 0xE23B3B));
    assert(ColorIs(dots[4], 0x2E7BD6));
    assert(lv_obj_get_x(dots[2]) == 128);
    assert(app.last == dot_tap_sounds::Get(dot_tap_sounds::Cue::Settle));
    Snapshot("page5-cross");

    app.audio.busy = false;
    Click(panel);
    assert(game.page() == DotTapSession::Page::One);
    Advance(DotTapSession::kLimitMs);
    assert(game.page() == DotTapSession::Page::Rest);
    assert(!Hidden(rest));
    assert(std::string(lv_label_get_text(lv_obj_get_child(rest, 0))) == "休息一下吧");
    Snapshot("page-rest");
    Hold(restart, 1400);
    assert(game.page() == DotTapSession::Page::Rest);
    Hold(restart, 1500);
    assert(game.page() == DotTapSession::Page::One);
    assert(Hidden(rest));

    game.RequestExit();
    assert(!game.IsVisible());
    assert(lv_screen_active() == chat);
    assert(!app.active);

    accel.on = true;
    Click(entry);
    assert(game.IsVisible());
    Click(dots[2]);
    app.audio.busy = false;
    Click(dots[0]);
    app.audio.busy = false;
    assert(game.page() == DotTapSession::Page::Mixed);
    Advance(50);  // learn the resting baseline
    accel.az = 8192 + 6000;
    Advance(50);
    assert(game.page() == DotTapSession::Page::Mixed);
    Advance(50);
    assert(game.page() == DotTapSession::Page::Scattering);
    game.RequestExit();
    assert(!game.IsVisible());

    lv_mem_monitor_t before{}, after{};
    lv_mem_monitor(&before);
    accel.on = false;
    for (int n = 0; n < 20; ++n) {
        Click(entry);
        game.RequestExit();
        Advance(50);
    }
    lv_mem_monitor(&after);
    assert(after.free_size >= before.free_size);
    assert(lv_mem_test() == LV_RESULT_OK);
    assert(lv_obj_get_child_count(layer) == 2);
    assert(lv_obj_get_child_count(panel) == 6);
    assert(lv_screen_active() == chat);
    printf("dot-tap UI checks passed, free %zu -> %zu bytes\n", before.free_size, after.free_size);
}
