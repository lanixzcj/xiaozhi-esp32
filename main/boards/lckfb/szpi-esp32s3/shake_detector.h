#pragma once

#include <cstdint>

// High-pass shake detector for a parent shaking the board with a toddler.
// Feed() expects raw QMI8658 counts at about 50 ms intervals.
//
// The dynamic signal is the magnitude of (sample - slow baseline), in milli-g,
// so both an up-down shake and a sideways shake count. Gravity and a slow tilt
// stay in the baseline and do not fire.
//
// Tuned for CTRL2 ±4g (8192 LSB/g) and a 50 ms poll. These are starting points
// for a real board, not a lab calibration:
//   * kDynamicMilliG (450 mg): about ten times the chip's noise, light enough
//     for a playful shake.
//   * Two high samples 20–280 ms apart reject a single spike.
//   * kCooldownMs (900 ms) so one jostle cannot retrigger.
//   * Baseline keeps 1/16 of each step (~0.8 s time constant at 50 ms).
class ShakeDetector {
public:
    static constexpr int kLsbPerG = 8192;
    static constexpr int kDynamicMilliG = 450;
    static constexpr int kBaselineShift = 4;
    static constexpr uint32_t kMinGapMs = 20;
    static constexpr uint32_t kConfirmWindowMs = 280;
    static constexpr uint32_t kCooldownMs = 900;

    void Reset() {
        ready_ = false;
        armed_ = false;
        fired_ = false;
    }

    bool Feed(int16_t x, int16_t y, int16_t z, uint32_t now_ms) {
        if (!ready_) {
            bx_ = x;
            by_ = y;
            bz_ = z;
            ready_ = true;
            return false;
        }
        const int32_t dx = int32_t(x) - bx_;
        const int32_t dy = int32_t(y) - by_;
        const int32_t dz = int32_t(z) - bz_;
        bx_ += TowardZero(dx);
        by_ += TowardZero(dy);
        bz_ += TowardZero(dz);

        const uint64_t mag2 = Square(dx) + Square(dy) + Square(dz);
        const int32_t milli_g = int32_t((Isqrt(mag2) * 1000) / uint32_t(kLsbPerG));
        if (milli_g < kDynamicMilliG) {
            if (armed_ && uint32_t(now_ms - armed_ms_) > kConfirmWindowMs) {
                armed_ = false;
            }
            return false;
        }
        if (!armed_) {
            armed_ = true;
            armed_ms_ = now_ms;
            return false;
        }
        const uint32_t gap = uint32_t(now_ms - armed_ms_);
        if (gap < kMinGapMs) {
            return false;
        }
        if (gap > kConfirmWindowMs) {
            armed_ms_ = now_ms;
            return false;
        }
        if (fired_ && uint32_t(now_ms - fired_ms_) < kCooldownMs) {
            armed_ = false;
            return false;
        }
        fired_ = true;
        fired_ms_ = now_ms;
        armed_ = false;
        return true;
    }

private:
    static int32_t TowardZero(int32_t delta) {
        if (delta >= 0) {
            return delta >> kBaselineShift;
        }
        return -((-delta) >> kBaselineShift);
    }

    static uint64_t Square(int32_t value) {
        const int64_t wide = value;
        return uint64_t(wide * wide);
    }

    static uint64_t Isqrt(uint64_t value) {
        uint64_t result = 0;
        uint64_t bit = 1ULL << 62;
        while (bit > value) {
            bit >>= 2;
        }
        while (bit != 0) {
            if (value >= result + bit) {
                value -= result + bit;
                result = (result >> 1) + bit;
            } else {
                result >>= 1;
            }
            bit >>= 2;
        }
        return result;
    }

    int32_t bx_ = 0;
    int32_t by_ = 0;
    int32_t bz_ = 0;
    uint32_t armed_ms_ = 0;
    uint32_t fired_ms_ = 0;
    bool ready_ = false;
    bool armed_ = false;
    bool fired_ = false;
};
