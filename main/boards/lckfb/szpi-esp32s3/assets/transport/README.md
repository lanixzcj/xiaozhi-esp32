# Transport game assets

Prepared 2026-10-23. Index order: car, modern train, bicycle, propeller plane.
These are real field recordings, not synthesized sounds. The steam train
recording/image from the earlier candidate set is deliberately NOT used.

## Sources and licenses

All recordings are CC0 1.0 per their individual BigSoundBank source pages and
https://bigsoundbank.com/licenses.html (verified 2026-10-23 for the new train).
Author credit is retained for traceability, not a license condition.

| File | Author | Source page | Original download |
|---|---|---|---|
| car-real-0258.wav | DenisChardonnet | https://bigsoundbank.com/recent-car-horn-2-s0258.html | https://bigsoundbank.com/UPLOAD/bwf-en/0258.wav |
| train-modern-2846.wav | Joseph SARDIN | https://bigsoundbank.com/train-horn-2-s2846.html | https://bigsoundbank.com/UPLOAD/bwf-en/2846.wav |
| bicycle-1042.wav | Joseph SARDIN | https://bigsoundbank.com/bell-bike-3-s1042.html | https://bigsoundbank.com/UPLOAD/bwf-en/1042.wav |
| airplane-0501.wav | Joseph SARDIN | https://bigsoundbank.com/small-airplane-moving-away-s0501.html | https://bigsoundbank.com/UPLOAD/bwf-en/0501.wav |

The train source identifies a stationary modern construction train recorded
outdoors, not a steam whistle. The stylized electric-train illustration is a
category cue, not a claim that the recording depicts that particular model.

| File | Author | Source / license |
|---|---|---|
| car-141901.png | jimmyboy99 | https://openclipart.org/detail/141901 |
| bicycle-169493.png | hatalar205 | https://openclipart.org/detail/169493 |
| airplane-small-324034.png | klsgfx | https://openclipart.org/detail/324034 |
| train-modern.svg | Original artwork created for this project | Dedicated to CC0 1.0, https://creativecommons.org/publicdomain/zero/1.0/ |

Openclipart artwork uses CC0 1.0: https://openclipart.org/share . The three
approved PNGs are preserved byte-for-byte. Original train art has no third-party
reference image. All four small output previews have been visually inspected.
The PNG airplane includes its original background; it is intentionally retained.

## Rebuild

From the repository root:

    python3 main/boards/lckfb/szpi-esp32s3/tools/generate_transport_assets.py
    python3 main/boards/lckfb/szpi-esp32s3/tools/generate_transport_assets.py --check

Requires Python 3, Pillow 12.3.0, Inkscape 1.4 and FFmpeg/libopus (tested FFmpeg
7.1.5). The tool only reads committed originals; it does not download anything.
Generated transport_game_assets.cc stores immutable flash-resident bytes and
LVGL 9 descriptors. Every image is 140×130 RGB565, little-endian, cream #FFF8EA
background, 280-byte stride. Total image storage: 145600 bytes. No runtime image
decoding, heap allocation, filesystem, PSRAM image cache, or partition change.

Audio is resampled to mono 24 kHz PCM16, fixed peak gain to -7 dBFS, 12 ms
fade-in and 25 ms fade-out, then encoded as 24 kbps CBR Ogg Opus using 60 ms
packets. No pitch/speed changes, concatenation or synthesis. Contiguous excerpts:
car 0–1.607 s, train 0–2.4 s, bicycle 0–1.955 s, plane 1–3.5 s (2.5 seconds).
Car/bicycle end trims are sub-millisecond rounding; the train tail is mostly
reverberation. Opus decode can overshoot input peak; decoded peaks are tested.

`previews/` contains exact converted WAVs, encoded Ogg files and small PNGs for
review, not runtime-loaded files. `validation.json` records sizes, padded packet
playback durations, peaks and encoded SHA256. Generator validates Ogg CRC,
sequence/continuation, mono/24 kHz headers, 60 ms packets, <=2048-byte packets,
<=3-second padded playback and decoded peak <0.56. Total encoded audio: 26721
bytes; total raw image/audio data: 172321 bytes (plus descriptors/font/code).
`SHA256SUMS` records original hashes. Hardware speaker listening, two-card visual
legibility, audio identification by children, and final firmware flash budget
must still be validated; technical checks are not a listening test.

The Chinese font remains `animal_game_font`, now with the transport UI glyphs.
It was regenerated with Noto Sans CJK SC Regular Version 2.004 from the official
Sans2.004 tag and lv_font_conv 1.5.3; the existing OFL license remains applicable.
