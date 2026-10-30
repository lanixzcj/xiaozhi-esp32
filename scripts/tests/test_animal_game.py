"""Host regression tests for the animal listen-and-pick session and assets."""

import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
BOARD = ROOT / "main/boards/lckfb/szpi-esp32s3"


class AnimalGameSessionTests(unittest.TestCase):
    @unittest.skipUnless(shutil.which("g++"), "g++ is required for the C++ session test")
    def test_listen_and_pick_policy(self):
        source = r'''
#include "animal_game_session.h"
#include <cassert>
#include <cstdint>
#include <limits>

int main() {
    using Session = AnimalGameSession;
    using Phase = Session::Phase;
    Session game;
    assert(game.phase() == Phase::Closed);
    assert(Session::kCount == 6);
    assert(!game.Listen(0) && !game.Choose(0, 0));
    assert(!game.Tick(100000, true));

    bool asked[Session::kCount] = {};
    for (size_t seed = 0; seed < Session::kCount; ++seed) {
        uint32_t now = 1000;
        game.Start(now, seed);
        size_t answers_on_left = 0;
        for (size_t round = 0; round < Session::kCount; ++round) {
            assert(game.phase() == Phase::Ready);
            assert(game.round() == round);
            assert(game.target() == (seed + round) % Session::kCount);
            asked[game.target()] = true;
            assert(game.choice(0) != game.choice(1));
            assert(game.choice(0) < Session::kCount && game.choice(1) < Session::kCount);
            const size_t answer = game.choice(0) == game.target() ? 0 : 1;
            if (answer == 0) ++answers_on_left;
            // Ready does not play or advance by itself.
            assert(!game.Tick(now + 1, true));
            assert(!game.Choose(now, answer));
            assert(game.Listen(now));
            assert(!game.Listen(now));
            assert(!game.Choose(now, answer));
            assert(!game.Tick(now + 1, false));
            assert(game.Tick(now + 2700, true));
            now += 2700;
            assert(game.phase() == Phase::Choosing);
            assert(!game.Choose(now, 2));
            assert(game.Choose(now, 1 - answer));
            assert(!game.correct());
            assert(!game.Choose(now, answer));
            assert(!game.Tick(now + Session::kFeedbackMs, false));
            assert(game.Tick(now + Session::kFeedbackMs, true));
            now += Session::kFeedbackMs;
            assert(game.phase() == Phase::Choosing && game.round() == round);
            assert(game.Listen(now));
            assert(game.Tick(now + 2700, true));
            now += 2700;
            assert(game.Choose(now, answer));
            assert(game.correct());
            assert(!game.Tick(now + Session::kFeedbackMs, false));
            assert(game.Tick(now + Session::kFeedbackMs, true));
            now += Session::kFeedbackMs;
        }
        assert(game.phase() == Phase::Rest);
        assert(answers_on_left == Session::kCount / 2);
        assert(!game.Listen(now) && !game.Choose(now, 0));
        assert(!game.Tick(now + 100000, true));
    }
    for (bool seen : asked) assert(seen);

    game.Start(0);
    assert(game.Listen(0));
    assert(game.Tick(Session::kLimitMs, false));
    assert(game.phase() == Phase::Rest);
    game.Start(0);
    assert(!game.Listen(Session::kLimitMs));
    assert(game.Tick(Session::kLimitMs, true));
    assert(game.phase() == Phase::Rest);

    constexpr auto near_wrap = std::numeric_limits<uint32_t>::max() - 50;
    game.Start(near_wrap);
    assert(game.Listen(1));
    assert(game.Tick(2, true));
    const size_t answer = game.choice(0) == game.target() ? 0 : 1;
    assert(game.Choose(3, answer));
    assert(!game.Tick(3 + Session::kFeedbackMs, false));
    assert(game.Tick(3 + Session::kFeedbackMs, true));
    assert(game.round() == 1 && game.phase() == Phase::Ready);
    assert(game.Tick(uint32_t(near_wrap + Session::kLimitMs), true));
    assert(game.phase() == Phase::Rest);
    game.Close();
    assert(game.phase() == Phase::Closed);
    assert(!game.Tick(10000, true));
}
'''
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)
            (path / "test.cc").write_text(source)
            subprocess.run(
                ["g++", "-std=c++17", "-Wall", "-Wextra", "-Werror",
                 "-I", str(BOARD), str(path / "test.cc"), "-o", str(path / "test")],
                check=True,
            )
            subprocess.run([str(path / "test")], check=True)

    def test_touch_is_release_only_and_nonrepeating(self):
        source = (BOARD / "listen_pick_touch.cc").read_text()
        self.assertIn("LV_EVENT_SHORT_CLICKED", source)
        self.assertNotIn("LV_EVENT_LONG_PRESSED_REPEAT", source)
        self.assertNotIn("LV_EVENT_PRESSING", source)
        self.assertNotIn("lv_obj_delete", source)
        self.assertNotIn("lv_screen_load", source)
        self.assertNotIn("lv_obj_clean", source)
        self.assertIn("poll_pending_.exchange(true)", source)
        self.assertIn("kParentHoldMs", source)
        self.assertNotIn("ToggleVisualMode", source)
        self.assertNotIn("photo_mode_", source)

    def test_cheer_ogg_fits_local_playback(self):
        import sys
        sys.path.insert(0, str(BOARD / "tools"))
        from generate_animal_sounds import ogg_packets, packet_duration_ms
        text = (BOARD / "listen_pick_cheer.h").read_text()
        blob = text.split("kOgg[] = {", 1)[1].split("};", 1)[0]
        data = bytes(int(token, 16) for token in blob.split(",") if token.strip())
        packets = ogg_packets(data)
        self.assertEqual(packets[0][:8], b"OpusHead")
        self.assertEqual(packets[0][9], 1)
        self.assertEqual(int.from_bytes(packets[0][12:16], "little"), 24000)
        self.assertTrue(all(packet_duration_ms(packet) == 60 for packet in packets[2:]))
        playback_ms = len(packets[2:]) * 60
        self.assertGreaterEqual(playback_ms, 300)
        self.assertLessEqual(playback_ms, 1000)
        self.assertLess(len(data), 8 * 1024)
        self.assertIn("listen_pick_cheer::Get()", (BOARD / "listen_pick_touch.cc").read_text())

    def test_six_photo_contract(self):
        import hashlib
        import json
        ui = (BOARD / "listen_pick_touch.cc").read_text()
        game = (BOARD / "animal_touch_game.cc").read_text()
        self.assertIn("animal_game_photo", game)
        self.assertIn("AnimalGameSession::kCount", game)
        self.assertIn("lv_obj_set_style_clip_corner(cards_[side], true", ui)
        self.assertNotIn("ToggleVisualMode", game)
        self.assertNotIn("session_.group()", game)
        data = json.loads((BOARD / "assets/photos/provenance.json").read_text())
        self.assertEqual([x["animal"] for x in data], ["cat", "dog", "duck", "chicken", "cow", "sheep"])
        for item in data:
            original = BOARD / "assets/photos" / (item["animal"] + ".jpg")
            self.assertEqual(hashlib.sha256(original.read_bytes()).hexdigest(), item["sha256"])
            self.assertTrue(item["license_url"].startswith("https://"))
        report = json.loads((BOARD / "assets/photos/processing.json").read_text())
        self.assertEqual(len(report), 6)
        self.assertEqual(sum(x["bytes"] for x in report), 6 * 296 * 162 * 2)
        self.assertIn("LV_COLOR_FORMAT_RGB565", (BOARD / "animal_game_photos.c").read_text())

    def test_offline_font_has_board_lifetime(self):
        source = (BOARD / "listen_pick_touch.cc").read_text()
        self.assertIn("LV_FONT_DECLARE(animal_game_font)", source)
        self.assertNotIn("GetTextFont", source)
        self.assertNotIn("GetTheme", source)
        self.assertIn("&animal_game_font", source)
        font = (BOARD / "animal_game_font.c").read_text()
        for character in "小猫狗鸭母鸡牛羊照片卡通动物返回休息家长再玩":
            self.assertIn(f"U+{ord(character):04X}", font)


if __name__ == "__main__":
    unittest.main()
