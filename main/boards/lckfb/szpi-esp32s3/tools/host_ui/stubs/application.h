#pragma once
#include <functional>
#include <string_view>
class AudioService {
public:
    bool busy = false;
    bool IsPlaybackIdle() { return !busy; }
};
class Application {
public:
    static Application& GetInstance() {
        static Application a;
        return a;
    }
    AudioService audio;
    int sounds = 0;
    bool active = false;
    void Schedule(std::function<void()>&& f) { f(); }
    bool BeginLocalInteraction() {
        active = true;
        return true;
    }
    void EndLocalInteraction() {
        active = false;
        audio.busy = false;
    }
    bool IsLocalInteractionActive() const { return active; }
    bool PlayLocalInteractionSound(std::string_view s) {
        if (!active || audio.busy || s.empty())
            return false;
        ++sounds;
        audio.busy = true;
        return true;
    }
    AudioService& GetAudioService() { return audio; }
};
