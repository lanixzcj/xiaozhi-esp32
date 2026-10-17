#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Create the board's original, offline animal-like sounds (Python stdlib + FFmpeg)."""

import argparse
import hashlib
import json
import math
from pathlib import Path
import random
import shutil
import struct
import subprocess
import tempfile
import wave


SAMPLE_RATE = 16000
PEAK = 0.19  # About -14.4 dBFS before lossy encoding, with substantial headroom.
SOUNDS = (("cat", 0.66, 1101), ("dog", 0.42, 2202), ("duck", 0.42, 3303))
BOARD_DIR = Path(__file__).resolve().parents[1]


def envelope(t, duration, attack, release):
    """One soft onset and release, plus silence at both ends; never a repeated call."""
    start = 0.012
    end = duration - 0.035
    if not start < t < end:
        return 0.0
    ramp_in = min(1.0, (t - start) / attack)
    ramp_out = min(1.0, (end - t) / release)
    return math.sin(ramp_in * math.pi / 2) ** 2 * math.sin(ramp_out * math.pi / 2) ** 2


def synthesize(name, duration, seed):
    """Voiced harmonics and seeded, low-pass noise; no recordings or fetched samples."""
    rng = random.Random(seed)
    phase = 0.0
    noise = 0.0
    result = []
    for index in range(round(duration * SAMPLE_RATE)):
        t = index / SAMPLE_RATE
        u = t / duration
        noise = 0.84 * noise + 0.16 * rng.uniform(-1.0, 1.0)
        if name == "cat":
            # A single rounded "mee-ow": rising then falling pitch and vowel formants.
            pitch = 440 + 170 * math.sin(math.pi * min(1.0, u / 0.9)) - 80 * u
            pitch *= 1.0 + 0.008 * math.sin(2 * math.pi * 5 * t)
            vowel = min(1.0, max(0.0, (u - 0.25) / 0.45))
            formants = ((850 - 220 * vowel, 340, 0.65), (2200 - 850 * vowel, 500, 0.32))
            attack, release, air = 0.055, 0.13, 0.025
            shape = 0.85 + 0.15 * math.sin(math.pi * u)
        elif name == "dog":
            # One small dog's soft "woof", low and rounded rather than a sharp bark.
            pitch = 155 - 55 * u + 8 * math.sin(math.pi * u)
            pitch *= 1.0 + 0.025 * math.sin(2 * math.pi * 29 * t)
            formants = ((470 - 100 * u, 260, 0.8), (1100 - 250 * u, 420, 0.32))
            attack, release, air = 0.035, 0.16, 0.24
            shape = math.exp(-1.8 * u)
        else:
            # One nasal, descending "quack", with rounded high harmonics.
            pitch = 285 - 105 * u
            pitch *= 1.0 + 0.018 * math.sin(2 * math.pi * 36 * t)
            formants = ((1000 - 350 * u, 250, 1.0), (1800 - 300 * u, 380, 0.34))
            attack, release, air = 0.028, 0.12, 0.07
            shape = math.exp(-1.0 * u)

        phase += 2 * math.pi * pitch / SAMPLE_RATE
        voiced = 0.0
        for harmonic in range(1, 11):
            frequency = pitch * harmonic
            weight = 0.22 / harmonic ** 1.4
            for center, width, gain in formants:
                weight += gain * math.exp(-0.5 * ((frequency - center) / width) ** 2) / harmonic
            weight *= math.exp(-(frequency / 3100) ** 2)
            if name == "duck" and harmonic % 2 == 0:
                weight *= 0.45
            voiced += weight * math.sin(harmonic * phase)
        value = (voiced + air * noise) * shape * envelope(t, duration, attack, release)
        result.append(value)

    # Remove any tiny DC offset, retaining exactly silent endpoints and smooth fades.
    mean = sum(result) / len(result)
    result = [value - mean * envelope(i / SAMPLE_RATE, duration, 0.06, 0.12)
              for i, value in enumerate(result)]
    scale = PEAK / max(abs(value) for value in result)
    return [round(value * scale * 32767) for value in result]


def run(args):
    return subprocess.run(args, check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE).stdout


def ogg_packets(data):
    """Read Ogg lacing so validation also covers the firmware's untrimmed playback."""
    packets = []
    packet = bytearray()
    offset = 0
    while offset < len(data):
        if data[offset:offset + 4] != b"OggS":
            raise ValueError("Invalid Ogg page")
        count = data[offset + 26]
        lacing = data[offset + 27:offset + 27 + count]
        offset += 27 + count
        for length in lacing:
            packet.extend(data[offset:offset + length])
            offset += length
            if length < 255:
                packets.append(bytes(packet))
                packet.clear()
    if packet or offset != len(data):
        raise ValueError("Truncated Ogg stream")
    return packets


def packet_duration_ms(packet):
    config = packet[0] >> 3
    if config < 12:
        frame = (10, 20, 40, 60)[config & 3]
    elif config < 16:
        frame = 20 if config & 1 else 10
    else:
        frame = (2.5, 5, 10, 20)[config & 3]
    code = packet[0] & 3
    count = (1, 2, 2)[code] if code < 3 else packet[1] & 63
    return frame * count


