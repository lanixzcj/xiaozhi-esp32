# Original toddler puzzle illustrations

Apple, bus and fish: original geometric vector scenes authored for this project,
licensed under the repository MIT license. No third-party images or recordings.
The scene primitives in `tools/generate_puzzle_assets.py` generate both editable
SVG previews here and the embedded `puzzle_game_assets.cc` using Pillow 11.1.0.
SVG previews use the same shapes; raster stroke placement follows Pillow.

Each opaque picture is 296x162, with 16 colors, a 5-pixel main contour, and a
large subject across the central split. I4 stores 24,040 bytes per picture,
including its 16-entry BGRA palette. Native-size 80x44 previews add 1,824 bytes each, for 77,592 total bytes.
No photo background or letterboxing; no indexed-image scaling at runtime.

Regenerate offline: `python tools/generate_puzzle_assets.py` (from board directory,
or pass the complete script path). Verify with `--check`.
