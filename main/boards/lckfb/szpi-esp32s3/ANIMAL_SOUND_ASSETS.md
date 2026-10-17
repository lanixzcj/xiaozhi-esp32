# Offline recorded animal sounds

The six sounds in `animal_game_sounds.h` come from real animal recordings.
Their source WAVs are committed in `assets/audio/` and are byte-for-byte
identical to the six approved, extended preview WAVs. The generator encodes
those files locally without downloading, synthesizing, trimming, normalizing,
changing pitch, or changing speed.

| Index | Animal | Embedded bytes | Source WAV duration | Packets | Firmware playback |
| --- | --- | ---: | ---: | ---: | ---: |
| 0 | Cat | 7,997 | 2.370000 s | 40 | 2.40 s |
| 1 | Dog | 6,997 | 2.038583 s | 35 | 2.10 s |
| 2 | Duck | 7,046 | 2.000000 s | 34 | 2.04 s |
| 3 | Chicken (adult hen) | 8,849 | 2.650000 s | 45 | 2.70 s |
| 4 | Cow | 8,134 | 2.400000 s | 41 | 2.46 s |
| 5 | Sheep | 7,255 | 2.094833 s | 36 | 2.16 s |

Total embedded sound data, including attribution metadata: **46,278 bytes**.
The separate WAVs are build inputs, not duplicate firmware data. The game uses
the fixed groups cat/dog/duck and hen/cow/sheep.

The sheep clip repeats the **same 0.8724-second real recorded bleat twice**,
with a **350 ms silent gap**. It is not an uninterrupted original recording
of two calls. The other five WAVs preserve one continuous source excerpt.
The chicken sound is an adult hen, not a chick or rooster.

## Recording credits and separate licenses

The repository's MIT software license does **not** replace these recordings'
licenses. The generator and C++ wrapper code are MIT licensed. Each audio
payload retains the license listed below. The duck WAV and embedded Opus
adaptation remain **CC BY-SA 3.0 Unported**.

