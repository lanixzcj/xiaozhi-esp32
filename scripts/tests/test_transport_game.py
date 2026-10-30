"""Host tests for transport matching, bounded playback and overlay integration."""
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path
ROOT = Path(__file__).resolve().parents[2]
BOARD = ROOT / 'main/boards/lckfb/szpi-esp32s3'

class TransportGameTests(unittest.TestCase):
    @unittest.skipUnless(shutil.which('g++'), 'requires g++')
    def test_policy(self):
        code = r'''
#include "transport_game_session.h"
#include <cassert>
#include <limits>
int main() {
 using S = TransportGameSession; using P = S::Phase;
 S s;
 assert(s.phase() == P::Closed);
 assert(!s.Listen(0) && !s.Choose(0, 0));
 for (size_t seed = 0; seed < 8; ++seed) {
  uint32_t now = 100; s.Start(now, seed);
  for (size_t round = 0; round < S::kCount; ++round) {
   assert(s.phase() == P::Ready && s.round() == round);
   assert(s.choice(0) < S::kCount && s.choice(1) < S::kCount);
   assert(s.choice(0) != s.choice(1));
   assert(!s.Choose(now, 0));
   assert(!s.Tick(now + 1, true)); // no auto sound/advance
   assert(s.Listen(now)); assert(!s.Listen(now));
   assert(!s.Choose(now, 0) && !s.Tick(now + 1, false));
   assert(s.Tick(now + 3000, true)); now += 3000;
   assert(s.phase() == P::Choosing && !s.Choose(now, 2));
   const size_t answer = s.choice(0) == s.target() ? 0 : 1;
   assert(s.Choose(now, 1 - answer)); assert(!s.correct());
   assert(!s.Choose(now, answer));
   assert(!s.Tick(now + 899, true));
   assert(!s.Tick(now + 900, false)); // wrong-answer replay still playing
   assert(s.Tick(now + 900, true)); now += 900;
   assert(s.phase() == P::Choosing && s.round() == round);
   assert(s.Listen(now)); // explicit replay
   assert(s.Tick(now + 3000, true)); now += 3000;
   assert(s.Choose(now, answer)); assert(s.correct());
   assert(!s.Tick(now + 900, false)); // cheer still playing
   assert(s.Tick(now + 900, true)); now += 900;
  }
  assert(s.phase() == P::Rest);
  assert(!s.Listen(now) && !s.Choose(now, 0) && !s.Tick(now + 100000, true));
 }
 s.Start(0); assert(s.Listen(0));
 assert(s.Tick(S::kLimitMs, false)); assert(s.phase() == P::Rest);
 s.Start(0); assert(!s.Listen(S::kLimitMs));
 assert(s.Tick(S::kLimitMs, true));
 constexpr uint32_t start = std::numeric_limits<uint32_t>::max() - 50;
 s.Start(start); assert(s.Listen(1)); assert(s.Tick(2, true));
 assert(s.Choose(3, s.choice(0) == s.target() ? 0 : 1));
 assert(s.Tick(903, true));
 assert(s.Tick(uint32_t(start + S::kLimitMs), true));
 s.Close(); assert(s.phase() == P::Closed && !s.Tick(10000, true));
}
'''
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp)
            (path / 'test.cc').write_text(code)
            subprocess.run(['g++', '-std=c++17', '-Wall', '-Wextra', '-Werror', '-I', str(BOARD), str(path / 'test.cc'), '-o', str(path / 'test')], check=True)
            subprocess.run([str(path / 'test')], check=True)

    def test_overlay_guards(self):
        src = (BOARD / 'listen_pick_touch.cc').read_text()
        for guard in ['generation != generation_', 'round != session_.round()', 'action_pending_.exchange(true)', 'poll_pending_.exchange(true)', 'app.IsLocalInteractionActive() || !app.BeginLocalInteraction()', 'EndLocalInteraction()', 'LV_EVENT_SHORT_CLICKED', 'listen_pick_cheer::Get()', 'kParentHoldMs']:
            self.assertIn(guard, src)
        for name in ('listen_pick_touch.cc', 'transport_touch_game.cc', 'animal_touch_game.cc'):
            text = (BOARD / name).read_text()
            for forbidden in ['lv_screen_load', 'lv_obj_clean', 'LV_EVENT_PRESSING', 'LV_EVENT_LONG_PRESSED_REPEAT', 'nvs_', 'esp_restart']:
                self.assertNotIn(forbidden, text, name)
        transport = (BOARD / 'transport_touch_game.cc').read_text()
        animal = (BOARD / 'animal_touch_game.cc').read_text()
        self.assertIn('"交通", 240', transport)
        self.assertIn('"动物", 160', animal)
        self.assertIn('animal_game_photo', animal)
        board = (BOARD / 'lichuang_dev_board.cc').read_text()
        self.assertIn('transport_game_->RequestExit()', board)
        self.assertEqual(board.count('(transport_game_ && transport_game_->IsVisible())'), 4)
        self.assertEqual(board.count('(animal_game_ && animal_game_->IsVisible())'), 4)

if __name__ == '__main__':
    unittest.main()
