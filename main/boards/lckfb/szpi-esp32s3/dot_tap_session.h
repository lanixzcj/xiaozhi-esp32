#pragma once

#include <cstdint>

// One round of the flat-dot game: one dot, five dots, mixed colors, a shake
// that scatters them, then a cross. The session never plays audio or moves
// pixels. kLimitMs only opens the rest page when the game was left running.
class DotTapSession {
public:
    enum class Page { Closed, One, Five, Mixed, Scattering, Settling, Settled, Rest };

    static constexpr int kDots = 5;
    static constexpr int kCenter = 2;
    static constexpr uint32_t kLimitMs = 180000;
    static constexpr uint32_t kParentHoldMs = 1500;
    static constexpr uint32_t kScatterMs = 480;
    static constexpr uint32_t kSettleMs = 560;

    void Start(uint32_t now) {
        started_ = now;
        mark_ = now;
        page_ = Page::One;
        last_tap_ = -1;
    }
    void Close() { page_ = Page::Closed; }

    bool Tap(uint32_t now, int index) {
        if (page_ == Page::Closed || page_ == Page::Rest) {
            return false;
        }
        if (!Alive(now)) {
            page_ = Page::Rest;
            return true;
        }
        switch (page_) {
            case Page::One:
                if (index != kCenter) {
                    return false;
                }
                page_ = Page::Five;
                return true;
            case Page::Five:
                if (index < 0 || index >= kDots) {
                    return false;
                }
                page_ = Page::Mixed;
                return true;
            case Page::Mixed:
                if (index < 0 || index >= kDots) {
                    return false;
                }
                last_tap_ = index;
                return true;
            case Page::Settled:
                page_ = Page::One;
                last_tap_ = -1;
                return true;
            default:
                return false;
        }
    }

    bool Shake(uint32_t now) {
        if (page_ != Page::Mixed || !Alive(now)) {
            return false;
        }
        mark_ = now;
        page_ = Page::Scattering;
        return true;
    }

    bool Tick(uint32_t now) {
        if (page_ == Page::Closed || page_ == Page::Rest) {
            return false;
        }
        if (!Alive(now)) {
            page_ = Page::Rest;
            return true;
        }
        if (page_ == Page::Scattering && uint32_t(now - mark_) >= kScatterMs) {
            mark_ = now;
            page_ = Page::Settling;
            return true;
        }
        if (page_ == Page::Settling && uint32_t(now - mark_) >= kSettleMs) {
            page_ = Page::Settled;
            return true;
        }
        return false;
    }

    Page page() const { return page_; }
    int last_tap() const { return last_tap_; }

private:
    bool Alive(uint32_t now) const { return uint32_t(now - started_) < kLimitMs; }

    Page page_ = Page::Closed;
    uint32_t started_ = 0;
    uint32_t mark_ = 0;
    int last_tap_ = -1;
};
