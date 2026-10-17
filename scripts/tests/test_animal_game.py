"""Host regression tests for the hardware-independent three-round policy."""

import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
BOARD = ROOT / "main/boards/lckfb/szpi-esp32s3"


class AnimalGameSessionTests(unittest.TestCase):
    @unittest.skipUnless(shutil.which("g++"), "g++ is required for the C++ session test")
    def test_session_policy(self):
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
    assert(!game.AcceptTap(0));
    assert(!game.Tick(100000, true));

    game.Start(100);
    for (size_t animal = 0; animal != 3; ++animal) {
        auto time = uint32_t(100 + animal * 1000);
        assert(game.phase() == Phase::Ready);
        assert(game.animal() == animal);
        // No automatic sound/advance while waiting for a tap.
        assert(!game.Tick(time + 100, true));
        assert(game.AcceptTap(time + 100));
        assert(game.phase() == Phase::Feedback);
        assert(!game.AcceptTap(time + 101)); // no tap queue
        assert(!game.Tick(time + 899, true)); // minimum feedback
        assert(!game.Tick(time + 900, false)); // audio still playing
        assert(game.Tick(time + 900, true));
    }
    assert(game.phase() == Phase::Rest);
    assert(!game.AcceptTap(4000));
    assert(!game.Tick(1000000, true)); // never restarts itself
    game.Start(5000); // explicit parent restart
    assert(game.animal() == 0);
    assert(game.phase() == Phase::Ready);
    game.Close();
    assert(!game.AcceptTap(5100));

    // Time cap covers inactivity and a stuck playback pipeline.
    game.Start(20);
    assert(!game.CanTap(20 + Session::kSessionLimitMs));
    assert(game.Tick(20 + Session::kSessionLimitMs, false));
    assert(game.phase() == Phase::Rest);
    game.Start(20);
    assert(game.AcceptTap(21));
    assert(game.Tick(20 + Session::kSessionLimitMs, false));
    assert(game.phase() == Phase::Rest);

    // LVGL tick wrap (roughly every 49 days) cannot create an endless session.
    constexpr auto near_wrap = std::numeric_limits<uint32_t>::max() - 10;
    game.Start(near_wrap);
    assert(game.AcceptTap(2));
    assert(!game.Tick(801, true));
    assert(game.Tick(802, true));
    assert(game.animal() == 1);
    assert(game.Tick(uint32_t(near_wrap + Session::kSessionLimitMs), true));
    assert(game.phase() == Phase::Rest);
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
        source = (BOARD / "animal_touch_game.cc").read_text()
        self.assertIn("LV_EVENT_SHORT_CLICKED", source)
        self.assertNotIn("LV_EVENT_LONG_PRESSED_REPEAT", source)
        self.assertNotIn("LV_EVENT_PRESSING", source)
        self.assertNotIn("lv_obj_delete", source)
        self.assertNotIn("lv_screen_load", source)
        self.assertNotIn("lv_obj_clean", source)
        self.assertIn("poll_pending_.exchange(true)", source)

    def test_offline_font_has_board_lifetime(self):
        source = (BOARD / "animal_touch_game.cc").read_text()
        self.assertIn("LV_FONT_DECLARE(animal_game_font)", source)
        self.assertNotIn("GetTextFont", source)
        self.assertNotIn("GetTheme", source)
        self.assertIn("&animal_game_font", source)
        font = (BOARD / "animal_game_font.c").read_text()
        for character in "小猫狗鸭动物返回休息家长再玩":
            self.assertIn(f"U+{ord(character):04X}", font)


if __name__ == "__main__":
    unittest.main()
