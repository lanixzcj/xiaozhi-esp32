#include "animal_touch_game.h"

#ifndef CONFIG_USE_EMOTE_MESSAGE_STYLE

#include "animal_game_photos.h"
#include "animal_game_sounds.h"

static_assert(animal_game_assets::kSoundCount == AnimalGameSession::kCount);

namespace {
const lv_image_dsc_t* AnimalPhoto(size_t index) { return animal_game_photo(index); }
}  // namespace

AnimalTouchGame::AnimalTouchGame(Display* display)
    : session_(),
      ui_(display, session_, "动物", 160, AnimalPhoto, animal_game_assets::GetSound,
          "和爸爸妈妈说说小动物") {}

#endif  // !CONFIG_USE_EMOTE_MESSAGE_STYLE
