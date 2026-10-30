# Listen-and-pick games

Two separate games for a parent and a baby about one year old. The device plays a
sound and the child taps a picture. There is no speech recognition, score, or
visible timer. This branch is source development, not a hardware-accepted
firmware. A host test or a successful build does not replace speaker and touch
checks on the Lichuang ESP32-S3 board.

The transport entry is **交通** (x = 240). The animal entry is **动物** (x = 160).
Both sit on the top layer, including the Wi-Fi configuration screen. Only one
game can own local audio at a time.

## Shared round

`ListenPickSession` is the policy. The screen code is `ListenPickTouch`.

1. The round starts by playing the target sound. Two large pictures are already
   visible. The child does not have to tap Listen first.
2. Taps during playback are ignored and are not queued.
3. A correct tap enlarges that picture, draws a green rim, and plays a short
   original chime (`listen_pick_cheer.h`, not a voice). When the chime and the
   short celebration have finished, the next prompt starts on its own.
4. A wrong tap does not mark the picture or play a buzzer. The same prompt plays
   again and the two pictures stay. **再听一次** replays it any time a sound is
   not already playing.
5. The answer side alternates. The other picture is always a different item.
6. After every item has been the target once, the game shows **休息一下吧**.
   A parent holds **家长长按再玩** for 1.5 seconds and releases to start again.
   A short tap does not restart.
7. **返回** or BOOT leaves at any time, stops playback, and restores the previous
   conversation state. Provisioning or upgrade cancellation also leaves.
8. If a game is left open for three minutes it rests on its own so the microphone
   can return. Nothing on screen counts down, and there is no score.

No network fetch, NVS writes, bootloader, partitions, pins, security settings,
automatic OTA, or backup uploads are added for these games.

## 交通

Car, modern train, bicycle, propeller plane. Pictures and field recordings are
described in `assets/transport/README.md`. Four rounds, then rest.

## 动物

Cat, dog, duck, hen, cow, sheep. Each round shows two of the existing real photos
(`assets/photos/`, 296×162 RGB565, scaled to fit the card) and plays that animal's
existing recording. Six rounds, then rest. The rest screen can say
**和爸爸妈妈说说小动物**. The older tap-to-hear sequence and the photo/cartoon
toggle are not part of this game; the cartoon drawing sources remain in the tree
but are not shown.

## Verification

```sh
python3 -m unittest discover -s scripts/tests -v
```

`test_animal_recordings.test_regeneration_and_full_lengths` can fail when the
local FFmpeg build does not match the bytes checked in earlier. That check is
independent of this interaction.

Policy tests cover both answer sides, distinct pictures, every animal as a
target, wrong-answer retry, no advance while audio is still playing, explicit
replay, rest, the three-minute safety rest, and tick wrap. The cheer bytes are
checked as mono 24 kHz Opus, 60 ms packets, under one second.

A desktop LVGL build of the animal screen (photos, font, and the shared overlay):

```sh
cmake -S main/boards/lckfb/szpi-esp32s3/tools/host_ui -B /tmp/listen-pick-ui \
  -DLVGL_SOURCE_DIR=/path/to/lvgl-v9.5.0
cmake --build /tmp/listen-pick-ui -j 4
mkdir -p /tmp/listen-pick-ui-images
/tmp/listen-pick-ui/check /tmp/listen-pick-ui-images
```

Firmware, ESP-IDF 6.1:

```sh
python scripts/build.py lckfb/szpi-esp32s3 --name lichuang-dev
```

Before flashing, listen at a low volume and check: both pictures are easy to hit,
a correct tap cheers and grows, a wrong tap only replays the prompt, **再听一次**
and **返回** work during playback and while choosing, BOOT leaves, the other game
cannot start underneath, wake and press-to-talk return after exit, and Wi-Fi
provisioning can open and close the games. Confirm all four vehicle recordings
and all six animal calls on the speaker. The touch controller on this board is
the FT5x06.
