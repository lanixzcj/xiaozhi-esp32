#pragma once

#include <cstdint>
#include <cstring>

// One silent round from the model. Centers are on the 320x240 landscape screen.
// Size and position are clamped so a toddler-sized dot stays fully on screen.
// A payload that is not this object is rejected (wrong types, bad colors, not 1–5 dots).
struct DotRound {
    enum class Color { Yellow, Red, Blue };

    struct Dot {
        int x = 0;  // top-left, after clamping
        int y = 0;
        int size = 64;
        int order = 0;
        Color color = Color::Yellow;
    };

    static constexpr int kMinDots = 1;
    static constexpr int kMaxDots = 5;
    static constexpr int kMinSize = 56;
    static constexpr int kMaxSize = 80;
    static constexpr int kMargin = 8;
    static constexpr int kScreenW = 320;
    static constexpr int kScreenH = 240;
    static constexpr int kDefaultGapMs = 180;
    static constexpr int kMinGapMs = 80;
    static constexpr int kMaxGapMs = 400;
    static constexpr size_t kMaxJson = 768;

    int count = 0;
    int gap_ms = kDefaultGapMs;
    Dot dots[kMaxDots] = {};

    static bool Parse(const char* json, DotRound& out);
};

// One outstanding request. Play never waits: Take() stays empty until a valid
// reply arrives, and a reply older than kTimeoutMs is ignored.
struct DotRoundQueue {
    static constexpr uint32_t kTimeoutMs = 1500;
    static constexpr int kMaxTries = 3;

    uint32_t Begin(uint32_t now) {
        if (inflight_ || has_ready_) {
            return 0;
        }
        inflight_ = true;
        sent_ms_ = now;
        return ++id_;
    }

    void AbortSend() { inflight_ = false; }

    bool OnReply(uint32_t id, const char* json) {
        if (!inflight_ || id != id_) {
            return false;
        }
        inflight_ = false;
        DotRound round;
        if (!DotRound::Parse(json, round)) {
            ++failures_;
            return false;
        }
        ready_ = round;
        has_ready_ = true;
        failures_ = 0;
        return true;
    }

    bool Offer(const char* json) {
        DotRound round;
        if (!DotRound::Parse(json, round)) {
            return false;
        }
        ready_ = round;
        has_ready_ = true;
        return true;
    }

    bool TimedOut(uint32_t now) const {
        return inflight_ && uint32_t(now - sent_ms_) >= kTimeoutMs;
    }

    void Expire() {
        if (inflight_) {
            inflight_ = false;
            ++failures_;
        }
    }

    bool Has() const { return has_ready_; }
    bool inflight() const { return inflight_; }
    int failures() const { return failures_; }
    uint32_t id() const { return id_; }

    bool Take(DotRound& out) {
        if (!has_ready_) {
            return false;
        }
        out = ready_;
        has_ready_ = false;
        return true;
    }

    void Reset() {
        inflight_ = false;
        has_ready_ = false;
        failures_ = 0;
    }

private:
    bool inflight_ = false;
    bool has_ready_ = false;
    int failures_ = 0;
    uint32_t id_ = 0;
    uint32_t sent_ms_ = 0;
    DotRound ready_{};
};

namespace dot_round_parse {

struct Parser {
    const char* p;
    const char* end;
    bool ok = true;

