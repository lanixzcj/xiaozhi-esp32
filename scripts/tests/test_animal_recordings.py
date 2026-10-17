"""Offline recording provenance, reproducibility, and real playback-path tests."""

import ctypes.util
import hashlib
import importlib.util
import json
import re
import shutil
import struct
import subprocess
import tempfile
import unittest
import wave
from pathlib import Path

import test_local_interaction as local_support

ROOT = Path(__file__).resolve().parents[2]
BOARD = ROOT / "main/boards/lckfb/szpi-esp32s3"
spec = importlib.util.spec_from_file_location("animal_sound_generator",
                                             BOARD / "tools/generate_animal_sounds.py")
generator = importlib.util.module_from_spec(spec)
spec.loader.exec_module(generator)


def embedded_sounds():
    source = (BOARD / "animal_game_sounds.h").read_text()
    result = []
    for name in generator.SOUNDS:
        body = re.search(r"k" + name.capitalize() + r"Ogg\[\] = \{(.*?)\};", source, re.S)[1]
        result.append(bytes(int(value, 16) for value in re.findall(r"0x([0-9a-f]{2})", body)))
    return result


def ogg_fixture(packets):
    """Make a correctly checksummed one-page test stream with real Opus headers."""
    lacing = bytearray()
    for packet in packets:
        lacing.extend([255] * (len(packet) // 255) + [len(packet) % 255])
    assert len(lacing) <= 255
    page = bytearray(b"OggS\0\x06" + struct.pack("<QIIIB", 0, 1, 0, 0, len(lacing)))
    page += lacing + b"".join(packets)
    crc = 0
    for value in page:
        crc ^= value << 24
        for _ in range(8):
            crc = ((crc << 1) ^ (0x04c11db7 if crc & 0x80000000 else 0)) & 0xffffffff
    struct.pack_into("<I", page, 22, crc)
    return bytes(page)


class AnimalRecordingTests(unittest.TestCase):
    def compile_and_run(self, driver):
        # Use the existing ESP platform stubs with the actual repository demuxer.
        local_support.LocalInteractionTests.compile_and_run(
            self, f'#include "{BOARD / "animal_game_sounds.h"}"\n' + driver,
            [ROOT / "main/audio/demuxer/ogg_demuxer.cc"])

    def test_exact_approved_sources_and_separate_licenses(self):
        expected_frames = (56880, 48926, 48000, 63600, 57600, 50276)
        records = generator.load_provenance()
        for name, frames in zip(generator.SOUNDS, expected_frames):
            path, actual_frames, digest = generator.source_info(name, records[name])
            self.assertEqual(actual_frames, frames)
            self.assertEqual(digest, records[name]["adaptation"]["wav_sha256"])
            self.assertEqual(hashlib.sha256(path.read_bytes()).hexdigest(), digest)
        with wave.open(str(BOARD / "assets/audio/animal-sheep.wav")) as wav:
            samples = wav.readframes(wav.getnframes())
        bleat_bytes, gap_bytes = 20938 * 2, 8400 * 2
        self.assertEqual(samples[:bleat_bytes], samples[bleat_bytes + gap_bytes:])
        self.assertEqual(samples[bleat_bytes:bleat_bytes + gap_bytes], bytes(gap_bytes))
        header = (BOARD / "animal_game_sounds.h").read_text()
        self.assertNotIn("// SPDX-License-Identifier: MIT", header)
        self.assertIn("CC BY-SA 3.0, NOT MIT", header)
        duck_tags = generator.ogg_packets(embedded_sounds()[2])[1]
        for required in (b"Jonathon Jongsma", b"https://creativecommons.org/licenses/by-sa/3.0/",
                         b"https://xeno-canto.org/62258", b"0.98-2.98", b"high-pass"):
            self.assertIn(required, duck_tags)

    @unittest.skipUnless(shutil.which("ffmpeg") and shutil.which("ffprobe"),
                         "requires local FFmpeg/libopus and ffprobe")
    def test_regeneration_and_full_lengths(self):
        expected_reports = json.loads((BOARD / "assets/audio/firmware-encoding-report.json").read_text())
        with tempfile.TemporaryDirectory() as temporary:
            sounds = []
            reports = []
            for name in generator.SOUNDS:
                data, report = generator.encode_and_validate(Path(temporary), name)
                sounds.append((name, data))
                reports.append(report)
            self.assertEqual(generator.render_header(sounds), (BOARD / "animal_game_sounds.h").read_text())
            self.assertEqual(reports, expected_reports)
        for data in embedded_sounds():
            with self.assertRaises(ValueError):
                generator.ogg_packets(data[:-1])
            damaged = bytearray(data)
            damaged[-5] ^= 1
            with self.assertRaises(ValueError):
                generator.ogg_packets(damaged)

    def test_actual_admission_accepts_all_recordings_atomically(self):
        packets = generator.ogg_packets(embedded_sounds()[0])
        # 51 * 60ms fails duration; 51 * 20ms independently fails packet bound.
        too_long = ogg_fixture(packets[:2] + [packets[2]] * 51)
        too_many = ogg_fixture(packets[:2] + [b"\xf8\xff\xfe"] * 51)
        # The exact 50-packet / 3000ms edge remains accepted.
        at_limit = ogg_fixture(packets[:2] + [packets[2]] * 50)
        fixtures = ""
        for name, value in (("TooLong", too_long), ("TooMany", too_many), ("AtLimit", at_limit)):
            fixtures += f"const unsigned char k{name}[] = {{" + ",".join(map(str, value)) + "};\n"
        platform = r'''
#include <atomic>
#include <cassert>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include "fixed_queue.h"
#include "ogg_demuxer.h"
struct AudioStreamPacket {
    int sample_rate = 0, frame_duration = 0;
    std::vector<uint8_t> payload;
};
class AudioService {
public:
    std::mutex audio_queue_mutex_;
    std::condition_variable audio_queue_cv_;
    std::atomic<bool> local_interaction_active_{false}, service_stopped_{false};
    uint32_t playback_generation_ = 0;
    bool playback_drained_notified_ = true, decode_in_flight_ = false, output_in_flight_ = false;
    FixedQueue<std::unique_ptr<AudioStreamPacket>, 166> audio_decode_queue_;
    FixedQueue<int, 2> audio_playback_queue_;
    bool PlayLocalInteractionSound(std::string_view);
    bool IsPlaybackDrainedLocked() const;
};
std::string_view View(const unsigned char* data, size_t size) {
    return {reinterpret_cast<const char*>(data), size};
}
'''
        checks = r'''
int main() {
    constexpr size_t counts[] = {40, 35, 34, 45, 41, 36};
    AudioService audio;
    assert(!audio.PlayLocalInteractionSound(animal_game_assets::GetSound(0)));
    audio.local_interaction_active_ = true;
    for (size_t i = 0; i < animal_game_assets::kSoundCount; ++i) {
        const auto clip = animal_game_assets::GetSound(i);
        assert(audio.PlayLocalInteractionSound(clip));
        assert(audio.audio_decode_queue_.size() == counts[i]);
        assert(!audio.playback_drained_notified_);
        assert(!audio.PlayLocalInteractionSound(clip)); // no duplicates or tap queue
        assert(audio.audio_decode_queue_.size() == counts[i]);
        size_t duration = 0;
        while (!audio.audio_decode_queue_.empty()) {
            auto& packet = audio.audio_decode_queue_.front();
            assert(packet->sample_rate == 24000 && packet->frame_duration == 60);
            assert(packet->payload.size() == 180);
            duration += packet->frame_duration;
            audio.audio_decode_queue_.pop_front();
        }
        assert(duration == counts[i] * 60);
        audio.decode_in_flight_ = true;
        assert(!audio.PlayLocalInteractionSound(clip));
        audio.decode_in_flight_ = false;
        audio.output_in_flight_ = true;
        assert(!audio.PlayLocalInteractionSound(clip));
        audio.output_in_flight_ = false;
        assert(!audio.PlayLocalInteractionSound(clip.substr(0, clip.size() - 1)));
        assert(audio.audio_decode_queue_.empty()); // reject rather than truncate
    }
    assert(!audio.PlayLocalInteractionSound(View(kTooLong, sizeof(kTooLong))));
    assert(audio.audio_decode_queue_.empty());
    assert(!audio.PlayLocalInteractionSound(View(kTooMany, sizeof(kTooMany))));
    assert(audio.audio_decode_queue_.empty());
    assert(audio.PlayLocalInteractionSound(View(kAtLimit, sizeof(kAtLimit))));
    assert(audio.audio_decode_queue_.size() == 50);
    audio.audio_decode_queue_.clear();
    assert(!audio.PlayLocalInteractionSound("not Ogg"));
    assert(!audio.PlayLocalInteractionSound({}));
    assert(!audio.PlayLocalInteractionSound(std::string(24577, 'x')));
    audio.service_stopped_ = true;
    assert(!audio.PlayLocalInteractionSound(animal_game_assets::GetSound(0)));
    assert(audio.audio_decode_queue_.empty());
    assert(animal_game_assets::GetSound(6).empty());
    assert(animal_game_assets::GetSound(size_t(-1)).empty());
}
'''
        audio_source = (ROOT / "main/audio/audio_service.cc").read_text()
        self.compile_and_run(platform + fixtures + "\n".join(
            local_support.definition(audio_source, signature) for signature in (
                "bool AudioService::PlayLocalInteractionSound(",
                "bool AudioService::IsPlaybackDrainedLocked()")) + checks)

    @unittest.skipUnless(ctypes.util.find_library("opus"), "requires the host libopus decoder")
    def test_actual_demuxer_and_decoder_full_and_chunked(self):
        library = json.dumps(ctypes.util.find_library("opus"))
        driver = r'''
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <dlfcn.h>
#include <iostream>
#include "ogg_demuxer.h"
struct OpusDecoder;
int main() {
    void* library = dlopen(OPUS_LIBRARY, RTLD_NOW);
    assert(library);
    auto create = reinterpret_cast<OpusDecoder* (*)(int32_t,int,int*)>(dlsym(library,"opus_decoder_create"));
    auto decode = reinterpret_cast<int (*)(OpusDecoder*,const unsigned char*,int32_t,int16_t*,int,int)>(
        dlsym(library,"opus_decode"));
    auto destroy = reinterpret_cast<void (*)(OpusDecoder*)>(dlsym(library,"opus_decoder_destroy"));
    assert(create && decode && destroy);
    constexpr size_t counts[] = {40, 35, 34, 45, 41, 36};
    constexpr size_t source_frames[] = {56880, 48926, 48000, 63600, 57600, 50276};
    for (size_t i = 0; i < animal_game_assets::kSoundCount; ++i) {
        const auto clip = animal_game_assets::GetSound(i);
        for (size_t chunk : {size_t(1), size_t(17), clip.size()}) {
            int error = -1;
            auto decoder = create(24000, 1, &error);
            assert(decoder && error == 0);
            OggDemuxer demuxer;
            size_t packet_count = 0, frames = 0;
            int peak = 0;
            demuxer.OnPacket([&](const uint8_t* data, int rate, int duration, size_t size) {
                assert(rate == 24000 && duration == 60 && size == 180 && size < 2048);
                int16_t pcm[1440];
                int n = decode(decoder, data, size, pcm, 1440, 0);
                assert(n == 1440);
                for (int j = 0; j < n; ++j) peak = std::max(peak, std::abs(int(pcm[j])));
                ++packet_count;
                frames += n;
            });
            for (size_t offset = 0; offset < clip.size(); offset += chunk) {
                const auto n = std::min(chunk, clip.size() - offset);
                assert(demuxer.Process(reinterpret_cast<const uint8_t*>(clip.data()) + offset, n) == n);
            }
            assert(demuxer.Finish() && !demuxer.HasError());
            assert(packet_count == counts[i] && frames == counts[i] * 1440);
            // Firmware intentionally decodes pre-skip and final padding, too.
            assert(frames >= source_frames[i] + 156);
            assert(frames - (source_frames[i] + 156) < 1440);
            assert(peak > 0 && peak <= int(0.56 * 32768));
            if (chunk == clip.size()) {
                std::cout << i << " packets=" << packet_count << " frames=" << frames
                          << " peak_dbfs=" << 20 * std::log10(peak / 32768.0) << '\n';
            }
            destroy(decoder);
        }
    }
    dlclose(library);
}
'''.replace("OPUS_LIBRARY", library)
        self.compile_and_run(driver)


if __name__ == "__main__":
    unittest.main()
