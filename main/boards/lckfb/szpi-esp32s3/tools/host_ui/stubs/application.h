#pragma once
#include <deque>
#include <functional>
#include <string_view>
class AudioService {
public:
    bool busy = false;
    bool IsPlaybackIdle() { return !busy; }
    void ResetDecoder() { busy = false; }
};
class Application {
public:
    static Application& GetInstance() {
        static Application a;
        return a;
    }
    AudioService audio;
    int sounds = 0;
    std::string_view last;
    bool active = false;
    bool deferred = false;
    std::deque<std::function<void()>> pending;
    void Schedule(std::function<void()>&& f) {
        if (deferred)
            pending.push_back(std::move(f));
        else
            f();
    }
    void Flush() {
        auto ready = std::move(pending);
        pending.clear();
        for (auto& f : ready)
            f();
    }
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
        last = s;
        audio.busy = true;
        return true;
    }
    AudioService& GetAudioService() { return audio; }
};