    void Ws() {
        while (p < end && (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t')) {
            ++p;
        }
    }

    bool Finished() {
        Ws();
        return ok && p == end;
    }

    bool Eat(char c) {
        Ws();
        if (p < end && *p == c) {
            ++p;
            return true;
        }
        ok = false;
        return false;
    }

    bool Try(char c) {
        Ws();
        if (p < end && *p == c) {
            ++p;
            return true;
        }
        return false;
    }

    bool SkipString() {
        Ws();
        if (p >= end || *p != '"') {
            ok = false;
            return false;
        }
        ++p;
        while (p < end && *p != '"') {
            if (*p == '\\') {
                ++p;
                if (p >= end) {
                    ok = false;
                    return false;
                }
            }
            ++p;
        }
        if (p >= end || *p != '"') {
            ok = false;
            return false;
        }
        ++p;
        return true;
    }

    bool ParseKey(char* out, size_t cap, bool& known) {
        Ws();
        if (p >= end || *p != '"') {
            ok = false;
            return false;
        }
        ++p;
        size_t n = 0;
        known = true;
        while (p < end && *p != '"') {
            char c = *p++;
            if (c == '\\' && p < end) {
                c = *p++;
            }
            if (n + 1 < cap) {
                out[n++] = c;
            } else {
                known = false;
            }
        }
        if (p >= end || *p != '"') {
            ok = false;
            return false;
        }
        ++p;
        out[n] = '\0';
        return true;
    }

    bool ParseInt(int& out) {
        Ws();
        if (p >= end) {
            ok = false;
            return false;
        }
        int sign = 1;
        if (*p == '-') {
            sign = -1;
            ++p;
        }
        if (p >= end || *p < '0' || *p > '9') {
            ok = false;
            return false;
        }
        int value = 0;
        int digits = 0;
        while (p < end && *p >= '0' && *p <= '9') {
            if (++digits > 6) {
                ok = false;
                return false;
            }
            value = value * 10 + (*p - '0');
            ++p;
        }
        if (p < end && *p == '.') {
            ++p;
            while (p < end && *p >= '0' && *p <= '9') {
                ++p;
            }
        }
        out = sign * value;
        return true;
    }

    bool SkipValue(int depth) {
        if (!ok || depth > 6) {
            ok = false;
            return false;
        }
        Ws();
        if (p >= end) {
            ok = false;
            return false;
        }
        const char c = *p;
        if (c == '"') {
            return SkipString();
        }
        if (c == '{') {
            ++p;
            if (Try('}')) {
                return true;
            }
            while (ok) {
                if (!SkipString() || !Eat(':') || !SkipValue(depth + 1)) {
                    return false;
                }
                if (Try('}')) {
                    return true;
                }
                if (!Eat(',')) {
                    return false;
                }
            }
            return false;
        }
        if (c == '[') {
            ++p;
            if (Try(']')) {
                return true;
            }
            while (ok) {
                if (!SkipValue(depth + 1)) {
                    return false;
                }
                if (Try(']')) {
                    return true;
                }
                if (!Eat(',')) {
                    return false;
                }
            }
            return false;
        }
        if (c == 't' || c == 'f' || c == 'n') {
            const char* word = c == 't' ? "true" : c == 'f' ? "false" : "null";
            for (; *word != '\0'; ++word, ++p) {
                if (p >= end || *p != *word) {
                    ok = false;
                    return false;
                }
            }
            return true;
        }
        int ignored = 0;
        return ParseInt(ignored);
    }

    static int Clamp(int value, int lo, int hi) {
        if (value < lo) {
            return lo;
        }
        return value > hi ? hi : value;
    }

    static bool ColorOf(const char* text, DotRound::Color& color) {
        if (std::strcmp(text, "yellow") == 0) {
            color = DotRound::Color::Yellow;
            return true;
        }
        if (std::strcmp(text, "red") == 0) {
            color = DotRound::Color::Red;
            return true;
        }
        if (std::strcmp(text, "blue") == 0) {
            color = DotRound::Color::Blue;
            return true;
        }
        return false;
    }

    bool ParseDot(DotRound::Dot& dot, int index) {
        if (!Eat('{')) {
            return false;
        }
        bool has_cx = false;
        bool has_cy = false;
        bool has_color = false;
        int cx = 0;
        int cy = 0;
        int size = 64;
        int order = index;
        DotRound::Color color = DotRound::Color::Yellow;
        if (Try('}')) {
            ok = false;
            return false;
        }
        while (ok) {
            char key[16];
            bool known = true;
            if (!ParseKey(key, sizeof(key), known) || !Eat(':')) {
                return false;
            }
            if (!known) {
                if (!SkipValue(1)) {
                    return false;
                }
            } else if (std::strcmp(key, "cx") == 0) {
                if (!ParseInt(cx)) {
                    return false;
                }
                has_cx = true;
            } else if (std::strcmp(key, "cy") == 0) {
                if (!ParseInt(cy)) {
                    return false;
                }
                has_cy = true;
            } else if (std::strcmp(key, "size") == 0) {
                if (!ParseInt(size)) {
                    return false;
                }
            } else if (std::strcmp(key, "order") == 0) {
                if (!ParseInt(order)) {
                    return false;
                }
            } else if (std::strcmp(key, "color") == 0) {
                char name[16];
                bool name_ok = true;
                if (!ParseKey(name, sizeof(name), name_ok) || !name_ok || !ColorOf(name, color)) {
                    ok = false;
                    return false;
                }
                has_color = true;
            } else if (!SkipValue(1)) {
                return false;
            }
            if (Try('}')) {
                break;
            }
            if (!Eat(',')) {
                return false;
            }
        }
        if (!ok || !has_cx || !has_cy || !has_color) {
            ok = false;
            return false;
        }
        size = Clamp(size, DotRound::kMinSize, DotRound::kMaxSize);
        const int half = size / 2;
        const int min_c = DotRound::kMargin + half;
        cx = Clamp(cx, min_c, DotRound::kScreenW - DotRound::kMargin - half);
        cy = Clamp(cy, min_c, DotRound::kScreenH - DotRound::kMargin - half);
        dot.x = cx - half;
        dot.y = cy - half;
        dot.size = size;
        dot.order = Clamp(order, 0, DotRound::kMaxDots - 1);
        dot.color = color;
        return true;
    }

    bool ParseRound(DotRound& round) {
        if (!Eat('{')) {
            return false;
        }
        bool has_dots = false;
        int gap = DotRound::kDefaultGapMs;
        if (Try('}')) {
            ok = false;
            return false;
        }
        while (ok) {
            char key[16];
            bool known = true;
            if (!ParseKey(key, sizeof(key), known) || !Eat(':')) {
                return false;
            }
            if (!known) {
                if (!SkipValue(1)) {
                    return false;
                }
            } else if (std::strcmp(key, "gap_ms") == 0) {
                if (!ParseInt(gap)) {
                    return false;
                }
            } else if (std::strcmp(key, "dots") == 0) {
                if (!Eat('[')) {
                    return false;
                }
                int count = 0;
                if (!Try(']')) {
                    while (ok) {
                        if (count >= DotRound::kMaxDots) {
                            ok = false;
                            return false;
                        }
                        if (!ParseDot(round.dots[count], count)) {
                            return false;
                        }
                        ++count;
                        if (Try(']')) {
                            break;
                        }
                        if (!Eat(',')) {
                            return false;
                        }
                    }
                }
                if (count < DotRound::kMinDots) {
                    ok = false;
                    return false;
                }
                round.count = count;
                has_dots = true;
            } else if (!SkipValue(1)) {
                return false;
            }
            if (Try('}')) {
                break;
            }
            if (!Eat(',')) {
                return false;
            }
        }
        if (!ok || !has_dots) {
            ok = false;
            return false;
        }
        round.gap_ms = Clamp(gap, DotRound::kMinGapMs, DotRound::kMaxGapMs);
        return true;
    }
};

}  // namespace dot_round_parse

inline bool DotRound::Parse(const char* json, DotRound& out) {
    if (json == nullptr) {
        return false;
    }
    const size_t length = std::strlen(json);
    if (length == 0 || length > kMaxJson) {
        return false;
    }
    dot_round_parse::Parser parser{json, json + length, true};
    DotRound round;
    if (!parser.ParseRound(round) || !parser.ok || !parser.Finished()) {
        return false;
    }
    out = round;
    return true;
}
