"""Host regression tests for exclusive local audio and stale main-task work."""

import os
import shlex
import shutil
import subprocess
import tempfile
import textwrap
import unittest
from pathlib import Path

from test_startup_wake_word_hint import definition

ROOT = Path(__file__).resolve().parents[2]
APP = (ROOT / "main/application.cc").read_text()
AUDIO = (ROOT / "main/audio/audio_service.cc").read_text()


class LocalInteractionTests(unittest.TestCase):
    def compile_and_run(self, driver, extra_sources=()):
        compiler = shlex.split(os.environ.get("CXX", "c++"))
        if not compiler or shutil.which(compiler[0]) is None:
            self.skipTest("requires a host C++23 compiler")
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)
            (path / "esp_log.h").write_text(
                "#define ESP_LOGI(...) ((void)0)\n#define ESP_LOGW(...) ((void)0)\n"
                "#define ESP_LOGE(...) ((void)0)\n#define ESP_LOGD(...) ((void)0)\n")
            (path / "esp_system.h").write_text(
                '#include <cstdlib>\ninline void esp_system_abort(const char*) { std::abort(); }\n')
            source = path / "local_interaction.cc"
            source.write_text(driver)
            executable = path / "local_interaction"
            compiled = subprocess.run(
                compiler + ["-std=c++23", "-pthread", "-I", str(path),
                            "-I", str(ROOT / "main/audio"),
                            "-I", str(ROOT / "main/audio/demuxer"),
                            str(source), *map(str, extra_sources), "-o", str(executable)],
                capture_output=True, text=True,
            )
            self.assertEqual(compiled.returncode, 0, compiled.stdout + compiled.stderr)
            result = subprocess.run([str(executable)], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_lifecycle_generation_nested_work_and_safe_exit(self):
        signatures = [
            "bool Application::SetDeviceState(",
            "bool Application::BeginLocalInteraction()",
            "void Application::EndLocalInteraction()",
            "void Application::FinishLocalInteraction()",
            "void Application::RestoreProvisioningAlert()",
            "void Application::Alert(",
            "bool Application::PlayLocalInteractionSound(",
            "void Application::ScheduleInteraction(",
            "void Application::RunInteractionCallback(",
            "void Application::Schedule(",
        ]
        platform = r'''
            #include <atomic>
            #include <cassert>
            #include <cstdint>
            #include <deque>
            #include <functional>
            #include <memory>
            #include <mutex>
            #include <string>
            #include <string_view>
            #include "esp_log.h"
            using TaskHandle_t = int;
            int current_task = 1;
            int xTaskGetCurrentTaskHandle() { return current_task; }
            void xEventGroupClearBits(uint32_t* events, uint32_t bits) { *events &= ~bits; }
            void xEventGroupSetBits(uint32_t* events, uint32_t bits) { *events |= bits; }
            constexpr uint32_t MAIN_EVENT_TOGGLE_CHAT = 1 << 0;
            constexpr uint32_t MAIN_EVENT_START_LISTENING = 1 << 1;
            constexpr uint32_t MAIN_EVENT_STOP_LISTENING = 1 << 2;
            constexpr uint32_t MAIN_EVENT_WAKE_WORD_DETECTED = 1 << 3;
            constexpr uint32_t MAIN_EVENT_SEND_AUDIO = 1 << 4;
            constexpr uint32_t MAIN_EVENT_ERROR = 1 << 5;
            constexpr uint32_t MAIN_EVENT_SCHEDULE = 1 << 6;
            enum DeviceState { kDeviceStateUnknown, kDeviceStateStarting,
                kDeviceStateWifiConfiguring, kDeviceStateIdle, kDeviceStateConnecting,
                kDeviceStateListening, kDeviceStateSpeaking, kDeviceStateNotifying,
                kDeviceStateActivating, kDeviceStateUpgrading, kDeviceStateAudioTesting };
            struct StateMachine {
                DeviceState state = kDeviceStateIdle;
                bool TransitionTo(DeviceState next) { state = next; return true; }
            };
            enum class PowerSaveLevel { LOW_POWER };
            namespace Lang::Strings { const char* WIFI_CONFIG_MODE = "Wi-Fi setup"; }
            struct Display {
                std::string status, message, emotion;
                void SetStatus(const char* value) { status = value; }
                void SetEmotion(const char* value) { emotion = value; }
                void SetChatMessage(const char*, const char* value) { message = value; }
            };
            struct Board {
                Display display;
                Display* GetDisplay() { return &display; }
                static Board& GetInstance() { static Board b; return b; }
                void SetPowerSaveLevel(PowerSaveLevel) {}
            };
            struct Protocol {
                bool open = true;
                int closed = 0, stopped = 0;
                bool IsAudioChannelOpened() { return open; }
                void CloseAudioChannel() { open = false; ++closed; }
                void SendStopListening() { ++stopped; }
            };
            struct NotifyPlayer {
                bool busy = false;
                int stops = 0;
                void Stop() { ++stops; }
                bool IsBusy() { return busy; }
            };
            struct AudioService {
                bool local = false, idle = true, wake = true, voice = true;
                int resets = 0, plays = 0, ordinary_plays = 0;
                void PlaySound(const std::string_view&) { ++ordinary_plays; }
                void SetLocalInteractionActive(bool value) {
                    local = value; if (value) wake = voice = false;
                }
                void ResetDecoder() { ++resets; }
                bool IsPlaybackIdle() { return idle; }
                bool PlayLocalInteractionSound(std::string_view) {
                    if (!local || !idle) return false;
                    idle = false; ++plays; return true;
                }
            };
            constexpr int kAbortReasonNone = 0;
            class Application {
            public:
                std::mutex mutex_, incoming_audio_mutex_;
                std::deque<std::function<void()>> main_tasks_;
                std::unique_ptr<Protocol> protocol_ = std::make_unique<Protocol>();
                StateMachine state_machine_;
                AudioService audio_service_;
                NotifyPlayer notify_player_;
                TaskHandle_t main_task_handle_ = 1;
                std::atomic<bool> local_interaction_active_{false}, activation_in_progress_{false};
                std::atomic<uint32_t> interaction_generation_{0};
                std::atomic<bool> remote_chat_suspended_{false}, network_error_pending_{false};
                bool dispatching_interaction_ = false;
                uint32_t dispatch_generation_ = 0;
                bool local_interaction_ending_ = false;
                std::atomic<bool> network_connected_{false};
                std::string provisioning_status_, provisioning_message_, provisioning_emotion_;
                bool startup_completed_ = true, pending_listening_start_ = true;
                bool play_popup_on_listening_ = true, pending_startup_wake_word_hint_ = true;
                uint32_t notification_playback_id_ = 0, events = 0;
                uint32_t* event_group_ = &events;
                std::string last_error_message_;
                int aborts = 0, network_events = 0, hint_hides = 0;
                bool IsLocalInteractionActive() const { return local_interaction_active_.load(); }
                DeviceState GetDeviceState() const { return state_machine_.state; }
                void HideStartupWakeWordHint() { ++hint_hides; }
                void AbortSpeaking(int) { ++aborts; }
                void HandleNetworkConnectedEvent() {
                    ++network_events;
                    if (GetDeviceState() == kDeviceStateStarting ||
                        GetDeviceState() == kDeviceStateWifiConfiguring) {
                        state_machine_.state = kDeviceStateActivating;
                    }
                }
                void HandleStateChangedEvent() {
                    audio_service_.wake = GetDeviceState() == kDeviceStateIdle;
                    audio_service_.voice = false;
                }
                bool SetDeviceState(DeviceState);
                bool BeginLocalInteraction();
                void EndLocalInteraction();
                void FinishLocalInteraction();
                void RestoreProvisioningAlert();
                void Alert(const char*, const char*, const char*, const std::string_view& = "");
                bool PlayLocalInteractionSound(std::string_view);
                void ScheduleInteraction(uint32_t, std::function<void()>&&);
                void RunInteractionCallback(uint32_t, const std::function<void()>&);
                void Schedule(std::function<void()>&&);
                void Flush() {
                    auto tasks = std::move(main_tasks_);
                    main_tasks_.clear();
                    for (auto& task : tasks) task();
                }
            };
        '''
        checks = r'''
            int main() {
                for (auto state : {kDeviceStateActivating, kDeviceStateUpgrading, kDeviceStateAudioTesting}) {
                    Application app;
                    app.state_machine_.state = state;
                    assert(!app.BeginLocalInteraction());
                    assert(!app.IsLocalInteractionActive());
                }
                Application early;
                early.startup_completed_ = false;
                assert(!early.BeginLocalInteraction());
                Application cancelled_activation;
                cancelled_activation.activation_in_progress_ = true;
                assert(!cancelled_activation.BeginLocalInteraction());
                Application offline;
                offline.startup_completed_ = false;
                offline.state_machine_.state = kDeviceStateStarting;
                assert(offline.BeginLocalInteraction());
                assert(offline.GetDeviceState() == kDeviceStateStarting);
                offline.EndLocalInteraction();
                assert(!offline.IsLocalInteractionActive());
                assert(offline.GetDeviceState() == kDeviceStateStarting);
                assert(!offline.audio_service_.wake);
                assert(offline.BeginLocalInteraction());
                offline.network_connected_ = true;
                offline.EndLocalInteraction();
                assert(offline.network_events == 1);
                current_task = 2;
                Application wrong_task;
                assert(!wrong_task.BeginLocalInteraction());
                current_task = 1;

                for (auto state : {kDeviceStateIdle, kDeviceStateConnecting, kDeviceStateListening,
                                   kDeviceStateSpeaking, kDeviceStateNotifying}) {
                    Application app;
                    app.state_machine_.state = state;
                    int stale = 0;
                    app.ScheduleInteraction(0, [&] { ++stale; });
                    assert(app.BeginLocalInteraction());
                    assert(app.IsLocalInteractionActive());
                    assert(app.GetDeviceState() == kDeviceStateIdle);
                    assert(!app.audio_service_.wake && !app.audio_service_.voice);
                    assert(app.protocol_->closed == 1);
                    assert(app.protocol_->stopped == (state == kDeviceStateListening));
                    assert(app.aborts == (state == kDeviceStateSpeaking));
                    assert(!app.pending_listening_start_ && !app.play_popup_on_listening_);
                    assert(!app.SetDeviceState(kDeviceStateListening));
                    app.ScheduleInteraction(app.interaction_generation_, [&] { ++stale; });
                    app.Flush();
                    assert(stale == 0);
                    assert(app.PlayLocalInteractionSound("clip"));
                    assert(!app.PlayLocalInteractionSound("clip"));
                    app.EndLocalInteraction();
                    assert(app.IsLocalInteractionActive());
                    assert(!app.BeginLocalInteraction());
                    assert(!app.PlayLocalInteractionSound("clip"));
                    assert(!app.audio_service_.wake);
                    app.audio_service_.idle = true;
                    app.FinishLocalInteraction();
                    assert(!app.IsLocalInteractionActive());
                    assert(app.audio_service_.wake && !app.audio_service_.voice);
                    assert(app.remote_chat_suspended_);  // Fresh user wake is required.
                    app.ScheduleInteraction(0, [&] { ++stale; });
                    app.Flush();
                    assert(stale == 0);
                    app.ScheduleInteraction(app.interaction_generation_, [&] { ++stale; });
                    app.Flush();
                    assert(stale == 1);
                }

                Application nested;
                int stale_mcp = 0;
                nested.ScheduleInteraction(0, [&] {
                    nested.Schedule([&] {
                        nested.Schedule([&] { ++stale_mcp; });
                    });
                });
                nested.Flush();
                nested.Flush();
                assert(nested.BeginLocalInteraction());
                nested.EndLocalInteraction();
                nested.Flush();
                assert(stale_mcp == 0);

                Application notification;
                notification.notify_player_.busy = true;
                assert(notification.BeginLocalInteraction());
                notification.EndLocalInteraction();
                assert(notification.IsLocalInteractionActive());
                notification.notify_player_.busy = false;
                notification.SetDeviceState(kDeviceStateWifiConfiguring);
                notification.network_connected_ = true;
                notification.FinishLocalInteraction();
                assert(!notification.IsLocalInteractionActive());
                assert(!notification.audio_service_.wake);
                assert(notification.network_events == 1);
                assert(notification.GetDeviceState() == kDeviceStateActivating);

                Application provisioning;
                provisioning.startup_completed_ = false;
                provisioning.state_machine_.state = kDeviceStateWifiConfiguring;
                provisioning.Alert("Setup", "Connect to the hotspot", "gear");
                assert(provisioning.BeginLocalInteraction());
                auto display = Board::GetInstance().GetDisplay();
                display->message = "Game";
                provisioning.Alert("Setup", "Updated hotspot instructions", "gear", "sound");
                assert(display->message == "Game");
                provisioning.EndLocalInteraction();
                assert(display->message == "Updated hotspot instructions");
                assert(provisioning.audio_service_.ordinary_plays == 0);
                assert(!provisioning.audio_service_.wake);

                // Coalesced network bits cannot replace the latest callback state.
                for (bool last_connected : {false, true}) {
                    Application coalesced;
                    coalesced.startup_completed_ = false;
                    coalesced.state_machine_.state = kDeviceStateWifiConfiguring;
                    assert(coalesced.BeginLocalInteraction());
                    coalesced.network_connected_ = !last_connected;
                    coalesced.network_connected_ = last_connected;
                    coalesced.EndLocalInteraction();
                    assert(coalesced.network_events == (last_connected ? 1 : 0));
                    assert(coalesced.GetDeviceState() == (last_connected
                        ? kDeviceStateActivating : kDeviceStateWifiConfiguring));
                }
            }
        '''
        self.compile_and_run(textwrap.dedent(platform) +
                             "\n".join(definition(APP, sig) for sig in signatures) +
                             textwrap.dedent(checks))

    def test_bounded_ogg_admission_and_remote_audio_rejection(self):
        platform = r'''
            #include <atomic>
            #include <cassert>
            #include <condition_variable>
            #include <cstdint>
            #include <fstream>
            #include <memory>
            #include <mutex>
            #include <string>
            #include <string_view>
            #include <vector>
            #include "fixed_queue.h"
            #include "ogg_demuxer.h"
            constexpr int MAX_DECODE_PACKETS_IN_QUEUE = 20;
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
                bool playback_drained_notified_ = true, decode_in_flight_ = false;
                bool output_in_flight_ = false;
                FixedQueue<std::unique_ptr<AudioStreamPacket>, 166> audio_decode_queue_;
                FixedQueue<int, 2> audio_playback_queue_;
                bool PlayLocalInteractionSound(std::string_view);
                bool PushPacketToDecodeQueue(std::unique_ptr<AudioStreamPacket>, bool = false);
                bool IsPlaybackDrainedLocked() const;
            };
            std::string Read(const char* path) {
                std::ifstream stream(path, std::ios::binary);
                return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
            }
        '''
        # Real repository popup and upgrade assets cover accepted vs oversized/long clips.
        popup = ROOT / "main/assets/common/popup.ogg"
        if not popup.exists():
            popup = next((ROOT / "main/assets").rglob("popup.ogg"))
        upgrade = ROOT / "main/assets/locales/en-US/upgrade.ogg"
        checks = r'''
            int main() {
                auto clip = Read(POPUP_PATH);
                assert(!clip.empty());
                AudioService audio;
                assert(!audio.PlayLocalInteractionSound(clip));
                audio.local_interaction_active_ = true;
                assert(!audio.PushPacketToDecodeQueue(std::make_unique<AudioStreamPacket>(), true));
                assert(audio.PlayLocalInteractionSound(clip));
                auto count = audio.audio_decode_queue_.size();
                assert(count > 0 && count <= 20);
                assert(!audio.PlayLocalInteractionSound(clip));
                assert(audio.audio_decode_queue_.size() == count);
                audio.audio_decode_queue_.clear();
                audio.output_in_flight_ = true;
                assert(!audio.PlayLocalInteractionSound(clip));
                audio.output_in_flight_ = false;
                audio.decode_in_flight_ = true;
                assert(!audio.PlayLocalInteractionSound(clip));
                audio.decode_in_flight_ = false;
                assert(!audio.PlayLocalInteractionSound("not Ogg"));
                assert(!audio.PlayLocalInteractionSound(clip.substr(0, clip.size() - 1)));
                assert(!audio.PlayLocalInteractionSound(std::string(24577, 'x')));
                assert(!audio.PlayLocalInteractionSound(Read(UPGRADE_PATH)));
                assert(audio.audio_decode_queue_.empty());
                audio.service_stopped_ = true;
                assert(!audio.PlayLocalInteractionSound(clip));
                audio.service_stopped_ = false;
                audio.local_interaction_active_ = false;
                assert(audio.PushPacketToDecodeQueue(std::make_unique<AudioStreamPacket>()));
            }
        '''
        driver = textwrap.dedent(platform) + "\n".join(definition(AUDIO, sig) for sig in [
            "bool AudioService::PlayLocalInteractionSound(",
            "bool AudioService::PushPacketToDecodeQueue(",
            "bool AudioService::IsPlaybackDrainedLocked()",
        ]) + textwrap.dedent(checks).replace("POPUP_PATH", repr(str(popup)).replace("'", '"')).replace(
            "UPGRADE_PATH", repr(str(upgrade)).replace("'", '"'))
        self.compile_and_run(driver, [ROOT / "main/audio/demuxer/ogg_demuxer.cc"])

    def test_capture_and_main_loop_guards(self):
        for signature in [
            "void Application::HandleToggleChatEvent()",
            "void Application::HandleStartListeningEvent()",
            "void Application::HandleStopListeningEvent()",
            "void Application::HandleWakeWordDetectedEvent()",
            "void Application::ContinueOpenAudioChannel(",
            "void Application::ContinueWakeWordInvoke(",
            "void Application::StartListeningAudio()",
            "void Application::HandleStateChangedEvent()",
        ]:
            self.assertIn("if (IsLocalInteractionActive())", definition(APP, signature))
        initialization = definition(APP, "void Application::Initialize()")
        callback = initialization[initialization.index("board.SetNetworkEventCallback"):]
        self.assertLess(callback.index("network_connected_.store(true)"),
                        callback.index("if (IsLocalInteractionActive())"))
        self.assertLess(callback.index("network_connected_.store(false)"),
                        callback.index("if (IsLocalInteractionActive())"))
        run = definition(APP, "void Application::Run()")
        self.assertLess(run.index("bits &= ~"), run.index("FinishLocalInteraction();"))
        capture = definition(AUDIO, "bool AudioService::ReadAudioData(")
        self.assertEqual(capture.count("if (local_interaction_active_.load())"), 2)
        encoder = definition(AUDIO, "void AudioService::OpusCodecTask()")
        self.assertEqual(encoder.count("input_generation != input_generation_"), 2)
        input_task = definition(AUDIO, "void AudioService::AudioInputTask()")
        self.assertIn("if (local_interaction_active_.load())", input_task)
        self.assertIn("codec_->EnableInput(false);", input_task)
        local_sound = definition(AUDIO, "bool AudioService::PlayLocalInteractionSound(")
        for forbidden in ["OutputData", "SetOutputVolume", "Settings", "wait(", "vTaskDelay"]:
            self.assertNotIn(forbidden, local_sound)


if __name__ == "__main__":
    unittest.main()
