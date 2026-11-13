#pragma once

// Opt-in internal Player harness. Buffered observations never enter the SDK API.
#if defined(CUEXIS_PLAYBACK_TRACE_HARNESS)
#include "../../../tests/pd/measurement_environment.hpp"
#include "player_surface.hpp"
#include <SDL3/SDL.h>
#include <Windows.h>
#include <algorithm>
#include <audiopolicy.h>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <mmdeviceapi.h>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace cuexis::player {
class ScriptedPlaybackTrace final {
    struct Event {
        std::int64_t offset{};
        unsigned kind{}, code{}, value{};
    };
    struct Sample {
        std::string metric;
        std::int64_t start{}, confirmed{}, end{};
    };
    struct Frame {
        std::int64_t at{}, request{}, ready{};
    };
    std::vector<Event> events_;
    std::vector<Sample> samples_;
    std::vector<Frame> frames_;
    std::size_t next_{};
    std::int64_t origin_{}, firstQpc_{}, activeFinishedAt_{};
    std::int64_t pendingStart_{}, confirmed_{};
    std::string pending_;
    std::string endpoint_;
    std::uint64_t audioReads_{}, audioSequence_{}, underruns_{}, captured_{};
    std::int64_t finalScore_{};
    std::uint64_t finalHits_{}, finalMisses_{};
    double maximumSourcePositionMs_{};
    bool audioEnded_{}, terminalExecuted_{}, judgementSeen_{};
    unsigned actions_{};
    bool active_{};
    static auto now() -> std::int64_t {
        return std::chrono::duration_cast<std::chrono::microseconds>(
                   std::chrono::steady_clock::now().time_since_epoch())
            .count();
    }
    static void check(HRESULT value) {
        if (FAILED(value))
            throw std::runtime_error("PD5 actual audio endpoint query failed, HRESULT=" +
                                     std::to_string(static_cast<long>(value)));
    }
    template <class T> struct Com {
        T* p{};
        ~Com() {
            if (p)
                p->Release();
        }
        auto operator->() const -> T* {
            return p;
        }
    };
    static auto endpoint() -> std::string {
        const auto initialized = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        // SDL may already own an STA on the main thread. Borrow that apartment without
        // uninitializing it; balance only this function's successful initialization.
        if (initialized != RPC_E_CHANGED_MODE)
            check(initialized);
        struct Uninit {
            bool owned;
            ~Uninit() {
                if (owned)
                    CoUninitialize();
            }
        } uninit{SUCCEEDED(initialized)};
        Com<IMMDeviceEnumerator> enumerator;
        check(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                               IID_PPV_ARGS(&enumerator.p)));
        Com<IMMDeviceCollection> devices;
        check(enumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &devices.p));
        UINT count{};
        check(devices->GetCount(&count));
        std::string found;
        for (UINT d = 0; d < count; ++d) {
            Com<IMMDevice> device;
            check(devices->Item(d, &device.p));
            Com<IAudioSessionManager2> manager;
            check(device->Activate(__uuidof(IAudioSessionManager2), CLSCTX_ALL, nullptr,
                                   reinterpret_cast<void**>(&manager.p)));
            Com<IAudioSessionEnumerator> sessions;
            check(manager->GetSessionEnumerator(&sessions.p));
            int total{};
            check(sessions->GetCount(&total));
            for (int s = 0; s < total; ++s) {
                Com<IAudioSessionControl> session;
                Com<IAudioSessionControl2> control;
                check(sessions->GetSession(s, &session.p));
                check(session->QueryInterface(IID_PPV_ARGS(&control.p)));
                DWORD pid{};
                check(control->GetProcessId(&pid));
                AudioSessionState state{};
                check(control->GetState(&state));
                if (pid != GetCurrentProcessId() || state == AudioSessionStateExpired)
                    continue;
                LPWSTR id{};
                check(device->GetId(&id));
                std::string value;
                for (const auto* ch = id; *ch; ++ch) {
                    if (*ch > 127) {
                        CoTaskMemFree(id);
                        throw std::runtime_error("Non-ASCII endpoint ID");
                    }
                    value.push_back(static_cast<char>(*ch));
                }
                CoTaskMemFree(id);
                if (!found.empty() && found != value)
                    throw std::runtime_error("PD5 process opened multiple audio endpoints");
                found = std::move(value);
            }
        }
        if (found.empty())
            throw std::runtime_error("PD5 opened stream endpoint unavailable");
        return found;
    }

  public:
    ScriptedPlaybackTrace() {
        char path[32768]{};
        std::size_t length{};
        if (getenv_s(&length, path, sizeof(path), "CUEXIS_PD5_PLAYER_SCRIPT") || length <= 1)
            return;
        std::ifstream input{path};
        if (!input)
            throw std::runtime_error("PD5 script missing");
        Event event;
        while (input >> event.offset >> event.kind >> event.code >> event.value) {
            if (events_.size() >= 4096 || event.offset < 0 || event.kind > 3 ||
                (!events_.empty() && event.offset < events_.back().offset))
                throw std::runtime_error("Invalid finite PD5 script");
            events_.push_back(event);
        }
        if (!input.eof() || events_.empty() || events_.back().kind != 3)
            throw std::runtime_error("PD5 script must end with explicit close");
        samples_.reserve(128);
        frames_.reserve(200000);
        active_ = true;
        pd_measure::environmentJson("begin");
    }
    auto active() const noexcept -> bool {
        return active_;
    }
    void pump(SDL_Window* window) {
        if (!active_ || !origin_)
            return;
        if (!(SDL_GetWindowFlags(window) & SDL_WINDOW_INPUT_FOCUS))
            throw std::runtime_error("PD5 foreground lost; process evidence invalid");
        if (next_ == events_.size() || now() - origin_ < events_[next_].offset)
            return;
        const auto e = events_[next_++];
        if (!pending_.empty())
            throw std::runtime_error("PD5 overlapping scripted controls");
        if (e.kind == 2) {
            begin("resize");
            if (!SDL_SetWindowSize(window, static_cast<int>(e.code), static_cast<int>(e.value)))
                throw std::runtime_error("PD5 resize failed");
            return;
        }
        if (e.kind == 3)
            begin("close");
        SDL_Event event{};
        event.type = e.kind == 3 ? SDL_EVENT_QUIT : e.value ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
        event.common.timestamp = SDL_GetTicksNS();
        if (e.kind != 3) {
            event.key.windowID = SDL_GetWindowID(window);
            event.key.scancode = static_cast<SDL_Scancode>(e.code);
            event.key.key = SDL_GetKeyFromScancode(event.key.scancode, SDL_KMOD_NONE, true);
            event.key.down = e.value != 0;
            event.key.repeat = false;
            if (e.kind == 0 && e.value)
                ++actions_;
        }
        if (!SDL_PushEvent(&event))
            throw std::runtime_error("PD5 SDL event rejected");
    }
    void begin(const char* metric) {
        if (!active_)
            return;
        if (!pending_.empty())
            throw std::runtime_error("PD5 control overlap");
        pending_ = metric;
        pendingStart_ = now();
        confirmed_ = 0;
    }
    // Called at UI admission, before the actual action is queued.
    void action(bool reload, bool paused) {
        if (!active_)
            return;
        begin(reload ? (paused ? "reset_paused" : "reset_running") : paused ? "resume" : "pause");
    }
    void confirmed(std::string_view metric = {}) {
        if (active_ && !pending_.empty() && (metric.empty() || pending_ == metric))
            confirmed_ = now();
    }
    void captured() noexcept {
        if (active_)
            ++captured_;
    }
    void audio(std::uint64_t sequence, std::uint64_t underruns) noexcept {
        if (active_) {
            ++audioReads_;
            audioSequence_ = std::max(audioSequence_, sequence);
            underruns_ = std::max(underruns_, underruns);
        }
    }
    void transport(double positionMs, bool ended) noexcept {
        if (active_) {
            maximumSourcePositionMs_ = std::max(maximumSourcePositionMs_, positionMs);
            audioEnded_ |= ended;
            if (ended && !activeFinishedAt_)
                activeFinishedAt_ = now();
        }
    }
    void terminalExecuted() noexcept {
        terminalExecuted_ = true;
        if (!activeFinishedAt_)
            activeFinishedAt_ = now();
    }
    void resumed() noexcept {
        terminalExecuted_ = false;
        audioEnded_ = false;
        activeFinishedAt_ = 0;
    }
    void judgement(std::int64_t score, std::uint64_t hits, std::uint64_t misses) noexcept {
        finalScore_ = score;
        finalHits_ = hits;
        finalMisses_ = misses;
        judgementSeen_ = true;
    }
    void firstPresent(bool hasAudio) {
        if (!active_ || origin_)
            return;
        LARGE_INTEGER qpc{};
        QueryPerformanceCounter(&qpc);
        firstQpc_ = qpc.QuadPart;
        origin_ = now();
        if (hasAudio)
            endpoint_ = endpoint();
    }
    void present(std::int64_t request, std::int64_t ready) {
        if (!active_)
            return;
        const auto at = now();
        if (!activeFinishedAt_) {
            if (frames_.size() == frames_.capacity())
                throw std::runtime_error("PD5 frame buffer full");
            frames_.push_back({at, request, ready});
        }
        if (confirmed_ && !pending_.empty() && pending_ != "close") {
            if (samples_.size() == samples_.capacity())
                throw std::runtime_error("PD5 control buffer full");
            samples_.push_back({std::move(pending_), pendingStart_, confirmed_, at});
            pending_.clear();
        }
    }
    void finish(bool hasAudio, bool replayEqual, bool gameplay = true) {
        if (!active_)
            return;
        if (next_ != events_.size() || pending_ != "close" || !origin_ || !replayEqual ||
            (hasAudio && (!audioReads_ || (gameplay && !audioSequence_))))
            throw std::runtime_error("PD5 Player trajectory did not complete");
        const auto closed = now();
        pd_measure::environmentJson("end");
        samples_.push_back({"close", pendingStart_, closed, closed});
        std::map<std::string, unsigned> counts;
        const auto sample = [&](const std::string& metric, std::int64_t start,
                                std::int64_t confirmed, std::int64_t end) {
            std::cout << "PD5_SAMPLE {\"metric\":\"" << metric
                      << "\",\"ordinal\":" << counts[metric]++ << ",\"elapsed_us\":" << end - start
                      << ",\"start_us\":" << start << ",\"confirmed_us\":" << confirmed
                      << ",\"first_present_us\":" << end << ",\"complete\":true}\n";
        };
        for (const auto& s : samples_)
            sample(s.metric, s.start, s.confirmed, s.end);
        for (std::size_t i = 0; i < frames_.size(); ++i) {
            const auto& f = frames_[i];
            sample("request_to_ready", f.request, f.ready, f.ready);
            if (i)
                sample("frame_interval", frames_[i - 1].at, frames_[i - 1].at, f.at);
        }
        LARGE_INTEGER frequency{};
        QueryPerformanceFrequency(&frequency);
        std::cout << "PD5_FIRST_FRAME {\"qpc\":" << firstQpc_
                  << ",\"frequency\":" << frequency.QuadPart << "}\n";
        std::cout << "PD5_PLAYER {\"script_events\":" << events_.size()
                  << ",\"dispatched\":" << next_ << ",\"actions\":" << actions_
                  << ",\"captured\":" << captured_ << ",\"frames\":" << frames_.size()
                  << ",\"trajectory_duration_us\":" << closed - origin_
                  << ",\"active_trajectory_duration_us\":"
                  << (activeFinishedAt_ ? activeFinishedAt_ : closed) - origin_
                  << ",\"terminal_wait_us\":"
                  << (activeFinishedAt_ ? closed - activeFinishedAt_ : 0)
                  << ",\"maximum_source_position_ms\":" << maximumSourcePositionMs_
                  << ",\"audio_ended\":" << (audioEnded_ ? "true" : "false")
                  << ",\"terminal_executed\":" << (terminalExecuted_ ? "true" : "false")
                  << ",\"judgement_seen\":" << (judgementSeen_ ? "true" : "false")
                  << ",\"score\":" << finalScore_ << ",\"hits\":" << finalHits_
                  << ",\"misses\":" << finalMisses_ << ",\"audio_reads\":" << audioReads_
                  << ",\"audio_sequence_max\":" << audioSequence_ << ",\"audio_boundary\":\""
                  << (gameplay ? "AudioPublication" : "IAudioTransport.snapshot")
                  << "\",\"underruns\":" << underruns_ << ",\"endpoint_id\":\"" << endpoint_
                  << "\",\"endpoint_method\":\"CoreAudio active render session matching native "
                     "PID\",\"foreground_checked\":true}\n";
        std::cout << "PD5_ID "
                     "{\"schema\":\"pd5.probe.v1\",\"harness\":\"player\",\"mode\":\"script\","
                     "\"metrics_compile_enabled\":false,\"allocator\":\"normal\",\"smoke\":false,"
                     "\"gpu\":true,\"audio\":"
                  << (hasAudio ? "true" : "false")
                  << ",\"gameplay\":" << (gameplay ? "true" : "false")
                  << ",\"closed\":true,\"replay_equal\":" << (gameplay ? "true" : "null")
                  << ",\"counts\":{";
        bool comma{};
        for (const auto& [metric, count] : counts) {
            std::cout << (comma ? "," : "") << '"' << metric << "\":" << count;
            comma = true;
        }
        std::cout << "}}\n";
    }
};
class ScriptedPlaybackSurface final : public PlayerSurface {
    std::unique_ptr<PlayerSurface> inner_;
    SDL_Window* window_;
    ScriptedPlaybackTrace& trace_;

  public:
    ScriptedPlaybackSurface(std::unique_ptr<PlayerSurface> inner, SDL_Window* window,
                            ScriptedPlaybackTrace& trace)
        : inner_(std::move(inner)), window_(window), trace_(trace) {}
    auto pollInput() -> core::Result<PlayerInput> override {
        trace_.pump(window_);
        return inner_->pollInput();
    }
    auto drawableSize() -> core::Result<PlayerDrawableSize> override {
        return inner_->drawableSize();
    }
};
} // namespace cuexis::player
#endif
