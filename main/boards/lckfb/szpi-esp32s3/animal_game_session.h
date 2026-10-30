#pragma once

#include "listen_pick_session.h"

// Cat, dog, duck, hen, cow, sheep. One session asks for each animal once.
class AnimalGameSession : public ListenPickSession {
public:
    static constexpr size_t kCount = 6;
    AnimalGameSession() : ListenPickSession(kCount) {}
};
