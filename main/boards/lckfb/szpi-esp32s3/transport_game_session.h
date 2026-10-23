#pragma once
#include <cstddef>
#include <cstdint>

// Pure two-choice policy. No background autoplay: each prompt/replay needs a tap.
class TransportGameSession {
public:
    enum class Phase { Closed, Ready, Listening, Choosing, Feedback, Rest };
    static constexpr size_t kCount = 4;
    static constexpr uint32_t kLimitMs = 90000;
    static constexpr uint32_t kFeedbackMs = 900;
    void Start(uint32_t now, size_t seed = 0) {
        started_ = now;
        seed_ = seed % kCount;
        round_ = 0;
        correct_ = false;
        phase_ = Phase::Ready;
    }
    void Close() { phase_ = Phase::Closed; }
    bool CanListen(uint32_t now) const {
        return Alive(now) && (phase_ == Phase::Ready || phase_ == Phase::Choosing);
    }
    bool Listen(uint32_t now) {
        if (!CanListen(now))
            return false;
        phase_ = Phase::Listening;
        return true;
    }
    bool Choose(uint32_t now, size_t side) {
        if (!Alive(now) || phase_ != Phase::Choosing || side >= 2)
            return false;
        correct_ = choice(side) == target();
        feedback_started_ = now;
        phase_ = Phase::Feedback;
        return true;
    }
    bool Tick(uint32_t now, bool playback_idle) {
        if (phase_ == Phase::Closed || phase_ == Phase::Rest)
            return false;
        if (!Alive(now)) {
            phase_ = Phase::Rest;
            return true;
        }
        if (phase_ == Phase::Listening && playback_idle) {
            phase_ = Phase::Choosing;
            return true;
        }
        if (phase_ == Phase::Feedback && uint32_t(now - feedback_started_) >= kFeedbackMs) {
            if (correct_) {
                ++round_;
                phase_ = round_ == kCount ? Phase::Rest : Phase::Ready;
            } else {
                phase_ = Phase::Choosing;
            }
            return true;
        }
        return false;
    }
    Phase phase() const { return phase_; }
    size_t round() const { return round_; }
    size_t target() const { return (seed_ + round_) % kCount; }
    size_t choice(size_t side) const {
        const size_t answer_side = (seed_ + round_) % 2;
        return side == answer_side ? target() : (target() + 1 + (round_ % 3)) % kCount;
    }
    bool correct() const { return correct_; }

private:
    bool Alive(uint32_t now) const { return uint32_t(now - started_) < kLimitMs; }
    Phase phase_ = Phase::Closed;
    uint32_t started_ = 0, feedback_started_ = 0;
    size_t seed_ = 0, round_ = 0;
    bool correct_ = false;
};
