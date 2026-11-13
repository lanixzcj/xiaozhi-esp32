#include <cassert>
#include <cstdio>

#include "animal_touch_game.h"
#include "dot_tap_game.h"
#include "photo_puzzle_game.h"
#include "transport_touch_game.h"

int main(int argc, char** argv) {
    lv_init();
    auto driver = lv_display_create(320, 240);
    static uint32_t pixels[320 * 240];
    lv_display_set_buffers(driver, pixels, nullptr, sizeof(pixels), LV_DISPLAY_RENDER_MODE_FULL);
    lv_display_set_flush_cb(driver, [](lv_display_t* display, const lv_area_t*, uint8_t*) {
        lv_display_flush_ready(display);
    });
    Display display;
    AnimalTouchGame animal(&display);
    TransportTouchGame transport(&display);
    DotTapGame dot(&display, nullptr);
    PhotoPuzzleGame puzzle(&display);
    animal.Initialize();
    transport.Initialize();
    dot.Initialize();
    puzzle.Initialize();
    auto layer = lv_layer_top();
    lv_obj_update_layout(layer);
    for (int row = 0; row < 4; ++row) {
        auto entry = lv_obj_get_child(layer, row * 2);
        assert(lv_obj_get_x(entry) == 240);
        assert(lv_obj_get_y(entry) == display.GetTopBarBottom() + 4 + row * 46);
        assert(lv_obj_get_width(entry) == 72 && lv_obj_get_height(entry) == 38);
        assert(lv_obj_has_flag(lv_obj_get_child(layer, row * 2 + 1), LV_OBJ_FLAG_HIDDEN));
    }
    auto image = lv_snapshot_take(layer, LV_COLOR_FORMAT_RGB888);
    assert(image);
    const char* target = argc > 1 ? argv[1] : "right-menu.ppm";
    FILE* file = fopen(target, "wb");
    assert(file);
    fprintf(file, "P6\n%u %u\n255\n", image->header.w, image->header.h);
    for (unsigned y = 0; y < image->header.h; ++y) {
        const auto row = image->data + y * image->header.stride;
        for (unsigned x = 0; x < image->header.w; ++x) {
            fputc(row[x * 3 + 2], file);
            fputc(row[x * 3 + 1], file);
            fputc(row[x * 3], file);
        }
    }
    fclose(file);
    lv_draw_buf_destroy(image);
    puts("Right-side menu geometry checks passed.");
}
