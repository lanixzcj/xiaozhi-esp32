#pragma once
#include "sdkconfig.h"
#ifndef CONFIG_USE_EMOTE_MESSAGE_STYLE
#include <lvgl.h>
#include <cstddef>
// Original flat vector art; MIT. See assets/puzzle/README.md.
namespace puzzle_game_assets {
inline constexpr size_t kCount = 3;
const lv_image_dsc_t* GetImage(size_t index);
const lv_image_dsc_t* GetPreview(size_t index);
}  // namespace puzzle_game_assets
#endif
