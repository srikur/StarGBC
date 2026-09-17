#include "SDLFrontend.h"

#include <cstdio>
#include <fstream>
#include <string_view>
#include <thread>
#include <print>

#include <starparse/starparse.hpp>

constexpr std::string_view kAppName = "StarGBC";
constexpr std::string_view kAppVersion = "0.0.1";
constexpr std::string_view kAppIdentifier = "com.srikur.stargbc";

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

struct [[=StarParse::Program{kAppName, "GBC Emulator", kAppVersion}]] Args {
    [[=StarParse::Positional{0}, =StarParse::Required{}]] std::string rom_path;
    [[=StarParse::Opt{'a', "Enable anti-aliasing"}]] bool anti_aliasing{true};
    [[=StarParse::Opt{"Start without speed limitations"}]] bool unthrottled{false};
    [[=StarParse::Opt{"Use real time clock"}]] bool real_rtc{false};
    [[=StarParse::Opt{"Start emulator paused"}]] bool debug_start{false};
    [[=StarParse::Opt{"Disable built in bootrom"}]] bool no_bootrom{false};
    [[=StarParse::Opt{'b', "Bios path"}, =StarParse::Alias{"bios"}]] std::string bios_path;
    [[=StarParse::Opt{'m', "Hardware model and revision to emulate"}]] Model model{Model::Auto};
};

SDL_AppResult SDLFrontend::Init(const int argc, char *argv[]) {
    SDL_SetAppMetadata(kAppName.data(), kAppVersion.data(), kAppIdentifier.data());

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
        SDL_Log("Couldn't initialise SDL: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    constexpr StarParse::Settings settings{.allow_case_insensitivity = true};
    const auto args{StarParse::parse_or_exit<Args>(argc, argv, settings)};
    if (args.help_requested()) {
        std::print("{}", args.help());
        return SDL_APP_SUCCESS;
    }
    if (args.version_requested()) {
        std::print("{}", args.version());
        return SDL_APP_SUCCESS;
    }
    romPath_ = args->rom_path;
    useNearest_ = args->anti_aliasing;
    paused_ = args->debug_start;
    throttled_ = !args->unthrottled;

    if (!SDL_CreateWindowAndRenderer(kAppName.data(),
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
    gameboy_ = Gameboy::init({
            .romName = args->rom_path,
            .biosPath = args->bios_path,
            .model = args->model,
            .noBootrom = args->no_bootrom,
            .realRTC = args->real_rtc
        }
    );

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

struct SaveStateHeader {
    std::array<char, 8> magic;
    uint64_t stateSize;
    uint32_t version;
    uint16_t cartChecksum;
    uint8_t model;
    uint8_t reserved;
};

static_assert(sizeof(SaveStateHeader) == 24 && std::has_unique_object_representations_v<SaveStateHeader>);

static constexpr std::array kStateMagic = {'S', 'T', 'A', 'R', 'G', 'B', 'C', '\0'};
static constexpr uint32_t kStateVersion = 1;

void SDLFrontend::SaveState(const uint8_t slot) const {
    try {
        const std::string filename = std::format("{}.sv{}", romPath_, slot);
        std::ofstream file(filename, std::ios::binary | std::ios::trunc);
        if (!file.is_open()) throw std::runtime_error("Failed to save: " + filename);
        const SaveStateHeader header{
            .magic = kStateMagic,
            .stateSize = kGameboyStateSize,
            .version = kStateVersion,
            .cartChecksum = gameboy_->CartChecksum(),
            .model = static_cast<uint8_t>(gameboy_->GetModel()),
            .reserved = 0,
        };
        const auto saveBytes = gameboy_->SaveState();
        file.write(reinterpret_cast<const char *>(&header), sizeof(header));
        file.write(reinterpret_cast<const char *>(saveBytes.data()), saveBytes.size());
        std::fprintf(stderr, "Saved state %u to %s\n", slot, filename.c_str());
    } catch (const std::exception &e) {
        std::fprintf(stderr, "Failed to save state: %s\n", e.what());
    }
}

void SDLFrontend::LoadState(const uint8_t slot) {
    try {
        const std::string filename = std::format("{}.sv{}", romPath_, slot);
        std::ifstream file(filename, std::ios::binary);
        if (!file.is_open()) {
            std::fprintf(stderr, "No save state in slot %u (%s)\n", slot, filename.c_str());
            return;
        }
        SaveStateHeader header{};
        file.read(reinterpret_cast<char *>(&header), sizeof(header));
        if (file.gcount() != static_cast<std::streamsize>(sizeof(header)) || header.magic != kStateMagic) {
            throw std::runtime_error("not a StarGBC save state");
        }
        if (header.version != kStateVersion || header.stateSize != kGameboyStateSize) {
            throw std::runtime_error("save state from an incompatible emulator version");
        }
        if (header.cartChecksum != gameboy_->CartChecksum()) {
            throw std::runtime_error("save state belongs to a different ROM");
        }
        if (header.model != static_cast<uint8_t>(gameboy_->GetModel())) {
            throw std::runtime_error("save state uses a different hardware model");
        }
        std::vector<std::byte> state(kGameboyStateSize);
        file.read(reinterpret_cast<char *>(state.data()), static_cast<std::streamsize>(state.size()));
        if (file.gcount() != static_cast<std::streamsize>(state.size())) {
            throw std::runtime_error("save state file is truncated");
        }
        if (!gameboy_->LoadState(state)) {
            throw std::runtime_error("save state failed validation");
        }
        if (audioStream_) {
            SDL_ClearAudioStream(audioStream_);
        }
        std::fprintf(stderr, "Loaded state %u from %s\n", slot, filename.c_str());
    } catch (const std::exception &e) {
        std::fprintf(stderr, "Failed to load state %u: %s\n", slot, e.what());
    }
}
