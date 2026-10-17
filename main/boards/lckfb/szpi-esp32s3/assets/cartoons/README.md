# Original farm cartoon sources

`chicken.svg`, `cow.svg`, and `sheep.svg` are the unchanged, original 320 × 240
browser-preview drawings. The hen, calf, and lamb artwork was authored for this
animal game and is available under the repository's MIT license. No external
animal imagery or image-generation service was used.

The full preview SVGs also contain Chinese text converted to outlines from Noto
Sans CJK SC. The font's SIL Open Font License is retained in
`../../licenses/animal_game_font-OFL.txt`. Those label groups are excluded from the
cartoon pixel conversion; firmware labels use the existing game font.

## Offline regeneration

From any working directory, run the following script (adjust its path as needed):

```sh
python3 main/boards/lckfb/szpi-esp32s3/tools/generate_animal_cartoons.py --check
python3 main/boards/lckfb/szpi-esp32s3/tools/generate_animal_cartoons.py
```

Dependencies: Python 3, Pillow, and the Inkscape command-line executable. The
checked-in pixels were generated and verified with Python 3.12, Pillow 12.3.0,
and Inkscape 1.4 (`e7c3feb100`, 2024-10-09). Pass `--inkscape /path/to/inkscape`
if it is not on PATH. Different renderer versions may produce slight antialiasing
changes; review those changes rather than treating every tool version as bit-exact.
There are no downloads, network services, font lookups, or runtime renderer
requirements. Inkscape preferences/cache and intermediate images use a temporary
directory. `--check` never updates the firmware files.

The generator extracts the sole top-level animal group with round line caps,
rendering only that artwork over transparency at 4× size. Area averaging returns
it to the original 320 × 240 coordinate system without ringing. The transparent
bounds are cropped with one pixel of padding and a four-byte-aligned RGB565 row.
Each pixel becomes RGB565 plus an 8-bit alpha plane. Source runs are expanded by
C++17 constant evaluation into immutable LVGL RGB565A8 data, so there is no
application-owned runtime expansion or image buffer. Header and implementation
are regenerated together, including original card-relative offsets.

Current artwork totals 207,756 pixel bytes across the three animals. The original
cat, dog, and duck drawing implementation is independent and unchanged.
