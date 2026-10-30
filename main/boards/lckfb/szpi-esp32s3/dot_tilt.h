#pragma once

#include <cstdint>

// Gravity for the flat dots. The QMI8658 reading points "up" (specific force).
// Dots accelerate the other way, toward the low edge of the screen.
//
// Assumed mounting, screen facing you, landscape 320x240 (LVGL +x right, +y down):
//   chip +X  along screen +x (right)
//   chip +Y  along screen up, so opposite LVGL +y
//   chip +Z  out of the screen
// kSignX / kSignY flip a real board whose sensor is rotated. They do not change
// the on-screen meaning of a positive pull: +pull_x moves dots right, +pull_y down.
class DotRoll {
public:
    static constexpr int kLsbPerG = 8192;
    static constexpr int kSignX = -1;
    static constexpr int kSignY = 1;
    static constexpr int kDeadzoneMg = 70;
    static constexpr int kMaxPullMg = 1000;
    // Pixels/s^2 at 1 g. With the spring below, ~30° gathers dots toward an edge
    // and a level board lets them drift back to the page layout.
    static constexpr float kGravity = 900.f;
    static constexpr float kSpring = 6.f;
    static constexpr float kDampPerSec = 3.5f;
    static constexpr float kBounce = 0.35f;
    static constexpr float kMaxSpeed = 420.f;
    static constexpr int kScreenW = 320;
    static constexpr int kScreenH = 240;
    static constexpr int kDot = 64;
    static constexpr int kMargin = 4;

    struct Body {
        float x = 0;
        float y = 0;
        float vx = 0;
        float vy = 0;
        float home_x = 0;
        float home_y = 0;
        bool alive = false;
    };

    // pull_*_mg is the low-side direction in screen pixels. Small noise is zero.
    static void Map(int16_t ax, int16_t ay, int16_t az, int& pull_x_mg, int& pull_y_mg) {
        (void)az;
        pull_x_mg = ClampPull(kSignX * MilliG(ax));
        pull_y_mg = ClampPull(kSignY * MilliG(ay));
    }

    void NoteHome(int index, float x, float y, bool alive) {
        if (index < 0 || index >= kDots) {
            return;
        }
        auto& body = bodies_[index];
        body.home_x = x;
        body.home_y = y;
        body.x = x;
        body.y = y;
        body.vx = 0;
        body.vy = 0;
        body.alive = alive;
    }

    void Step(int pull_x_mg, int pull_y_mg, uint32_t dt_ms) {
        float dt = dt_ms / 1000.f;
        if (dt <= 0.f) {
            return;
        }
        if (dt > 0.1f) {
            dt = 0.1f;
        }
        float keep = 1.f - kDampPerSec * dt;
        if (keep < 0.5f) {
            keep = 0.5f;
        }
        const float gx = (pull_x_mg / 1000.f) * kGravity;
        const float gy = (pull_y_mg / 1000.f) * kGravity;
        const float min_x = float(kMargin);
        const float max_x = float(kScreenW - kDot - kMargin);
        const float min_y = float(kMargin);
        const float max_y = float(kScreenH - kDot - kMargin);
        for (auto& body : bodies_) {
            if (!body.alive) {
                continue;
            }
            body.vx += (kSpring * (body.home_x - body.x) + gx) * dt;
            body.vy += (kSpring * (body.home_y - body.y) + gy) * dt;
            body.vx *= keep;
            body.vy *= keep;
            body.vx = ClampSpeed(body.vx);
            body.vy = ClampSpeed(body.vy);
            body.x += body.vx * dt;
            body.y += body.vy * dt;
            if (body.x < min_x) {
                body.x = min_x;
                body.vx = pull_x_mg < 0 ? 0.f : -body.vx * kBounce;
            } else if (body.x > max_x) {
                body.x = max_x;
                body.vx = pull_x_mg > 0 ? 0.f : -body.vx * kBounce;
            }
            if (body.y < min_y) {
                body.y = min_y;
                body.vy = pull_y_mg < 0 ? 0.f : -body.vy * kBounce;
            } else if (body.y > max_y) {
                body.y = max_y;
                body.vy = pull_y_mg > 0 ? 0.f : -body.vy * kBounce;
            }
        }
    }

    const Body& body(int index) const { return bodies_[index]; }

    static constexpr int kDots = 5;

private:
    static int MilliG(int16_t raw) { return int(raw) * 1000 / kLsbPerG; }

    static int ClampPull(int milli_g) {
        if (milli_g > -kDeadzoneMg && milli_g < kDeadzoneMg) {
            return 0;
        }
        if (milli_g > kMaxPullMg) {
            return kMaxPullMg;
        }
        if (milli_g < -kMaxPullMg) {
            return -kMaxPullMg;
        }
        return milli_g;
    }

    static float ClampSpeed(float speed) {
        if (speed > kMaxSpeed) {
            return kMaxSpeed;
        }
        if (speed < -kMaxSpeed) {
            return -kMaxSpeed;
        }
        return speed;
    }

    Body bodies_[kDots] = {};
};
