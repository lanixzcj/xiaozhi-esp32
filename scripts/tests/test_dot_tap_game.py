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
#include "dot_round.h"
#include "dot_tap_session.h"
#include "dot_tilt.h"
#include "shake_detector.h"
#include <cassert>
#include <limits>
#include <cmath>
#include <initializer_list>
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

 // Realistic 3D gravity at flat, oblique and upright neutral poses. The
 // upright +/- pitch samples have identical Y but opposite Z: old XY-only
 // subtraction drives both toward the same edge. Require all four edges,
 // reversal, neutral return and resistance to acceleration magnitude changes.
 constexpr float pi=3.14159265358979323846f;
 auto raw=[](float x){return int16_t(std::lround(x*8192.f));};
 for(float hold : {0.f,30.f,60.f,85.f,90.f}) {
  float h=hold*pi/180.f, ny=std::sin(h), nz=std::cos(h);
  for(int axis=0;axis<2;++axis) {
   DotRoll moving; moving.NoteHome(2,128,88,true);
   for(int sign : {1,-1}) {
    float angle=sign*15.f*pi/180.f;
    float x=0,y=0,z=0;
    if(axis==0) { // Rotate around the low-side roll axis.
     x=-std::sin(angle);y=ny*std::cos(angle);z=nz*std::cos(angle);
    } else { // Pitch about screen-right; Z preserves the direction upright.
     y=ny*std::cos(angle)+nz*std::sin(angle);
     z=nz*std::cos(angle)-ny*std::sin(angle);
    }
    assert(DotRoll::MapRelative(raw(x),raw(y),raw(z),0,raw(ny),raw(nz),px,py));
    int active=axis==0?px:py, other=axis==0?py:px;
    assert(sign*active>350 && std::abs(other)<2);
    for(int i=0;i<100;++i) moving.Step(px,py,33);
    float position=axis==0?moving.body(2).x:moving.body(2).y;
    assert(sign>0 ? position>(axis==0?245:165) : position<10);
   }
   DotRoll::MapRelative(0,raw(ny),raw(nz),0,raw(ny),raw(nz),px,py);
   assert(px==0 && py==0);
   for(int i=0;i<150;++i) moving.Step(px,py,33);
   assert(std::abs(moving.body(2).x-128)<5 && std::abs(moving.body(2).y-88)<5);
  }
  DotRoll::MapRelative(0,raw(ny*.9f),raw(nz*.9f),0,raw(ny),raw(nz),px,py);
  assert(px==0 && py==0);
 }
 assert(!DotRoll::MapRelative(0,0,0,0,0,8192,px,py) && px==0 && py==0);
 assert(!DotRoll::MapRelative(0,0,20000,0,0,8192,px,py) && px==0 && py==0);
 DotRoll gentle;gentle.NoteHome(2,128,88,true);
 for(int i=0;i<60;++i) gentle.Step(250,0,33);
 assert(gentle.body(2).x > 225); // ~14 degree tilt reaches near the edge
 DotRoll reverse;reverse.NoteHome(2,128,88,true);
 for(int i=0;i<60;++i) reverse.Step(-250,0,33);
 assert(reverse.body(2).x < 35);

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

 // The entire local round must also be playable by taps alone.
 DotTapSession tap_only;tap_only.Start(0);
 assert(tap_only.Tap(1,2) && tap_only.Tap(2,0));
 assert(tap_only.page()==P::Mixed);
 assert(tap_only.Tap(3,0) && tap_only.page()==P::Mixed);
 assert(tap_only.Tap(4,1) && tap_only.page()==P::Mixed);
 assert(tap_only.Tap(5,4) && tap_only.page()==P::Scattering);
 assert(tap_only.Tick(5+S::kScatterMs));
 assert(tap_only.Tick(5+S::kScatterMs+S::kSettleMs));
 assert(tap_only.page()==P::Settled && tap_only.Tap(2000,-1));
 assert(tap_only.page()==P::One);

 DotRound round;
 const char* valid =
     "{\"gap_ms\":10,\"note\":\"hi\",\"dots\":["
     "{\"cx\":160,\"cy\":120,\"color\":\"yellow\",\"size\":64,\"order\":2},"
     "{\"cx\":70,\"cy\":60,\"color\":\"red\",\"size\":200,\"order\":0},"
     "{\"cx\":250.5,\"cy\":180,\"color\":\"blue\",\"size\":10,\"order\":1}]}";
 assert(DotRound::Parse(valid, round));
 assert(round.count == 3);
 assert(round.gap_ms == DotRound::kMinGapMs);
 assert(round.dots[0].color == DotRound::Color::Yellow);
 assert(round.dots[0].x == 160 - 32 && round.dots[0].y == 120 - 32);
 assert(round.dots[1].size == DotRound::kMaxSize);
 assert(round.dots[1].order == 0);
 assert(round.dots[2].size == DotRound::kMinSize);
 const char* wild =
     "{\"dots\":[{\"cx\":-40,\"cy\":900,\"color\":\"yellow\",\"size\":80}]}";
 assert(DotRound::Parse(wild, round));
 assert(round.count == 1);
 assert(round.dots[0].x >= DotRound::kMargin);
 assert(round.dots[0].y + round.dots[0].size <= DotRound::kScreenH - DotRound::kMargin);
 assert(!DotRound::Parse("not json", round));
 assert(!DotRound::Parse("{\"dots\":[]}", round));
 assert(!DotRound::Parse("{\"dots\":[{\"cx\":10,\"color\":\"green\"}]}", round));
 assert(!DotRound::Parse(
     "{\"dots\":[{},{},{},{},{},{\"cx\":1,\"cy\":1,\"color\":\"red\"}]}", round));
 assert(!DotRound::Parse("", round));

 DotRoundQueue queue;
 uint32_t id = queue.Begin(0);
 assert(id == 1 && queue.inflight());
 assert(!queue.Take(round));
 assert(queue.OnReply(id, valid));
 assert(queue.Has() && queue.Take(round) && round.count == 3);
 assert(!queue.OnReply(id, valid));
 id = queue.Begin(100);
 assert(!queue.TimedOut(100 + 1499));
 queue.Expire();
 assert(!queue.inflight() && queue.failures() == 1);
 assert(!queue.OnReply(id, valid));
 assert(!queue.Offer("{\"dots\":[{\"cx\":1,\"cy\":1,\"color\":\"purple\"}]}"));
 assert(queue.Offer(wild));
 assert(queue.Take(round) && round.dots[0].size == 80);

 DotTapSession custom;
 custom.Start(0);
 assert(custom.EnterCustom(0) && custom.page() == P::Custom);
 assert(custom.Tap(1, 0) && custom.page() == P::Custom);
 assert(custom.Shake(2) && custom.page() == P::Scattering);
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
        self.assertIn('Chip(layer, "点点点"', game)
        self.assertIn("kFallbackHoldMs", game)
        self.assertIn("dot_tap_sounds::Get", game)
        self.assertNotIn("Press Here", game)
        for forbidden in ("lv_screen_load", "lv_obj_clean", "lv_obj_delete", "nvs_", "esp_restart",
                          "LV_EVENT_LONG_PRESSED_REPEAT"):
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
