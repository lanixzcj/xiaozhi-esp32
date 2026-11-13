#include <cassert>
#include <cstdio>
#include <string>
#include "animal_game_photos.h"
#include "application.h"
#include "photo_puzzle_game.h"
LV_FONT_DECLARE(animal_game_font);
static lv_point_t finger{0, 0};
static lv_indev_state_t state = LV_INDEV_STATE_RELEASED;
void Advance(unsigned ms) {
    lv_tick_inc(ms);
    lv_timer_handler();
}
void Click(lv_obj_t* obj) { lv_obj_send_event(obj, LV_EVENT_SHORT_CLICKED, nullptr); }
void Drag(lv_indev_t* input, lv_obj_t* piece, int x, int y) {
    lv_obj_update_layout(lv_layer_top());
    const int sx = lv_obj_get_x(piece), sy = lv_obj_get_y(piece);
    finger = {sx + 74, sy + 81};
    state = LV_INDEV_STATE_PRESSED;
    lv_indev_read(input);
    Advance(20);
    for (int i = 1; i <= 8; ++i) {
        finger = {sx + 74 + (x - sx) * i / 8, sy + 81 + (y - sy) * i / 8};
        lv_indev_read(input);
        Advance(20);
    }
    state = LV_INDEV_STATE_RELEASED;
    lv_indev_read(input);
    Advance(40);
    lv_obj_update_layout(lv_layer_top());
}
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
    lv_init();
    for (uint32_t glyph : {U'拼', U'图', U'拖'}) {
        lv_font_glyph_dsc_t dsc{};
        assert(lv_font_get_glyph_dsc(&animal_game_font, &dsc, glyph, 0));
    }
    auto driver = lv_display_create(320, 240);
    static uint32_t pixels[320 * 240];
    lv_display_set_buffers(driver, pixels, nullptr, sizeof(pixels), LV_DISPLAY_RENDER_MODE_FULL);
    lv_display_set_flush_cb(
        driver, [](lv_display_t* d, const lv_area_t*, uint8_t*) { lv_display_flush_ready(d); });
    Display display;
    PhotoPuzzleGame game(&display);
    game.Initialize();
    auto& app = Application::GetInstance();
    auto layer = lv_layer_top(), entry = lv_obj_get_child(layer, 0),
         panel = lv_obj_get_child(layer, 1);
    auto ghost = lv_obj_get_child(panel, 0), title = lv_obj_get_child(panel, 1);
    auto a = lv_obj_get_child(panel, 2), b = lv_obj_get_child(panel, 3),
         next = lv_obj_get_child(panel, 5);
    app.active = true;
    Click(entry);
    assert(!game.IsVisible());
    app.active = false;
    Click(entry);
    Advance(40);
    lv_obj_update_layout(layer);
    assert(game.IsVisible() && app.active);
    assert(lv_image_get_src(ghost) == animal_game_photo(2));
    assert(!lv_obj_has_flag(a, LV_OBJ_FLAG_OVERFLOW_VISIBLE));
    assert(lv_obj_get_width(a) == 148 && lv_obj_get_height(a) == 162);
    if (argc > 1)
        Snapshot(argv[1]);
    auto input = lv_indev_create();
    lv_indev_set_type(input, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(input, [](lv_indev_t*, lv_indev_data_t* d) {
        d->point = finger;
        d->state = state;
    });
    Drag(input, a, 170, 0);
    assert(lv_obj_has_flag(a, LV_OBJ_FLAG_CLICKABLE));
    assert(lv_obj_has_flag(next, LV_OBJ_FLAG_HIDDEN));
    Click(next);
    assert(lv_image_get_src(ghost) == animal_game_photo(2));
    Drag(input, a, 30, 58);
    assert(lv_obj_get_x(a) == 12 && lv_obj_get_y(a) == 58);
    assert(!lv_obj_has_flag(a, LV_OBJ_FLAG_CLICKABLE));
    // The second, unplaced half must remain reachable after being covered.
    Drag(input, b, 155, 50);
    assert(lv_obj_get_x(b) == 160 && lv_obj_get_y(b) == 58);
    assert(!lv_obj_has_flag(b, LV_OBJ_FLAG_CLICKABLE));
    assert(std::string(lv_label_get_text(title)) == "找到了");
    assert(!lv_obj_has_flag(next, LV_OBJ_FLAG_HIDDEN));
    if (argc > 2)
        Snapshot(argv[2]);
    Advance(10000);
    assert(lv_image_get_src(ghost) == animal_game_photo(2));
    Click(next);
    Advance(40);
    assert(lv_image_get_src(ghost) == animal_game_photo(4));
    assert(lv_obj_has_flag(a, LV_OBJ_FLAG_CLICKABLE) && lv_obj_has_flag(b, LV_OBJ_FLAG_CLICKABLE));
    app.EndLocalInteraction();
    Advance(500);
    assert(!game.IsVisible() && !app.active);
    lv_indev_delete(input);
    lv_mem_monitor_t before{}, after{};
    lv_mem_monitor(&before);
    for (int i = 0; i < 20; ++i) {
        Click(entry);
        game.RequestExit();
        Advance(50);
    }
    lv_mem_monitor(&after);
    assert(after.free_size >= before.free_size);
    assert(lv_mem_test() == LV_RESULT_OK);
    printf(
        "two-piece puzzle: pointer drag, broad snap, occlusion recovery, manual next, exit and "
        "heap checks passed\n");
}