def encode_and_validate(directory, name, duration, seed, ffmpeg, ffprobe):
    pcm = synthesize(name, duration, seed)
    wav_path = directory / f"{name}.wav"
    ogg_path = directory / f"{name}.ogg"
    with wave.open(str(wav_path), "wb") as wav:
        wav.setparams((1, 2, SAMPLE_RATE, 0, "NONE", "not compressed"))
        wav.writeframes(struct.pack(f"<{len(pcm)}h", *pcm))
    run([ffmpeg, "-hide_banner", "-loglevel", "error", "-y", "-fflags", "+bitexact",
         "-i", str(wav_path), "-map_metadata", "-1", "-ac", "1", "-ar", str(SAMPLE_RATE),
         "-c:a", "libopus", "-b:a", "24000", "-vbr", "off", "-application", "audio",
         "-frame_duration", "60", "-compression_level", "10", "-flags:a", "+bitexact",
         "-fflags", "+bitexact", "-serial_offset", "0", str(ogg_path)])
    encoded = ogg_path.read_bytes()
    packets = ogg_packets(encoded)
    if (packets[0][:8] != b"OpusHead" or packets[0][9] != 1
            or struct.unpack_from("<I", packets[0], 12)[0] != SAMPLE_RATE
            or packets[1][:8] != b"OpusTags"):
        raise ValueError(f"{name}: wrong mono/16 kHz Opus headers")
    audio_packets = packets[2:]
    durations = [packet_duration_ms(packet) for packet in audio_packets]
    if not durations or any(value != 60 for value in durations):
        raise ValueError(f"{name}: packets must each contain 60 ms")
    if sum(durations) > 800 or max(map(len, audio_packets)) > 2048:
        raise ValueError(f"{name}: exceeds duration or demuxer packet buffer limits")
    probe = json.loads(run([ffprobe, "-v", "error", "-show_entries",
                           "stream=codec_name,channels:format=duration", "-of", "json",
                           str(ogg_path)]))
    if probe["streams"][0] != {"codec_name": "opus", "channels": 1}:
        raise ValueError(f"{name}: unexpected ffprobe stream")
    decoded = run([ffmpeg, "-v", "error", "-i", str(ogg_path), "-f", "s16le",
                   "-ac", "1", "-ar", str(SAMPLE_RATE), "pipe:1"])
    samples = struct.unpack(f"<{len(decoded) // 2}h", decoded)
    peak = max(abs(sample) for sample in samples) / 32768
    rms = math.sqrt(sum(sample * sample for sample in samples) / len(samples)) / 32768
    if peak > 0.23 or len(samples) / SAMPLE_RATE > 0.8:
        raise ValueError(f"{name}: decoded audio exceeds conservative peak/duration limits")
    report = {"name": name, "bytes": len(encoded), "source_seconds": duration,
              "ogg_seconds": float(probe["format"]["duration"]),
              "untrimmed_playback_seconds": sum(durations) / 1000,
              "packets": len(audio_packets), "largest_packet_bytes": max(map(len, audio_packets)),
              "decoded_peak_dbfs": round(20 * math.log10(peak), 2),
              "decoded_rms_dbfs": round(20 * math.log10(rms), 2),
              "sha256": hashlib.sha256(encoded).hexdigest()}
    return encoded, report


def render_header(sounds):
    lines = ["// SPDX-License-Identifier: MIT",
             "// Generated by tools/generate_animal_sounds.py; see ANIMAL_SOUND_ASSETS.md.",
             "// Original synthesized calls. Mono, 16 kHz input, Ogg Opus, 60 ms packets.",
             "#pragma once", "", "#include <cstddef>", "#include <string_view>", "",
             "namespace animal_game_assets {", "", "inline constexpr size_t kSoundCount = 3;", ""]
    for name, data in sounds:
        symbol = "k" + name.capitalize() + "Ogg"
        lines += ["// clang-format off", f"inline constexpr unsigned char {symbol}[] = {{"]
        for offset in range(0, len(data), 12):
            lines.append("    " + ", ".join(f"0x{byte:02x}" for byte in data[offset:offset + 12]) + ",")
        lines += ["};", "// clang-format on", ""]
    lines += ["// The board's animal order is cat, dog, duck. Views refer to flash-resident data.",
              "inline std::string_view GetSound(size_t index) {", "    switch (index) {"]
    for index, (name, _) in enumerate(sounds):
        symbol = "k" + name.capitalize() + "Ogg"
        lines += [f"        case {index}:",
                  f"            return {{reinterpret_cast<const char*>({symbol}), sizeof({symbol})}};"]
    lines += ["        default:", "            return {};", "    }", "}", "",
              "}  // namespace animal_game_assets", ""]
    return "\n".join(lines)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true", help="verify committed header without writing")
    parser.add_argument("--preview-dir", type=Path, help="keep WAV/OGG previews outside the firmware")
    parser.add_argument("--ffmpeg", default="ffmpeg")
    parser.add_argument("--ffprobe", default="ffprobe")
    args = parser.parse_args()
    for executable in (args.ffmpeg, args.ffprobe):
        if shutil.which(executable) is None:
            parser.error(f"Missing executable: {executable}")
    with tempfile.TemporaryDirectory(prefix="animal-game-sounds-") as temporary:
        directory = Path(temporary)
        assets = []
        reports = []
        for name, duration, seed in SOUNDS:
            data, report = encode_and_validate(directory, name, duration, seed,
                                               args.ffmpeg, args.ffprobe)
            assets.append((name, data))
            reports.append(report)
        header = render_header(assets)
        target = BOARD_DIR / "animal_game_sounds.h"
        if args.check:
            if not target.exists() or target.read_text() != header:
                raise SystemExit("Generated header differs. Regenerate with the documented toolchain.")
        else:
            target.write_text(header)
        if args.preview_dir:
            args.preview_dir.mkdir(parents=True, exist_ok=True)
            for source in directory.iterdir():
                shutil.copy2(source, args.preview_dir / source.name)
        print(json.dumps(reports, indent=2))


if __name__ == "__main__":
    main()
