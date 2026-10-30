"""Host tests for the flat-dot session, shake detector, and board wiring."""
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
BOARD = ROOT / "main/boards/lckfb/szpi-esp32s3"


class DotTapGameTests(unittest.TestCase):
    def test_session_and_shake(self):
        code = r"""
#include "dot_tap_session.h"
#include "dot_tilt.h"
#include "shake_detector.h"
#include <cassert>
#include <limits>
int main() {
 using S = DotTapSession; using P = S::Page;
 S s;
 assert(s.page() == P::Closed);
 assert(!s.Tap(0, S::kCenter) && !s.Shake(0) && !s.Tick(1));
 s.Start(0);
 assert(s.page() == P::One);
 assert(!s.Tap(0, 0) && !s.Shake(0));
 assert(s.Tap(0, S::kCenter) && s.page() == P::Five);
 assert(!s.Tap(1, -1));
 assert(s.Tap(1, 4) && s.page() == P::Mixed);
 assert(s.Tap(2, 4) && s.page() == P::Mixed && s.last_tap() == 4);
 assert(!s.Tick(3));
 assert(s.Shake(1000) && s.page() == P::Scattering);
 assert(!s.Shake(1001) && !s.Tap(1001, 0));
 assert(!s.Tick(1000 + S::kScatterMs - 1));
 assert(s.Tick(1000 + S::kScatterMs) && s.page() == P::Settling);
 assert(!s.Tick(1000 + S::kScatterMs + S::kSettleMs - 1));
 assert(s.Tick(1000 + S::kScatterMs + S::kSettleMs) && s.page() == P::Settled);
 assert(s.Tap(2000, -1) && s.page() == P::One);
 s.Start(0);
 assert(s.Tap(0, S::kCenter));
 assert(!s.Tick(S::kLimitMs - 1));
 assert(s.Tick(S::kLimitMs) && s.page() == P::Rest);
 assert(!s.Tick(S::kLimitMs + 1000) && !s.Tap(1, 0) && !s.Shake(1));
 constexpr uint32_t start = std::numeric_limits<uint32_t>::max() - 50;
 s.Start(start);
 assert(s.Tap(start, S::kCenter));
 assert(!s.Tick(uint32_t(start + 40)));
 assert(s.Tick(uint32_t(start + S::kLimitMs)) && s.page() == P::Rest);

 ShakeDetector shake;
 assert(!shake.Feed(0, 0, 8192, 0));
 for (uint32_t t = 50; t <= 2000; t += 50) {
  int32_t z = 8192 - int32_t(t);
  assert(!shake.Feed(int16_t(t / 2), 0, int16_t(z > 0 ? z : 0), t));
 }
 shake.Reset();
 assert(!shake.Feed(0, 0, 8192, 0));
 assert(!shake.Feed(0, 0, 8192 + 8000, 50));
 assert(!shake.Feed(0, 0, 8192, 400));
 shake.Reset();
 assert(!shake.Feed(0, 0, 8192, 0));
 assert(!shake.Feed(0, 0, 8192 + 6000, 50));
 assert(shake.Feed(0, 0, 8192 + 6000, 100));
 assert(!shake.Feed(0, 0, 8192 + 6000, 160));
 assert(!shake.Feed(0, 0, 8192 + 6000, 210));
 for (int i = 0; i < 16; ++i) assert(!shake.Feed(0, 0, 8192 + 6000, 300 + uint32_t(i) * 50));
 assert(!shake.Feed(0, 0, 8192 + 14000, 1200));
 assert(shake.Feed(0, 0, 8192 + 14000, 1250));
 shake.Reset();
 assert(!shake.Feed(0, 0, 8192, 0));
 for (int i = 1; i <= 40; ++i) {
  assert(!shake.Feed(int16_t(i * 80), 0, int16_t(8192 - i * 40), uint32_t(i) * 50));
 }
 // A deliberate tilt, about 0.04 g per 50 ms, must not scatter the page.
 shake.Reset();
 assert(!shake.Feed(0, 0, 8192, 0));
 for (int i = 1; i <= 30; ++i) {
  assert(!shake.Feed(int16_t(-300 * i), 0, 6000, uint32_t(i) * 50));
 }

 int px = 0, py = 0;
 DotRoll::Map(0, 0, 8192, px, py);
 assert(px == 0 && py == 0);
 DotRoll::Map(-4096, 0, 5000, px, py);
 assert(px > 400 && py == 0);  // chip -X is screen-right
 DotRoll::Map(4096, 0, 5000, px, py);
 assert(px < -400 && py == 0);
 DotRoll::Map(0, 4096, 5000, px, py);
 assert(py > 400 && px == 0);  // chip +Y is screen-down in LVGL
 DotRoll::Map(200, 0, 8192, px, py);
 assert(px == 0);  // inside the deadzone

 DotRoll roll;
 roll.NoteHome(2, 128, 88, true);
 for (int i = 0; i < 30; ++i) roll.Step(800, 0, 50);
 assert(roll.body(2).x > 200);
 assert(roll.body(2).x <= 320 - 64 - 4);
 assert(roll.body(2).y > 70 && roll.body(2).y < 110);
 for (int i = 0; i < 50; ++i) roll.Step(0, 0, 50);
 assert(roll.body(2).x > 90 && roll.body(2).x < 170);
 roll.NoteHome(0, 24, 14, true);
 for (int i = 0; i < 40; ++i) roll.Step(-1000, 0, 50);
 assert(roll.body(0).x <= 8);
}
"""
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary)
            (path / "test.cc").write_text(code)
            subprocess.run(
                ["g++", "-std=c++17", "-Wall", "-Wextra", "-Werror", "-I", str(BOARD),
                 str(path / "test.cc"), "-o", str(path / "test")],
                check=True,
            )
            subprocess.run([str(path / "test")], check=True)

    def test_sounds_and_wiring(self):
        import sys
        sys.path.insert(0, str(BOARD / "tools"))
        from generate_animal_sounds import ogg_packets, packet_duration_ms

        text = (BOARD / "dot_tap_sounds.h").read_text()
        for symbol, low_ms, high_ms in (
            ("kYellow", 240, 300), ("kRed", 240, 300), ("kBlue", 240, 300),
            ("kScatter", 360, 420), ("kSettle", 420, 480),
        ):
            blob = text.split(f"{symbol}[] = {{", 1)[1].split("};", 1)[0]
            data = bytes(int(token, 16) for token in blob.split(",") if token.strip())
            packets = ogg_packets(data)
            self.assertEqual(packets[0][:8], b"OpusHead")
            self.assertEqual(packets[0][9], 1)
            self.assertEqual(int.from_bytes(packets[0][12:16], "little"), 24000)
            self.assertTrue(all(packet_duration_ms(packet) == 60 for packet in packets[2:]))
            playback_ms = len(packets[2:]) * 60
            self.assertGreaterEqual(playback_ms, low_ms)
            self.assertLessEqual(playback_ms, high_ms)
            self.assertLess(len(data), 6 * 1024)

        game = (BOARD / "dot_tap_game.cc").read_text()
        self.assertIn("轻点", game)
        self.assertIn("kFallbackHoldMs", game)
        self.assertIn("dot_tap_sounds::Get", game)
        self.assertNotIn("点点点", game)
        self.assertNotIn("Press Here", game)
        for forbidden in ("lv_screen_load", "lv_obj_clean", "lv_obj_delete", "nvs_", "esp_restart",
                          "LV_EVENT_LONG_PRESSED_REPEAT", "LV_EVENT_PRESSING"):
            self.assertNotIn(forbidden, game)
        font = (BOARD / "animal_game_font.c").read_text()
        for character in "轻点休息一下吧和爸爸妈妈再点一点家长长按再玩返回":
            self.assertIn(f"U+{ord(character):04X}", font)

        board = (BOARD / "lichuang_dev_board.cc").read_text()
        self.assertIn("imu_.Init(i2c_bus_)", board)
        self.assertIn("dot_game_->RequestExit()", board)
        self.assertEqual(board.count("(dot_game_ && dot_game_->IsVisible())"), 4)
        self.assertEqual(board.count("(transport_game_ && transport_game_->IsVisible())"), 4)
        self.assertEqual(board.count("(animal_game_ && animal_game_->IsVisible())"), 4)
        driver = (BOARD / "qmi8658.cc").read_text()
        self.assertIn("0x6A", (BOARD / "qmi8658.h").read_text())
        self.assertIn("kWhoAmI", driver)
        self.assertNotIn("ESP_ERROR_CHECK", driver)


if __name__ == "__main__":
    unittest.main()
