// SPDX-License-Identifier: MIT
#pragma once

#include "sdkconfig.h"

#ifndef CONFIG_USE_EMOTE_MESSAGE_STYLE

#include <lvgl.h>
#include <cstddef>

namespace animal_game_art {
// Draw the original farm-animal cartoon inside a 296 x 162 touch card.
// card_x/card_y are the card's absolute top-left; indices 3/4/5 are hen/cow/sheep.
// Unknown indices are ignored. The caller owns the LVGL display lock.
void DrawFarmAnimal(lv_layer_t* layer, int card_x, int card_y, size_t animal_index);
}  // namespace animal_game_art

#endif  // !CONFIG_USE_EMOTE_MESSAGE_STYLE