| Audio | Original title and creator | Source | License |
| --- | --- | --- | --- |
| Cat | *Meow of a pleading cat*, Heismark | [Wikimedia Commons](https://commons.wikimedia.org/wiki/File:Meow_of_a_pleading_cat.oga) | [Public domain, PD-self](https://commons.wikimedia.org/wiki/File:Meow_of_a_pleading_cat.oga#Licensing) |
| Dog | *Dog barking mono*, Brandon Morris; uploaded by HaelDB | [OpenGameArt](https://opengameart.org/content/dog-barking-mono) | [CC0 1.0](https://creativecommons.org/publicdomain/zero/1.0/) selected from the offered licenses |
| Duck | *Anas platyrhynchos - Mallard - XC62258*, Jonathon Jongsma | [Wikimedia Commons](https://commons.wikimedia.org/wiki/File:Anas_platyrhynchos_-_Mallard_-_XC62258.ogg), [original XC62258](https://xeno-canto.org/62258) | [CC BY-SA 3.0 Unported](https://creativecommons.org/licenses/by-sa/3.0/) |
| Adult hen | *Hens Lays*, Joseph SARDIN | [BigSoundBank](https://bigsoundbank.com/hens-lays-s0978.html) | [CC0 1.0](https://creativecommons.org/publicdomain/zero/1.0/) |
| Cow | *Cow Moos #1*, Joseph SARDIN | [BigSoundBank](https://bigsoundbank.com/vache-qui-meugle-s0546.html) | [CC0 1.0](https://creativecommons.org/publicdomain/zero/1.0/) |
| Sheep | *Sheep Baa*, mikewest (original recording); AntumDeluge (edited one-shot) | [OpenGameArt](https://opengameart.org/content/sheep-baa), [original Freesound recording](https://freesound.org/people/mikewest/sounds/414342/) | [CC0 1.0](https://creativecommons.org/publicdomain/zero/1.0/) |

Source verification dates, original download URLs and hashes, exact WAV hashes,
recording evidence, and adaptation details are preserved verbatim in
`assets/audio/recording-provenance.json`. Its historical `opus_sha256` fields
refer to the earlier browser-preview encoding, **not** this firmware encoding.
The authoritative firmware hashes and measured output are in
`assets/audio/firmware-encoding-report.json`.

The already-prepared WAVs use a 60 Hz high-pass, 12 ms fade-in, 18 ms fade-out,
constant gain, and mono 24 kHz PCM16. Excerpt ranges are cat 9.25–11.62 s, dog
0–2.038594 s, duck 0.98–2.98 s, hen 11.45–14.10 s, and cow 2.90–5.30 s. Sheep
uses the complete edited one-shot twice with the stated gap. Firmware encoding
adds lossy Opus compression and container/codec padding only.

In particular, the duck adaptation is based on Jonathon Jongsma's XC62258:
excerpt 0.98–2.98 s, high-pass, fades, -7.44 dB constant gain, mono/resampling,
and Opus encoding. It is offered under the same CC BY-SA 3.0 license. The
original creators have not endorsed this firmware.

Ship this document and the following notices with firmware distributions:

- `licenses/audio-CC-BY-SA-3.0-NOTICE.txt`: duck attribution, license URI,
  modification notice, and link to the complete legal terms
- `licenses/audio-PUBLIC-DOMAIN-NOTICE.txt`: public-domain/CC0 credits and sources
- `licenses/audio-CC0-1.0.txt`: complete CC0 legal text

Each Ogg stream also carries its title, author, source and license in OpusTags.
The duck tags additionally retain the original source, excerpt, changes and
share-alike notice. Do not label these recordings collectively as MIT or
remove their credit/license information when distributing embedded versions.

## Playback API and bounded format

- `animal_game_assets::kSoundCount` is 6.
- `GetSound(index)` returns a `std::string_view` over flash-resident Ogg data;
  an out-of-range index returns an empty view.
- Mono Ogg Opus, **24,000 Hz** source and `OpusHead` input rate, 24 kbit/s CBR,
  **60 ms** audio packets. Each audio packet is **180 bytes**. The largest
  OpusTags packet is **761 bytes**. Both fit the demuxer's 2,048-byte buffer.
- Ogg/Opus granules use 48 kHz units. `ffprobe` therefore reports a 48 kHz
  stream, while the firmware reads 24 kHz from `OpusHead`, configures the
  decoder accordingly, and uses its existing output resampler when needed.
- The firmware demuxer does not trim the 6.5 ms codec pre-skip or last-packet
  padding. The table reports the full untrimmed playback durations. All
  approved source samples are retained. The longest cue is 2.70 seconds.
- Local playback admission allows at most 24 KiB, 50 packets, and 3,000 ms.
  Clips are admitted atomically only into an idle local-interaction playback
  pipeline. Oversized, too-long, malformed, truncated, or busy requests are
  rejected rather than played partially. Network buffering limits are separate.

## Offline reproduction and validation

Requirements: Python 3 standard library, FFmpeg with libopus, and ffprobe.
No Python packages or network access are needed. From the repository root:

```sh
python3 main/boards/lckfb/szpi-esp32s3/tools/generate_animal_sounds.py
python3 main/boards/lckfb/szpi-esp32s3/tools/generate_animal_sounds.py --check
python3 -m unittest discover -s scripts/tests -p test_animal_recordings.py -v
```

`--check` validates approved input hashes and exact PCM frame counts, regenerates
all six encodings, and compares the complete generated header without modifying
it. Validation includes Ogg checksums, page sequencing, continuation, end marker,
mono/rate headers, 60 ms packet durations, every packet's buffer limit, source
length encoded in the final granule, complete FFmpeg decoding, exact decoded
frame count, padded playback length, and decoded peak at most 0.56 full scale.
That peak limit permits Opus overshoot above the supplied dog's -6 dBTP source;
it is not a volume normalization operation.

To retain exact WAV copies and encoded previews outside the repository:

```sh
python3 main/boards/lckfb/szpi-esp32s3/tools/generate_animal_sounds.py \
  --check --preview-dir /tmp/animal-game-audio-preview
```

The committed output was generated with Python 3.12.14, FFmpeg
7.1.5-0+deb13u1, and libopus 1.5.2. Repeated runs with this toolchain produce
byte-identical output. Other encoder versions can change the output; inspect
and revalidate those changes rather than bypassing `--check`.

## Verified results

All six WAV hashes match the supplied approved preview. Repeated generation
and `--check` match the committed header and encoding report exactly. FFmpeg
decodes the exact source frame counts, including all 63,600 frames of the hen
and both sheep calls with their original gap.

| Cue | FFmpeg decoded peak | FFmpeg RMS | Firmware-style libopus peak |
| --- | ---: | ---: | ---: |
| Cat | -10.93 dBFS | -26.47 dBFS | -10.94 dBFS |
| Dog | -5.70 dBFS | -26.02 dBFS | -5.70 dBFS |
| Duck | -11.31 dBFS | -28.15 dBFS | -11.74 dBFS |
| Hen | -12.83 dBFS | -31.86 dBFS | -12.59 dBFS |
| Cow | -11.24 dBFS | -25.42 dBFS | -11.24 dBFS |
| Sheep | -7.53 dBFS | -27.16 dBFS | -7.54 dBFS |

The new host tests compile the actual `main/audio/demuxer/ogg_demuxer.cc` and
extract the actual `AudioService::PlayLocalInteractionSound` admission method;
only ESP platform primitives are stubbed. All six embedded streams are parsed
whole and in 1-byte/17-byte chunks, with `Finish()` verified. The host libopus
decoder produces exactly 1,440 mono PCM samples per packet at 24 kHz, including
the complete untrimmed 64,800-frame hen stream. Source hashes, sheep repetition
and exact 8,400-frame silent gap, embedded duck credits, out-of-range indexes,
reproducibility, and damaged-file rejection are covered.

Admission tests verify all six clips, exact 50-packet/3,000 ms acceptance,
3,060 ms rejection, 51 short packets rejected independently of duration,
rejection while decode/output is busy, no duplicate/queued tap, malformed or
truncated streams, and no partial enqueue on failure.

These host tests do not run the ESP hardware decoder or validate the actual
speaker, output resampler, stopping latency, or subjective listening quality.
Audition all six on the board at a comfortable low volume before child use.
Digital headroom alone does not establish a safe acoustic sound level.
