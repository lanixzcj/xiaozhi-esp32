#pragma once

#include "listen_pick_session.h"

// Car, train, bicycle, plane. One session asks for each vehicle once.
class TransportGameSession : public ListenPickSession {
public:
    static constexpr size_t kCount = 4;
    TransportGameSession() : ListenPickSession(kCount) {}
};
