#pragma once

#include <cstddef>
#include <cstdint>

// Two-choice "listen, then tap a picture" policy shared by the vehicle and
// animal games. The session never starts audio by itself: the UI asks for a
// prompt, a replay, a wrong-answer replay, or the cheer. kLimitMs is not shown
// and is not a score. It only rests a game that was left open so the microphone
// can come back.
class ListenPickSession {
public:
    enum class Phase { Closed, Ready, Listening, Choosing, Feedback, Rest };
    static constexpr uint32_t kLimitMs = 180000;
    static constexpr uint32_t kFeedbackMs = 900;
    static constexpr uint32_t kParentHoldMs = 1500;

    explicit ListenPickSession(size_t count) : count_(count < 2 ? 2 : count) {}

    void Start(uint32_t now, size_t seed = 0) {
        started_ = now;
        seed_ = seed % count_;
        round_ = 0;
        correct_ = false;
        phase_ = Phase::Ready;
    }
    void Close() { phase_ = Phase::Closed; }

    bool CanListen(uint32_t now) const {
        return Alive(now) && (phase_ == Phase::Ready || phase_ == Phase::Choosing);
    }
    bool Listen(uint32_t now) {
        if (!CanListen(now)) {
            return false;
        }
        phase_ = Phase::Listening;
        return true;
    }
    bool Choose(uint32_t now, size_t side) {
        if (!Alive(now) || phase_ != Phase::Choosing || side >= 2) {
            return false;
        }
        correct_ = choice(side) == target();
        feedback_started_ = now;
        phase_ = Phase::Feedback;
        return true;
    }
    // playback_idle must be true before feedback ends, so a cheer or a
    // wrong-answer replay is not cut off and the next tap cannot queue over it.
    bool Tick(uint32_t now, bool playback_idle) {
        if (phase_ == Phase::Closed || phase_ == Phase::Rest) {
            return false;
        }
        if (!Alive(now)) {
            phase_ = Phase::Rest;
            return true;
        }
        if (phase_ == Phase::Listening && playback_idle) {
            phase_ = Phase::Choosing;
            return true;
        }
        if (phase_ == Phase::Feedback && playback_idle &&
            uint32_t(now - feedback_started_) >= kFeedbackMs) {
            if (correct_) {
                ++round_;
                phase_ = round_ == count_ ? Phase::Rest : Phase::Ready;
            } else {
                phase_ = Phase::Choosing;
            }
            return true;
        }
        return false;
    }

    Phase phase() const { return phase_; }
    size_t count() const { return count_; }
    size_t round() const { return round_; }
    size_t target() const { return (seed_ + round_) % count_; }
    size_t choice(size_t side) const {
        const size_t answer_side = (seed_ + round_) % 2;
        if (side == answer_side) {
            return target();
        }
        // For four vehicles this is the previous pairing (offset 1, 2, or 3).
        const size_t offset = 1 + (round_ % (count_ - 1));
        return (target() + offset) % count_;
    }
    bool correct() const { return correct_; }

private:
    bool Alive(uint32_t now) const { return uint32_t(now - started_) < kLimitMs; }

    Phase phase_ = Phase::Closed;
    uint32_t started_ = 0;
    uint32_t feedback_started_ = 0;
    size_t count_;
    size_t seed_ = 0;
    size_t round_ = 0;
    bool correct_ = false;
};
