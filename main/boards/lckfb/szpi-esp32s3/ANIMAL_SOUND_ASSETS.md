# Offline animal sound assets

`animal_game_sounds.h` contains three short, original synthesized animal-like
calls, in the same order as the game's animals:

| Index | Cue | Embedded bytes | Source duration | Firmware playback duration |
| --- | --- | ---: | ---: | ---: |
| 0 | Cat, one gentle meow | 2,320 | 0.66 s | 0.72 s |
| 1 | Dog, one soft woof | 1,596 | 0.42 s | 0.48 s |
| 2 | Duck, one rounded quack | 1,596 | 0.42 s | 0.48 s |

Total encoded sound data: **5,512 bytes**. No recordings, downloaded samples,
third-party sound libraries, external generation services, or runtime network
requests are used. The cues are playful synthetic approximations of animals,
not recordings of actual animals.

## Provenance and license

All waveforms were generated specifically for this project by
`tools/generate_animal_sounds.py`. It combines voiced sine harmonics, moving
formants, and seeded low-pass noise. Every clip contains a single call, a smooth
attack and release, and a silent lead-in/tail. Source peaks are normalized to
0.19 full scale (approximately -14.4 dBFS).

The generator, generated header, and original synthesized waveforms are
distributed under this repository's MIT license; see the root `LICENSE`.
FFmpeg/libopus are local encoding tools, not sources of recorded sound.

## Playback API and format

- `animal_game_assets::kSoundCount` is 3.
- `animal_game_assets::GetSound(index)` returns a `std::string_view` over
  flash-resident Ogg bytes. An out-of-range index returns an empty view.
- Encoding: mono Ogg Opus, 16,000 Hz source/`OpusHead` input rate, 24 kbit/s CBR,
  60 ms packets. Each encoded audio packet is 180 bytes, below the demuxer's
  2,048-byte packet limit.
- Cat has 12 packets; dog and duck have 8 packets each. The packet count includes
  encoder delay and final padding. The firmware demuxer does not trim Ogg
  pre-skip or the last granule, so the table reports its full decoded duration.
- Standard Opus container timestamps use 48 kHz; `ffprobe` therefore reports
  48 kHz for the stream. The firmware reads the 16 kHz input rate from `OpusHead`
  and decodes at 16 kHz before its normal output resampling.

## Reproduce or verify

From the repository root:

```sh
python3 main/boards/lckfb/szpi-esp32s3/tools/generate_animal_sounds.py
python3 main/boards/lckfb/szpi-esp32s3/tools/generate_animal_sounds.py --check
```

Requirements: Python 3 standard library, `ffmpeg` with `libopus`, and `ffprobe`.
No Python packages or network access are needed. A fixed noise seed and FFmpeg
bit-exact flags make repeated runs byte-identical with the same toolchain.
The checked-in assets were generated with Python 3.12.14, FFmpeg
7.1.5-0+deb13u1, and libopus 1.5.2. Other encoder versions may legitimately
produce different bytes and should be revalidated before committing.

To retain WAV/OGG files for listening without adding them to the firmware:

```sh
python3 main/boards/lckfb/szpi-esp32s3/tools/generate_animal_sounds.py \
  --check --preview-dir /tmp/animal-game-audio-preview
```

The script verifies mono and input-rate headers, 60 ms packet durations,
packet sizes, padded playback duration, successful FFmpeg decoding, and a
decoded peak below 0.23 full scale. It prints sizes, durations, peaks, RMS
levels, and SHA-256 hashes. `--check` compares the complete generated header
with the checked-in file without overwriting it.

## Validation results

Repeated generation matched byte-for-byte. `ffprobe` identified all three as
mono Opus; container durations were 0.6665 s, 0.4265 s, and 0.4265 s. FFmpeg
decoded all clips without errors. Its decoded peak/RMS levels were:

| Cue | Peak | RMS |
| --- | ---: | ---: |
| Cat | -14.22 dBFS | -20.80 dBFS |
| Dog | -14.45 dBFS | -26.23 dBFS |
| Duck | -14.45 dBFS | -25.13 dBFS |

A host C++17 test compiled the header and the actual
`main/audio/demuxer/ogg_demuxer.cc`, replacing only ESP logging with no-op
macros. It parsed the assets both whole and in 1-byte/17-byte chunks, verified
`Finish()`, and decoded every packet using libopus at 16 kHz mono. All packets
produced 960 PCM samples (60 ms), with untrimmed peaks no higher than
-14.21 dBFS. Out-of-range API indexes returned empty views.

These host checks do not replace testing the ESP decoder, output resampler,
speaker, or perceived sound on the actual board. Verify each animal's cue and
set a comfortable speaker volume before child use. Digital headroom alone
does not establish a safe acoustic sound level.
