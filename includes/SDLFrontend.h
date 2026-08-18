#pragma once

#include <SDL3/SDL.h>
#include <chrono>
#include <memory>
#include <string>
#include <vector>

#include "Audio.h"
#include "Gameboy.h"

class SDLFrontend {
public:
    SDLFrontend() = default;

    ~SDLFrontend();

    SDLFrontend(const SDLFrontend &) = delete;

    SDLFrontend(SDLFrontend &&) = delete;

    SDLFrontend &operator=(const SDLFrontend &) = delete;

    SDLFrontend &operator=(SDLFrontend &&) = delete;

    [[nodiscard]] SDL_AppResult Init(int argc, char *argv[]);

    [[nodiscard]] SDL_AppResult HandleEvent(const SDL_Event &event);

    [[nodiscard]] SDL_AppResult Iterate();

private:
    static constexpr int GB_SCREEN_W = 160;
    static constexpr int GB_SCREEN_H = 144;
    static constexpr int WINDOW_SCALE = 3;
    static constexpr int MAX_AUDIO_QUEUE_BYTES = AUDIO_SAMPLE_RATE * 2 * sizeof(float) / 15;

    SDL_AppResult HandleKeyDown(const SDL_KeyboardEvent &key);

    void HandleKeyUp(const SDL_KeyboardEvent &key);

    void PresentFrame() const;

    void PumpAudio();

    void ThrottleFrame();

    void SaveScreenshot() const;

    SDL_Window *window_{nullptr};
    SDL_Renderer *renderer_{nullptr};
    SDL_Texture *texture_{nullptr};
    SDL_AudioStream *audioStream_{nullptr};
    std::unique_ptr<Gameboy> gameboy_;
    std::string romPath_;
    std::vector<float> audioBuffer_ = std::vector<float>(AUDIO_BUFFER_SIZE * 2);
    bool useNearest_{true};
    bool audioEnabled_{true};
    bool paused_{false};
    bool throttled_{true};
    int speedMultiplier_{1};
    std::chrono::steady_clock::time_point nextFrameTime_{};
};
