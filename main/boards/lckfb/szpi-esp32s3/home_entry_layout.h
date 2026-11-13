#pragma once

#include <algorithm>

#include "display.h"
#include "lvgl.h"

// This board has a 320 x 240 display. Keep the bottom 40 pixels free for
// XiaoZhi replies, including the theme font and subtitle padding.
inline void PositionHomeEntry(lv_obj_t* entry, Display* display, int row) {
    if (!entry)
        return;
    const int top = display->GetTopBarBottom() + 4;
    const int height = std::min(32, (200 - top - 4 * 3) / 5);
    lv_obj_set_height(entry, height);
    lv_obj_set_y(entry, top + row * (height + 3));
}
