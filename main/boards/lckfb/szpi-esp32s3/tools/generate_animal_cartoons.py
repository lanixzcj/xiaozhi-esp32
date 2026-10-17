#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Rebuild the three original farm cartoons without downloading any resources.

Requires Python 3, Pillow, and Inkscape. The checked-in output was generated with
Python 3.12, Pillow 12.3.0, and Inkscape 1.4 (e7c3feb100, 2024-10-09).
Run with --check to compare without changing files. See assets/cartoons/README.md.
"""
import argparse
import itertools
import os
from pathlib import Path
import subprocess
import tempfile
import xml.etree.ElementTree as ET

from PIL import Image

BOARD = Path(__file__).resolve().parents[1]
SOURCE = BOARD / "assets/cartoons"
ANIMALS = (("chicken", "Hen"), ("cow", "Cow"), ("sheep", "Sheep"))

HEADER = """// SPDX-License-Identifier: MIT
#pragma once

#include "sdkconfig.h"

#ifndef CONFIG_USE_EMOTE_MESSAGE_STYLE

#include <lvgl.h>
#include <cstddef>

namespace animal_game_art {
// Draw the original farm-animal cartoon inside a 296 x 162 touch card.
// card_x/card_y are the card's absolute top-left; indices 3/4/5 are hen/cow/sheep.
// Unknown indices are ignored. The caller owns the LVGL display lock.
void DrawFarmAnimal(lv_layer_t* layer, int card_x, int card_y, size_t animal_index);
}  // namespace animal_game_art

#endif  // !CONFIG_USE_EMOTE_MESSAGE_STYLE
"""

CXX_PREFIX = """// SPDX-License-Identifier: MIT
#include "animal_game_cartoons.h"

#ifndef CONFIG_USE_EMOTE_MESSAGE_STYLE

#include <array>
#include <cstdint>

namespace animal_game_art {
namespace {
// Original authored hen, calf and lamb artwork from chicken.svg, cow.svg and sheep.svg
// (320 x 240 browser previews). Only the animal artwork is rasterized, not the card,
// labels or controls. Four-times supersampling with area averaging preserves the SVG
// geometry without sharpened/ringing edges. The alpha plane preserves the rounded card.
// These are uncompressed LVGL RGB565A8 assets in flash: no files, SVG/vector engine,
// image objects, theme assets or application-owned decoded buffers are needed.
//
// To keep this source reviewable, repeated pixels are represented as 32-bit runs:
// [count:8 | alpha:8 | RGB565:16]. ExpandPixels executes only at compile time and
// produces the standard RGB565 plane followed by the A8 plane. All image storage
// and descriptors are immutable, shared across redraws and game entries.

template <size_t PixelCount, size_t RunCount>
constexpr std::array<uint8_t, PixelCount * 3> ExpandPixels(const uint32_t (&runs)[RunCount]) {
    std::array<uint8_t, PixelCount * 3> pixels{};
    size_t position = 0;
    for (uint32_t run : runs) {
        const uint32_t count = run >> 24;
        for (uint32_t i = 0; i < count; ++i) {
            // Array bounds are evaluated during constant initialization, so corrupt
            // run data fails compilation rather than writing outside a runtime buffer.
            pixels[position * 2] = static_cast<uint8_t>(run);
            pixels[position * 2 + 1] = static_cast<uint8_t>(run >> 8);
            pixels[PixelCount * 2 + position] = static_cast<uint8_t>(run >> 16);
            ++position;
        }
    }
    return pixels;
}

template <size_t RunCount>
constexpr size_t CountPixels(const uint32_t (&runs)[RunCount]) {
    size_t count = 0;
    for (uint32_t run : runs) {
        count += run >> 24;
    }
    return count;
}

template <size_t Size>
constexpr lv_image_dsc_t Image(const std::array<uint8_t, Size>& pixels, int width, int height) {
    lv_image_dsc_t image{};
    image.header.magic = LV_IMAGE_HEADER_MAGIC;
    image.header.cf = LV_COLOR_FORMAT_RGB565A8;
    image.header.w = width;
    image.header.h = height;
    image.header.stride = width * 2;
    image.data_size = Size;
    image.data = pixels.data();
    return image;
}

"""

CXX_SUFFIX = """constexpr bool ArtworkFitsCard() {
    for (const auto& artwork : kArtwork) {
        if (artwork.x < 0 || artwork.y < 0 || artwork.x + artwork.image->header.w > 296 ||
            artwork.y + artwork.image->header.h > 162) {
            return false;
        }
    }
    return true;
}
static_assert(ArtworkFitsCard());
}  // namespace

void DrawFarmAnimal(lv_layer_t* layer, int card_x, int card_y, size_t animal_index) {
    constexpr size_t kFirstFarmAnimal = 3;
    constexpr size_t kFarmAnimalCount = sizeof(kArtwork) / sizeof(kArtwork[0]);
    if (layer == nullptr || animal_index < kFirstFarmAnimal ||
        animal_index - kFirstFarmAnimal >= kFarmAnimalCount) {
        return;
    }
    const auto& artwork = kArtwork[animal_index - kFirstFarmAnimal];
    lv_draw_image_dsc_t draw;
    lv_draw_image_dsc_init(&draw);
    draw.src = artwork.image;
    const int x = card_x + artwork.x;
    const int y = card_y + artwork.y;
    const lv_area_t area = {x, y, x + artwork.image->header.w - 1, y + artwork.image->header.h - 1};
    lv_draw_image(layer, &draw, &area);
}

}  // namespace animal_game_art

