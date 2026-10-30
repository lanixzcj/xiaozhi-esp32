#pragma once

#include <cstdint>

// Non-owning accelerometer the dot game can poll. Hardware and host tests both
// implement this. A missing chip reports available() == false; it must not throw.
class AccelSource {
public:
    virtual ~AccelSource() = default;
    virtual bool available() const = 0;
    virtual bool ReadAccel(int16_t& x, int16_t& y, int16_t& z) = 0;
};
