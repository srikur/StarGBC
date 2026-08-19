#include "SDLFrontend.h"

#include <cstdio>
#include <fstream>
#include <string_view>
#include <thread>

SDLFrontend::~SDLFrontend() {
    if (audioStream_) {
        SDL_DestroyAudioStream(audioStream_);
        audioStream_ = nullptr;
    }
    if (texture_) {
        SDL_DestroyTexture(texture_);
        texture_ = nullptr;
    }
    if (renderer_) {
        SDL_DestroyRenderer(renderer_);
        renderer_ = nullptr;
    }
    if (window_) {
        SDL_DestroyWindow(window_);
        window_ = nullptr;
    }
    SDL_Quit();
}

SDL_AppResult SDLFrontend::Init(const int argc, char *argv[]) {
    SDL_SetAppMetadata("StarGBC", "0.0.1", "com.srikur.stargbc");

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
        SDL_Log("Couldn't initialise SDL: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    const std::vector<std::string_view> args(argv + 1, argv + argc);
    GameboySettings settings{};
    for (std::size_t i = 0; i < args.size(); ++i) {
        if (args[i] == "--anti-aliasing") {
            useNearest_ = false;
        } else if (args[i] == "--gbc") {
            settings.mode = Mode::CGB_GBC;
        } else if (args[i] == "--gb") {
            settings.mode = Mode::DMG;
        } else if (args[i] == "--debugStart") {
            paused_ = true;
        } else if (args[i] == "--unthrottled") {
            throttled_ = false;
        } else if (args[i] == "--realRTC") {
            settings.realRTC = true;
        } else if (args[i] == "--bios") {
            if (i + 1 < args.size()) {
                settings.biosPath = args[++i];
            } else {
                std::fprintf(stderr, "Error: --bios requires a path argument\n");
                return SDL_APP_FAILURE;
            }
        } else if (args[i] == "--no-bootrom") {
            settings.noBootrom = true;
        } else if (i == args.size() - 1 ||
                   args[i].ends_with(".gb") || args[i].ends_with(".gbc")) {
            settings.romName = args[i];
        } else {
            std::fprintf(stderr, "USAGE: StarGBC [options] romFile\n"
                         "Options:\n"
                         "  --gbc | --gb        force gbc/dmg mode\n"
                         "  --bios <path>       external BIOS ROM\n"
                         "  --no-bootrom        skip built-in bootrom; jump straight to cart\n"
                         "  --anti-aliasing     linear-filter pixels");
            return SDL_APP_FAILURE;
        }
    }
    if (settings.romName.empty() || (!settings.romName.ends_with(".gb") &&
                                     !settings.romName.ends_with(".gbc"))) {
        std::fprintf(stderr, "Error: no ROM specified");
        return SDL_APP_FAILURE;
    }
    romPath_ = settings.romName;

    if (!SDL_CreateWindowAndRenderer("StarGBC",
                                     GB_SCREEN_W * WINDOW_SCALE, GB_SCREEN_H * WINDOW_SCALE,
                                     SDL_WINDOW_RESIZABLE,
                                     &window_, &renderer_)) {
        SDL_Log("CreateWindowAndRenderer: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    SDL_SetRenderLogicalPresentation(renderer_,
                                     GB_SCREEN_W, GB_SCREEN_H,
                                     SDL_LOGICAL_PRESENTATION_INTEGER_SCALE);

    texture_ = SDL_CreateTexture(renderer_,
                                 SDL_PIXELFORMAT_RGBA32,
                                 SDL_TEXTUREACCESS_STREAMING,
                                 GB_SCREEN_W, GB_SCREEN_H);
    if (!texture_) {
        SDL_Log("CreateTexture: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    SDL_SetTextureScaleMode(texture_,
                            useNearest_ ? SDL_SCALEMODE_NEAREST : SDL_SCALEMODE_LINEAR);
    gameboy_ = Gameboy::init(settings);

    SDL_AudioSpec audioSpec{};
    audioSpec.freq = AUDIO_SAMPLE_RATE;
    audioSpec.format = SDL_AUDIO_F32;
    audioSpec.channels = 2;

    audioStream_ = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &audioSpec, nullptr, nullptr);
    if (!audioStream_) {
        SDL_Log("Failed to create audio stream: %s", SDL_GetError());
        audioEnabled_ = false;
    } else {
        SDL_ResumeAudioStreamDevice(audioStream_);
    }

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDLFrontend::HandleEvent(const SDL_Event &event) {
    switch (event.type) {
        case SDL_EVENT_QUIT:
            return SDL_APP_SUCCESS;

        case SDL_EVENT_WINDOW_EXPOSED:
            SDL_RenderTexture(renderer_, texture_, nullptr, nullptr);
            SDL_RenderPresent(renderer_);
            break;

        case SDL_EVENT_KEY_DOWN:
            return HandleKeyDown(event.key);

        case SDL_EVENT_KEY_UP:
            HandleKeyUp(event.key);
            break;

        default: break;
    }
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDLFrontend::HandleKeyDown(const SDL_KeyboardEvent &key) {
    switch (key.key) {
        case SDLK_ESCAPE: return SDL_APP_SUCCESS;
        case SDLK_Z: gameboy_->KeyDown(Keys::A);
            break;
        case SDLK_X: gameboy_->KeyDown(Keys::B);
            break;
        case SDLK_RETURN: gameboy_->KeyDown(Keys::Start);
            break;
        case SDLK_BACKSPACE: gameboy_->KeyDown(Keys::Select);
            break;
        case SDLK_RIGHT: gameboy_->KeyDown(Keys::Right);
            break;
        case SDLK_LEFT: gameboy_->KeyDown(Keys::Left);
            break;
        case SDLK_UP: gameboy_->KeyDown(Keys::Up);
            break;
        case SDLK_DOWN: gameboy_->KeyDown(Keys::Down);
            break;
        case SDLK_SPACE: throttled_ = false;
            break;
        case SDLK_M: speedMultiplier_ = speedMultiplier_ == 1 ? 4 : 1;
            break;
        case SDLK_P: paused_ = true;
            break;
        case SDLK_R: paused_ = false;
            break;
        case SDLK_F2:
            SaveScreenshot();
            break;
        case SDLK_N:
            audioEnabled_ = !audioEnabled_;
            if (audioStream_) {
                if (audioEnabled_) {
                    SDL_ResumeAudioStreamDevice(audioStream_);
                } else {
                    SDL_PauseAudioStreamDevice(audioStream_);
                }
            }
            break;
        // Save states with Shift + 1-7, load with Ctrl + 1-7
        case SDLK_1:
        case SDLK_2:
        case SDLK_3:
        case SDLK_4:
        case SDLK_5:
        case SDLK_6:
        case SDLK_7: {
            const auto slot = static_cast<uint8_t>(key.key - SDLK_1 + 1);
            if (key.mod & SDL_KMOD_LSHIFT) SaveState(slot);
            else if (key.mod & SDL_KMOD_LCTRL) LoadState(slot);
            break;
        }
        default: break;
    }
    return SDL_APP_CONTINUE;
}

void SDLFrontend::HandleKeyUp(const SDL_KeyboardEvent &key) {
    switch (key.key) {
        case SDLK_Z: gameboy_->KeyUp(Keys::A);
            break;
        case SDLK_X: gameboy_->KeyUp(Keys::B);
            break;
        case SDLK_RETURN: gameboy_->KeyUp(Keys::Start);
            break;
        case SDLK_BACKSPACE: gameboy_->KeyUp(Keys::Select);
            break;
        case SDLK_RIGHT: gameboy_->KeyUp(Keys::Right);
            break;
        case SDLK_LEFT: gameboy_->KeyUp(Keys::Left);
            break;
        case SDLK_UP: gameboy_->KeyUp(Keys::Up);
            break;
        case SDLK_DOWN: gameboy_->KeyUp(Keys::Down);
            break;
        case SDLK_SPACE: throttled_ = true;
            break;
        default: break;
    }
}

SDL_AppResult SDLFrontend::Iterate() {
    if (!paused_) {
        gameboy_->RunFrame();
        ThrottleFrame();
    }

    if (gameboy_->ConsumeFrame()) {
        PresentFrame();
    }

    PumpAudio();

    return SDL_APP_CONTINUE;
}

void SDLFrontend::ThrottleFrame() {
    using clock = std::chrono::steady_clock;
    if (throttled_) {
        nextFrameTime_ += Gameboy::FRAME_PERIOD / speedMultiplier_;
        if (const auto now = clock::now(); nextFrameTime_ > now) {
            std::this_thread::sleep_until(nextFrameTime_);
        } else {
            nextFrameTime_ = now; // fell behind; don't try to catch up in a burst
        }
    } else {
        nextFrameTime_ = clock::now();
    }
}

void SDLFrontend::PresentFrame() const {
    SDL_UpdateTexture(texture_,
                      nullptr,
                      gameboy_->GetScreenData(),
                      GB_SCREEN_W * sizeof(uint32_t));

    SDL_RenderTexture(renderer_, texture_, nullptr, nullptr);
    SDL_RenderPresent(renderer_);
}

void SDLFrontend::PumpAudio() {
    if (!audioEnabled_ || !audioStream_) return;

    // Drain the emulator's ring buffer completely every frame: a throttled frame yields
    // ~804 sample frames, and anything left behind overflows the ring and gets dropped
    if (const size_t samplesRead = gameboy_->ReadAudioSamples(audioBuffer_.data(), AUDIO_BUFFER_SIZE);
        samplesRead > 0 && SDL_GetAudioStreamQueued(audioStream_) <= MAX_AUDIO_QUEUE_BYTES) {
        // When the queue is backed up (unthrottled/4x speed), samples are consumed but dropped
        SDL_PutAudioStreamData(audioStream_, audioBuffer_.data(),
                               static_cast<int>(samplesRead * 2 * sizeof(float)));
    }
}

void SDLFrontend::SaveScreenshot() const {
    try {
        std::ofstream file(romPath_ + ".screen", std::ios::binary | std::ios::trunc);
        if (!file.is_open()) throw std::runtime_error("Could not open " + romPath_ + ".screen");
        file.write(reinterpret_cast<const char *>(gameboy_->GetScreenData()), GB_SCREEN_W * GB_SCREEN_H * 4);
        std::fprintf(stderr, "Saved screen to %s.screen\n", romPath_.c_str());
    } catch (const std::exception &e) {
        std::fprintf(stderr, "Failed to save screen: %s\n", e.what());
    }
}

void SDLFrontend::SaveState(const uint8_t slot) const {
    try {
        const std::string filename = std::format("{}.sv{}", romPath_, slot);
        std::ofstream file(filename, std::ios::binary | std::ios::trunc);
        if (!file.is_open()) throw std::runtime_error("Failed to save: " + filename);
        const auto saveBytes = gameboy_->SaveState();
    } catch (const std::exception &e) {

    }
}

void SDLFrontend::LoadState(const uint8_t slot) {

}