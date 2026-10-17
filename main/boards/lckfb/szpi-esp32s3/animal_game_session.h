#pragma once

#include <cstddef>
#include <cstdint>

// Pure session policy: no timer, hardware, network, allocation, or auto-play.
// A parent starts three rounds; each round requires a new released tap.
class AnimalGameSession {
public:
    enum class Phase { Closed, Ready, Feedback, Rest };
    static constexpr size_t kAnimalCount = 3;
    static constexpr uint32_t kSessionLimitMs = 90000;
    static constexpr uint32_t kFeedbackMinimumMs = 800;
    static constexpr uint32_t kParentHoldMs = 1500;

    void Start(uint32_t now) {
        phase_ = Phase::Ready;
        animal_ = 0;
        started_ = now;
    }
    void Close() { phase_ = Phase::Closed; }
    bool CanTap(uint32_t now) const {
        return phase_ == Phase::Ready && uint32_t(now - started_) < kSessionLimitMs;
    }
    bool AcceptTap(uint32_t now) {
        if (!CanTap(now)) {
            return false;
        }
        phase_ = Phase::Feedback;
        feedback_started_ = now;
        return true;
    }
    bool Tick(uint32_t now, bool playback_idle) {
        if (phase_ == Phase::Closed || phase_ == Phase::Rest) {
            return false;
        }
        if (uint32_t(now - started_) >= kSessionLimitMs) {
            phase_ = Phase::Rest;
            return true;
        }
        if (phase_ == Phase::Feedback && playback_idle &&
            uint32_t(now - feedback_started_) >= kFeedbackMinimumMs) {
            if (animal_ + 1 == kAnimalCount) {
                phase_ = Phase::Rest;
            } else {
                ++animal_;
                phase_ = Phase::Ready;
            }
            return true;
        }
        return false;
    }
    Phase phase() const { return phase_; }
    size_t animal() const { return animal_; }

private:
    Phase phase_ = Phase::Closed;
    size_t animal_ = 0;
    uint32_t started_ = 0;
    uint32_t feedback_started_ = 0;
};
