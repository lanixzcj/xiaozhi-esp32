#pragma once

#include <cmath>
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
    static constexpr int kDeadzoneMg = 35;
    static constexpr int kMaxPullMg = 1000;
    // Pixels/s^2 at 1 g. A gentle relative tilt can reach an edge; the weaker spring
    // lets the neutral holding pose bring them back to the page layout.
    static constexpr float kGravity = 1200.f;
    static constexpr float kSpring = 1.8f;
    static constexpr float kDampPerSec = 2.8f;
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
        float size = kDot;
        bool alive = false;
    };

    // pull_*_mg is the low-side direction in screen pixels. Small noise is zero.
    static void Map(int16_t ax, int16_t ay, int16_t az, int& pull_x_mg, int& pull_y_mg) {
        (void)az;
        pull_x_mg = ClampPull(kSignX * MilliG(ax));
        pull_y_mg = ClampPull(kSignY * MilliG(ay));
    }

    static bool IsGravitySample(int16_t ax, int16_t ay, int16_t az) {
        float x = ax, y = ay, z = az;
        return NormalizeGravity(x, y, z);
    }

    // Compare complete gravity vectors, rather than subtracting X/Y. Near
    // upright, both forward and backward pitch reduce Y; their Z signs differ.
    // The shortest relative rotation (current gravity cross neutral gravity)
    // distinguishes them without gyro integration or accumulated drift.
    static bool MapRelative(int16_t ax, int16_t ay, int16_t az, int16_t neutral_x,
                            int16_t neutral_y, int16_t neutral_z, int& pull_x_mg, int& pull_y_mg) {
        pull_x_mg = pull_y_mg = 0;
        float x = ax, y = ay, z = az;
        float nx = neutral_x, ny = neutral_y, nz = neutral_z;
        if (!NormalizeGravity(x, y, z) || !NormalizeGravity(nx, ny, nz))
            return false;
        // Right-control rotation axis is gravity cross screen-right. Down is
        // the perpendicular tangent axis. These adapt to flat and upright use.
        float rx = 0.f, ry = nz, rz = -ny;
        float length = std::sqrt(ry * ry + rz * rz);
        if (length < 0.01f) {
            ry = 1.f;
            rz = 0.f;
        } else {
            ry /= length;
            rz /= length;
        }
        const float dx = ry * nz - rz * ny;
        const float dy = rz * nx;
        const float dz = -ry * nx;
        const float cx = y * nz - z * ny;
        const float cy = z * nx - x * nz;
        const float cz = x * ny - y * nx;
        constexpr float sensitivity = 1400.f;  // 1.4 g pull at sin(angle)=1
        pull_x_mg = ClampPull(int(sensitivity * (rx * cx + ry * cy + rz * cz)));
        pull_y_mg = ClampPull(int(sensitivity * (dx * cx + dy * cy + dz * cz)));
        return true;
    }

    void NoteHome(int index, float x, float y, bool alive, float size = kDot) {
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
        body.size = size < 1.f ? float(kDot) : size;
        body.alive = alive;
    }

    void DragTo(int index, float x, float y) {
        if (index < 0 || index >= kDots || !bodies_[index].alive)
            return;
        auto& b = bodies_[index];
        const float right = kScreenW - b.size - kMargin;
        const float bottom = kScreenH - b.size - kMargin;
        b.x = b.home_x = x < kMargin ? kMargin : (x > right ? right : x);
        b.y = b.home_y = y < kMargin ? kMargin : (y > bottom ? bottom : y);
        b.vx = b.vy = 0;
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
        const float min_y = float(kMargin);
        for (auto& body : bodies_) {
            if (!body.alive) {
                continue;
            }
            const float max_x = float(kScreenW) - body.size - float(kMargin);
            const float max_y = float(kScreenH) - body.size - float(kMargin);
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
    static bool NormalizeGravity(float& x, float& y, float& z) {
        const float length = std::sqrt(x * x + y * y + z * z);
        // Ignore strong linear acceleration/free fall instead of treating a
        // shake as a new holding pose or a huge tilt command.
        if (length < 0.75f * kLsbPerG || length > 1.25f * kLsbPerG)
            return false;
        x /= length;
        y /= length;
        z /= length;
        return true;
    }

    static int MilliG(int raw) { return int(raw) * 1000 / kLsbPerG; }

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
