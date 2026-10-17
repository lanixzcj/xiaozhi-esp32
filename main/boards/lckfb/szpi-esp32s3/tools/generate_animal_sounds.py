#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Encode the six committed recordings offline (Python stdlib + FFmpeg/libopus).

The generator is MIT licensed; recordings retain their separate source licenses.
No downloading, synthesis, trimming, normalization, or time/pitch changes occur.
"""

import argparse
import hashlib
import json
import math
from pathlib import Path
import shutil
import struct
import subprocess
import tempfile
import wave


SAMPLE_RATE = 24000
FRAME_MS = 60
MAX_PLAYBACK_MS = 3000
MAX_PACKET_BYTES = 2048  # OggDemuxer::context_t::packet_buf, including OpusTags.
MAX_SOUND_BYTES = 24 * 1024
MAX_DECODED_PEAK = 0.56  # Allows codec overshoot above the supplied -6 dBTP dog.
SOUNDS = ("cat", "dog", "duck", "chicken", "cow", "sheep")
BOARD_DIR = Path(__file__).resolve().parents[1]
AUDIO_DIR = BOARD_DIR / "assets/audio"


def run(args):
    return subprocess.run(args, check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE).stdout


def load_provenance():
    records = json.loads((AUDIO_DIR / "recording-provenance.json").read_text())
    if tuple(record["animal"] for record in records) != SOUNDS:
        raise ValueError("Provenance must contain exactly cat, dog, duck, chicken, cow, sheep")
    return {record["animal"]: record for record in records}


def ogg_pages(data):
    """Validate boundaries and checksums, retaining granules for completeness checks."""
    offset = 0
    sequence = 0
    serial = None
    while offset < len(data):
        if len(data) - offset < 27 or data[offset:offset + 4] != b"OggS":
            raise ValueError("Invalid or truncated Ogg page")
        count = data[offset + 26]
        header_end = offset + 27 + count
        if header_end > len(data):
            raise ValueError("Truncated Ogg lacing table")
        lacing = data[offset + 27:header_end]
        end = header_end + sum(lacing)
        if end > len(data):
            raise ValueError("Truncated Ogg page body")
        page = bytearray(data[offset:end])
        expected_crc = struct.unpack_from("<I", page, 22)[0]
        page[22:26] = b"\0" * 4
        crc = 0
        for byte in page:
            crc ^= byte << 24
            for _ in range(8):
                crc = ((crc << 1) ^ (0x04c11db7 if crc & 0x80000000 else 0)) & 0xffffffff
        if crc != expected_crc:
            raise ValueError("Invalid Ogg checksum")
        page_serial, page_sequence = struct.unpack_from("<II", data, offset + 14)
        if serial is None:
            serial = page_serial
        if data[offset + 4] != 0 or page_serial != serial or page_sequence != sequence:
            raise ValueError("Unexpected Ogg version, serial, or sequence")
        yield (data[offset + 5], struct.unpack_from("<Q", data, offset + 6)[0],
               lacing, data[header_end:end])
        sequence += 1
        offset = end


def ogg_packets(data):
    """Read all lacing, including continuation across Ogg page boundaries."""
    packets = []
    packet = bytearray()
    pages = list(ogg_pages(data))
    if not pages or not pages[0][0] & 2 or not pages[-1][0] & 4:
        raise ValueError("Ogg stream must have beginning and end markers")
    for flags, _, lacing, body in pages:
        if bool(flags & 1) != bool(packet):
            raise ValueError("Invalid Ogg packet continuation")
        offset = 0
        for length in lacing:
            packet.extend(body[offset:offset + length])
            offset += length
            if length < 255:
                packets.append(bytes(packet))
                packet.clear()
    if packet:
        raise ValueError("Truncated Ogg packet")
    return packets


def packet_duration_ms(packet):
    if not packet:
        raise ValueError("Empty Opus packet")
    config = packet[0] >> 3
    if config < 12:
        frame = (10, 20, 40, 60)[config & 3]
    elif config < 16:
        frame = 20 if config & 1 else 10
    else:
        frame = (2.5, 5, 10, 20)[config & 3]
    code = packet[0] & 3
    if code == 3 and len(packet) < 2:
        raise ValueError("Truncated Opus packet")
    count = (1, 2, 2)[code] if code < 3 else packet[1] & 63
    if not count or frame * count > 120:
        raise ValueError("Invalid Opus duration")
    return frame * count


def source_info(name, record):
    adaptation = record["adaptation"]
    path = AUDIO_DIR / adaptation["wav_file"]
    if path.name != f"animal-{name}.wav" or path.parent != AUDIO_DIR:
        raise ValueError(f"{name}: unexpected source filename")
    digest = hashlib.sha256(path.read_bytes()).hexdigest()
    if digest != adaptation["wav_sha256"]:
        raise ValueError(f"{name}: committed WAV differs from the approved recording")
    with wave.open(str(path), "rb") as wav:
        if (wav.getnchannels(), wav.getsampwidth(), wav.getframerate(), wav.getcomptype()) != (
                1, 2, SAMPLE_RATE, "NONE"):
            raise ValueError(f"{name}: source must be mono 24 kHz PCM16")
        frames = wav.getnframes()
        pcm = wav.readframes(frames)
        if len(pcm) != frames * 2 or frames != round(adaptation["duration_seconds"] * SAMPLE_RATE):
            raise ValueError(f"{name}: source recording length differs from provenance")
    if not 2 * SAMPLE_RATE <= frames <= 3 * SAMPLE_RATE:
        raise ValueError(f"{name}: expected the approved 2-3 second recording")
    return path, frames, digest


def encode_and_validate(directory, name, ffmpeg="ffmpeg", ffprobe="ffprobe"):
    record = load_provenance()[name]
    wav_path, source_frames, source_digest = source_info(name, record)
    ogg_path = directory / f"{name}.ogg"
    # Keep attribution in the encoded stream as well as the source distribution.
    metadata = {
        "title": record["title"], "artist": record["author"],
        "copyright": record["license"], "license": record["license_url"],
        "source": record["source_page_url"],
        "comment": "Adapted recording: " + record["adaptation"]["processing"] +
                   "; encoded as 24 kHz mono Ogg Opus. See ANIMAL_SOUND_ASSETS.md.",
    }
    if name == "duck":
        metadata["comment"] += (
            " Based on XC62258 by Jonathon Jongsma, https://xeno-canto.org/62258; "
            "excerpt 0.98-2.98 seconds. This adaptation remains CC BY-SA 3.0 "
            "https://creativecommons.org/licenses/by-sa/3.0/; no endorsement.")
    args = [ffmpeg, "-hide_banner", "-loglevel", "error", "-y", "-fflags", "+bitexact",
            "-i", str(wav_path), "-map_metadata", "-1", "-ac", "1", "-ar", str(SAMPLE_RATE),
            "-c:a", "libopus", "-b:a", "24000", "-vbr", "off", "-application", "audio",
            "-frame_duration", str(FRAME_MS), "-compression_level", "10",
            "-flags:a", "+bitexact", "-fflags", "+bitexact", "-serial_offset", "0"]
    for key, value in metadata.items():
        args += ["-metadata", f"{key}={value}"]
    run(args + [str(ogg_path)])
    encoded = ogg_path.read_bytes()
    packets = ogg_packets(encoded)
    if (len(packets) < 3 or packets[0][:8] != b"OpusHead" or len(packets[0]) != 19
            or packets[0][9] != 1 or packets[0][18] != 0
            or struct.unpack_from("<I", packets[0], 12)[0] != SAMPLE_RATE
            or packets[1][:8] != b"OpusTags"):
        raise ValueError(f"{name}: wrong mono/24 kHz Opus headers")
    if max(map(len, packets)) > MAX_PACKET_BYTES or len(encoded) > MAX_SOUND_BYTES:
        raise ValueError(f"{name}: exceeds firmware byte/packet limits")
    audio_packets = packets[2:]
    durations = [packet_duration_ms(packet) for packet in audio_packets]
    if any(value != FRAME_MS for value in durations) or sum(durations) > MAX_PLAYBACK_MS:
        raise ValueError(f"{name}: expected 60 ms packets and at most 3 seconds padded playback")
    preskip_48k = struct.unpack_from("<H", packets[0], 10)[0]
    granule = list(ogg_pages(encoded))[-1][1]
    if granule - preskip_48k != source_frames * (48000 // SAMPLE_RATE):
        raise ValueError(f"{name}: last granule would trim or extend the source recording")
    # Exactly enough whole packets to include every sample plus encoder delay.
    expected_packets = math.ceil((source_frames * 2 + preskip_48k) / (48 * FRAME_MS))
    if len(audio_packets) != expected_packets:
        raise ValueError(f"{name}: truncated or unexpectedly padded packet stream")
    probe = json.loads(run([ffprobe, "-v", "error", "-show_entries",
                           "stream=codec_name,channels:format=duration", "-of", "json",
                           str(ogg_path)]))
    if probe["streams"][0] != {"codec_name": "opus", "channels": 1}:
        raise ValueError(f"{name}: unexpected ffprobe stream")
    decoded = run([ffmpeg, "-v", "error", "-i", str(ogg_path), "-f", "s16le",
                   "-ac", "1", "-ar", str(SAMPLE_RATE), "pipe:1"])
    if len(decoded) != source_frames * 2:
        raise ValueError(f"{name}: decoded length differs from the complete approved WAV")
    samples = struct.unpack(f"<{len(decoded) // 2}h", decoded)
    peak = max(abs(sample) for sample in samples) / 32768
    rms = math.sqrt(sum(sample * sample for sample in samples) / len(samples)) / 32768
    if not 0 < peak <= MAX_DECODED_PEAK:
        raise ValueError(f"{name}: decoded audio exceeds peak limit or is silent")
    report = {"name": name, "bytes": len(encoded), "source_frames": source_frames,
              "source_seconds": source_frames / SAMPLE_RATE,
              "ogg_seconds": float(probe["format"]["duration"]),
              "untrimmed_playback_seconds": sum(durations) / 1000,
              "packets": len(audio_packets), "largest_packet_bytes": max(map(len, audio_packets)),
              "largest_header_packet_bytes": max(map(len, packets[:2])),
              "decoded_frames": len(samples), "decoded_peak_dbfs": round(20 * math.log10(peak), 2),
              "decoded_rms_dbfs": round(20 * math.log10(rms), 2),
              "source_sha256": source_digest, "sha256": hashlib.sha256(encoded).hexdigest()}
    # Preview source bytes are a direct copy, never an altered intermediate WAV.
    shutil.copyfile(wav_path, directory / f"{name}.wav")
    return encoded, report


def render_header(sounds):
    records = load_provenance()
    lines = ["// Generated by tools/generate_animal_sounds.py; see ANIMAL_SOUND_ASSETS.md.",
             "// C++ wrapper: MIT. Audio bytes retain the individual licenses below.",
             "// The duck recording and its adaptation are CC BY-SA 3.0, NOT MIT.",
             "// Real recordings. Mono, 24 kHz input, Ogg Opus, 60 ms packets.",
             "#pragma once", "", "#include <cstddef>", "#include <string_view>", "",
             "namespace animal_game_assets {", "", f"inline constexpr size_t kSoundCount = {len(sounds)};", ""]
    for name, data in sounds:
        record = records[name]
        symbol = "k" + name.capitalize() + "Ogg"
        lines += [f'// {record["title"]} by {record["author"]}.',
                  f'// Audio license: {record["license"]}.',
                  f'// {record["source_page_url"]}', f'// {record["license_url"]}']
        if name == "duck":
            lines += ["// Adapted: excerpt 0.98-2.98 s, high-pass, fades, gain, resampling, Opus encoding.",
                      "// This adaptation remains CC BY-SA 3.0. No endorsement is implied."]
        lines += ["// clang-format off", f"inline constexpr unsigned char {symbol}[] = {{"]
        for offset in range(0, len(data), 12):
            lines.append("    " + ", ".join(f"0x{byte:02x}" for byte in data[offset:offset + 12]) + ",")
        lines += ["};", "// clang-format on", ""]
    lines += ["// Order: cat, dog, duck, chicken (adult hen), cow, sheep. Flash-resident data.",
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
    parser.add_argument("--preview-dir", type=Path, help="keep exact source WAV/encoded OGG previews")
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
        for name in SOUNDS:
            data, report = encode_and_validate(directory, name, args.ffmpeg, args.ffprobe)
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