#endif  // !CONFIG_USE_EMOTE_MESSAGE_STYLE
"""

def render_animal(name, symbol, work, inkscape):
    source = ET.parse(SOURCE / (name + ".svg")).getroot()
    groups = [group for group in source if group.get("stroke-linecap") == "round"]
    if len(groups) != 1:
        raise ValueError(name + ": expected exactly one top-level animal artwork group")
    svg = ET.Element("{http://www.w3.org/2000/svg}svg", {
        "width": "320", "height": "240", "viewBox": "0 0 320 240",
    })
    svg.append(groups[0])
    svg_path = work / (name + ".svg")
    png_path = work / (name + ".png")
    ET.ElementTree(svg).write(svg_path, encoding="unicode")
    environment = dict(os.environ)
    # Isolate renderer preferences and caches; generation never modifies user settings.
    environment["INKSCAPE_PROFILE_DIR"] = str(work / "inkscape-profile")
    environment["XDG_CACHE_HOME"] = str(work / "cache")
    result = subprocess.run([
        inkscape, str(svg_path), "--export-type=png",
        "--export-filename=" + str(png_path), "--export-width=1280", "--export-height=960",
    ], env=environment, text=True, capture_output=True)
    if result.returncode:
        raise RuntimeError(name + ": Inkscape failed: " + result.stderr)
    with Image.open(png_path) as original:
        rgba = original.convert("RGBA").resize((320, 240), Image.Resampling.BOX)
    bounds = rgba.getbbox()
    if bounds is None:
        raise ValueError(name + ": empty artwork")
    left, top, right, bottom = bounds
    if left < 12 or top < 46 or right > 308 or bottom > 208:
        raise ValueError(name + ": artwork extends outside the touch card")
    # Include the antialias fringe; use a four-byte-aligned RGB565 row stride.
    left, top = max(12, left - 1), max(46, top - 1)
    right, bottom = min(308, right + 1), min(208, bottom + 1)
    if (right - left) % 2:
        right += 1
    if right > 308:
        raise ValueError(name + ": aligned image extends outside the touch card")
    rgba = rgba.crop((left, top, right, bottom))
    values = []
    channels = iter(rgba.tobytes())
    for red, green, blue, alpha in zip(channels, channels, channels, channels):
        color = ((red >> 3) << 11) | ((green >> 2) << 5) | (blue >> 3) if alpha else 0
        values.append((alpha << 16) | color)
    runs = []
    for value, group in itertools.groupby(values):
        length = sum(1 for _ in group)
        while length:
            count = min(255, length)
            runs.append((count << 24) | value)
            length -= count
    return {"symbol": symbol, "x": left - 12, "y": top - 46,
            "w": rgba.width, "h": rgba.height, "runs": runs, "pixels": len(values)}


def generate(inkscape):
    with tempfile.TemporaryDirectory(prefix="animal-cartoons-") as temporary:
        entries = [render_animal(name, symbol, Path(temporary), inkscape)
                   for name, symbol in ANIMALS]
    text = CXX_PREFIX
    for entry in entries:
        symbol, count, runs = entry["symbol"], entry["pixels"], entry["runs"]
        text += (f"// {symbol}: {entry['w']} x {entry['h']}; "
                 f"card-relative origin ({entry['x']}, {entry['y']}).\n")
        text += "// clang-format off\n"
        text += f"constexpr uint32_t k{symbol}Runs[] = {{\n"
        for offset in range(0, len(runs), 7):
            text += "    " + ", ".join(f"0x{value:08X}" for value in runs[offset:offset + 7]) + ",\n"
        text += "};\n// clang-format on\n"
        text += f"static_assert(CountPixels(k{symbol}Runs) == {count});\n"
        text += f"alignas(4) constexpr auto k{symbol}Pixels = ExpandPixels<{count}>(k{symbol}Runs);\n"
        text += f"constexpr auto k{symbol}Image = Image(k{symbol}Pixels, {entry['w']}, {entry['h']});\n\n"
    text += ("struct Artwork {\n    const lv_image_dsc_t* image;\n    int x;\n    int y;\n};\n\n"
             "constexpr Artwork kArtwork[] = {\n")
    for entry in entries:
        text += f"    {{&k{entry['symbol']}Image, {entry['x']}, {entry['y']}}},\n"
    text += "};\n\n" + CXX_SUFFIX
    return {"animal_game_cartoons.h": HEADER, "animal_game_cartoons.cc": text}, entries


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true", help="compare generated files without writing")
    parser.add_argument("--inkscape", default="inkscape", help="path to the Inkscape executable")
    args = parser.parse_args()
    files, entries = generate(args.inkscape)
    mismatch = []
    for name, content in files.items():
        destination = BOARD / name
        if args.check:
            if not destination.exists() or destination.read_text(encoding="utf-8") != content:
                mismatch.append(name)
        else:
            destination.write_text(content, encoding="utf-8")
    if mismatch:
        parser.exit(1, "Cartoon output differs: " + ", ".join(mismatch) +
                    ". Check renderer versions, then regenerate and review.\n")
    verb = "Verified" if args.check else "Generated"
    print(f"{verb} {len(entries)} cartoons; "
          f"{sum(entry['pixels'] * 3 for entry in entries):,} RGB565A8 bytes.")


if __name__ == "__main__":
    main()
