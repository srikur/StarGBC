#include "SDLFrontend.h"

#include <algorithm>
#include <cstdio>
#include <format>
#include <fstream>
#include <print>
#include <limits>
#include <string_view>

#include <starparse/starparse.hpp>

constexpr std::string_view kAppName = "StarGBC";
constexpr std::string_view kAppVersion = "0.0.1";
constexpr std::string_view kAppIdentifier = "com.srikur.stargbc";

SDLFrontend::~SDLFrontend() {
#ifdef STARGBC_ENABLE_LUA
    lua_.reset();
#endif
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

struct[[=StarParse::Program{kAppName, "GBC Emulator", kAppVersion}]] Args {
    [[=StarParse::Positional{0}, =StarParse::Required]] std::string rom_path;
    [[=StarParse::Opt{'a', "Enable anti-aliasing"}]] bool anti_aliasing{true};
    [[=StarParse::Opt{"Start without speed limitations"}]] bool unthrottled{false};
    [[=StarParse::Opt{"Use real time clock"}]] bool real_rtc{false};
    [[=StarParse::Opt{"Start emulator paused"}]] bool debug_start{false};
    [[=StarParse::Opt{"Disable built in bootrom"}]] bool no_bootrom{false};
    [[=StarParse::Opt{"Disable audio"}]] bool no_audio{false};
    [[=StarParse::Opt{'b', "Bios path"}, =StarParse::Alias{"bios"}]] std::string bios_path;
    [[=StarParse::Opt{'m', "Hardware model and revision to emulate"}]] Model model{Model::Auto};
    [[= StarParse::Opt{"Lua script to start/stop with Shift+F5"}]] std::string script;
    [[=StarParse::Opt{"Lua instructions per frame"}]] uint64_t script_instruction_budget{1'000'000};
    [[=StarParse::Opt{"Lua memory limit in MiB"}]] uint64_t script_memory_mib{32};
};

SDL_AppResult SDLFrontend::Init(const int argc, char *argv[]) {
    SDL_SetAppMetadata(kAppName.data(), kAppVersion.data(), kAppIdentifier.data());

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
    useNearest_ = !args->anti_aliasing;
    paused_ = args->debug_start;
    throttled_ = !args->unthrottled;
    audioEnabled_ = !args->no_audio;
    scriptPath_ = args->script;
#ifndef STARGBC_ENABLE_LUA
    if (!scriptPath_.empty()) {
        SDL_Log("This build does not include Lua scripting support");
        return SDL_APP_FAILURE;
    }
#endif
    if (!args->script_instruction_budget || args->script_instruction_budget > std::numeric_limits<uint64_t>::max() / 10 ||
        !args->script_memory_mib || args->script_memory_mib > std::numeric_limits<size_t>::max() / (1024 * 1024)) {
        SDL_Log("Script limits must be positive and fit in the supported range");
        return SDL_APP_FAILURE;
    }

    if (!SDL_Init(SDL_INIT_VIDEO | (audioEnabled_ ? SDL_INIT_AUDIO : 0))) {
        SDL_Log("Couldn't initialise SDL: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    if (!SDL_CreateWindowAndRenderer(kAppName.data(), GB_SCREEN_W * WINDOW_SCALE, GB_SCREEN_H * WINDOW_SCALE, SDL_WINDOW_RESIZABLE, &window_,
                                     &renderer_)) {
        SDL_Log("CreateWindowAndRenderer: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    SDL_SetRenderVSync(renderer_, SDL_RENDERER_VSYNC_DISABLED);

    SDL_SetRenderLogicalPresentation(renderer_, GB_SCREEN_W, GB_SCREEN_H, SDL_LOGICAL_PRESENTATION_INTEGER_SCALE);

    if (const SDL_DisplayMode *mode = SDL_GetCurrentDisplayMode(SDL_GetDisplayForWindow(window_)); mode && mode->refresh_rate > 0.0f) {
        unthrottledPresentInterval_ = std::chrono::nanoseconds(static_cast<int64_t>(1e9 / mode->refresh_rate));
    }

    texture_ = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING, GB_SCREEN_W, GB_SCREEN_H);
    if (!texture_) {
        SDL_Log("CreateTexture: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    SDL_SetTextureScaleMode(texture_, useNearest_ ? SDL_SCALEMODE_NEAREST : SDL_SCALEMODE_LINEAR);

    if (audioEnabled_) {
        SDL_AudioSpec audioSpec{};
        audioSpec.freq = AUDIO_SAMPLE_RATE;
        audioSpec.format = SDL_AUDIO_F32;
        audioSpec.channels = 2;

        audioStream_ = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &audioSpec, nullptr, nullptr);
        if (!audioStream_) {
            SDL_Log("Failed to create audio stream: %s", SDL_GetError());
            audioEnabled_ = false;
        } else {
            audioBuffer_.resize(AUDIO_BUFFER_SIZE * 2);
            SDL_ResumeAudioStreamDevice(audioStream_);
        }
    }

    gameboy_ = Gameboy::init({
        .romName = args->rom_path,
        .biosPath = args->bios_path,
        .model = args->model,
        .noBootrom = args->no_bootrom,
        .realRTC = args->real_rtc,
        .noAudio = !audioEnabled_
    });

    lastTitleTime_ = std::chrono::steady_clock::now();
    session_ = std::make_unique<EmulationSession>(*gameboy_);
#ifdef STARGBC_ENABLE_LUA
    lua_ = std::make_unique<LuaRuntime>(*session_,
                                        LuaLimits{args->script_instruction_budget, static_cast<size_t>(args->script_memory_mib) * 1024 * 1024});
    if (!scriptPath_.empty())
        SDL_Log("Lua script selected: %s. Press Shift+F5 to start/stop; F5 reloads while running.", scriptPath_.c_str());
#endif

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDLFrontend::HandleEvent(const SDL_Event &event) {
    switch (event.type) {
        case SDL_EVENT_QUIT:
            return SDL_APP_SUCCESS;

        case SDL_EVENT_WINDOW_FOCUS_LOST:
            session_->FocusLost();
            break;

        case SDL_EVENT_WINDOW_EXPOSED:
            SDL_RenderTexture(renderer_, texture_, nullptr, nullptr);
            SDL_RenderPresent(renderer_);
            break;

        case SDL_EVENT_KEY_DOWN:
            return HandleKeyDown(event.key);

        case SDL_EVENT_KEY_UP:
            HandleKeyUp(event.key);
            break;

        default:
            break;
    }
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDLFrontend::HandleKeyDown(const SDL_KeyboardEvent &key) {
    switch (key.key) {
        case SDLK_ESCAPE:
            return SDL_APP_SUCCESS;
        case SDLK_Z:
            session_->SetPhysicalButton(Keys::A, true);
            break;
        case SDLK_X:
            session_->SetPhysicalButton(Keys::B, true);
            break;
        case SDLK_RETURN:
            session_->SetPhysicalButton(Keys::Start, true);
            break;
        case SDLK_BACKSPACE:
            session_->SetPhysicalButton(Keys::Select, true);
            break;
        case SDLK_RIGHT:
            session_->SetPhysicalButton(Keys::Right, true);
            break;
        case SDLK_LEFT:
            session_->SetPhysicalButton(Keys::Left, true);
            break;
        case SDLK_UP:
            session_->SetPhysicalButton(Keys::Up, true);
            break;
        case SDLK_DOWN:
            session_->SetPhysicalButton(Keys::Down, true);
            break;
        case SDLK_SPACE:
            throttled_ = false;
            break;
        case SDLK_M:
            speedMultiplier_ = speedMultiplier_ == 1 ? 4 : 1;
            break;
        case SDLK_P:
            paused_ = true;
            break;
        case SDLK_R:
            paused_ = false;
            session_->SetPaused(false);
            break;
        case SDLK_F5:
#ifdef STARGBC_ENABLE_LUA
            if (!key.repeat && lua_) {
                const bool toggle = key.mod & SDL_KMOD_SHIFT;
                if (toggle && lua_->Active()) {
                    lua_->Stop();
                    SDL_Log("Lua script stopped. Press Shift+F5 to start again.");
                } else if (!scriptPath_.empty() && (toggle || lua_->Active())) {
                    if (lua_->LoadFile(scriptPath_))
                        SDL_Log("Lua script started: %s. Press Shift+F5 to stop.", scriptPath_.c_str());
                } else if (!scriptPath_.empty()) {
                    SDL_Log("Lua script is stopped. Press Shift+F5 to start.");
                } else {
                    SDL_Log("No Lua script selected. Use --script path/to/script.lua.");
                }
            }
#endif
            break;
        case SDLK_F2:
            SaveScreenshot();
            break;
        case SDLK_N:
            if (audioStream_) {
                audioEnabled_ = !audioEnabled_;
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
            if (key.mod & SDL_KMOD_LSHIFT)
                SaveState(slot);
            else if (key.mod & SDL_KMOD_LCTRL)
                LoadState(slot);
            break;
        }
        default:
            break;
    }
    return SDL_APP_CONTINUE;
}

void SDLFrontend::HandleKeyUp(const SDL_KeyboardEvent &key) {
    switch (key.key) {
        case SDLK_Z:
            session_->SetPhysicalButton(Keys::A, false);
            break;
        case SDLK_X:
            session_->SetPhysicalButton(Keys::B, false);
            break;
        case SDLK_RETURN:
            session_->SetPhysicalButton(Keys::Start, false);
            break;
        case SDLK_BACKSPACE:
            session_->SetPhysicalButton(Keys::Select, false);
            break;
        case SDLK_RIGHT:
            session_->SetPhysicalButton(Keys::Right, false);
            break;
        case SDLK_LEFT:
            session_->SetPhysicalButton(Keys::Left, false);
            break;
        case SDLK_UP:
            session_->SetPhysicalButton(Keys::Up, false);
            break;
        case SDLK_DOWN:
            session_->SetPhysicalButton(Keys::Down, false);
            break;
        case SDLK_SPACE:
            throttled_ = true;
            break;
        default:
            break;
    }
}

SDL_AppResult SDLFrontend::Iterate() {
    if (!paused_ && session_->RunFrame()) {
        ++framesSinceTitle_;
        ThrottleFrame();
    }
    if (audioStateGeneration_ != session_->StateGeneration()) {
        audioStateGeneration_ = session_->StateGeneration();
        if (audioStream_) SDL_ClearAudioStream(audioStream_);
    }

    if (gameboy_->ConsumeFrame()) {
        if (throttled_) {
            PresentFrame();
        } else if (const auto now = std::chrono::steady_clock::now(); now - lastPresentTime_ >= unthrottledPresentInterval_) {
            lastPresentTime_ = now;
            PresentFrame();
        }
    }

    PumpAudio();
    UpdateWindowTitle();

    return SDL_APP_CONTINUE;
}

void SDLFrontend::ThrottleFrame() {
    using clock = std::chrono::steady_clock;
    if (throttled_) {
        nextFrameTime_ += Gameboy::FRAME_PERIOD / speedMultiplier_;
        if (const auto now = clock::now(); nextFrameTime_ > now) {
            SDL_DelayPrecise(std::chrono::duration_cast<std::chrono::nanoseconds>(nextFrameTime_ - now).count());
        } else {
            nextFrameTime_ = now; // fell behind; don't try to catch up in a burst
        }
    } else {
        nextFrameTime_ = clock::now();
    }
}

void SDLFrontend::UpdateWindowTitle() {
    const auto now = std::chrono::steady_clock::now();
    const auto elapsed = std::chrono::duration<double>(now - lastTitleTime_).count();
    if (elapsed < 1.0)
        return;
    const double fps = framesSinceTitle_ / elapsed;
    framesSinceTitle_ = 0;
    lastTitleTime_ = now;

    std::string suffix;
    if (paused_ || session_->Paused())
        suffix = " (paused)";
    else if (!throttled_)
        suffix = " (unthrottled)";
    else if (speedMultiplier_ != 1)
        suffix = std::format(" ({}x)", speedMultiplier_);
    const std::string title = std::format("{} — {:.1f} fps{}", kAppName, fps, suffix);
    SDL_SetWindowTitle(window_, title.c_str());
}

void SDLFrontend::PresentFrame() const {
    SDL_UpdateTexture(texture_, nullptr, gameboy_->GetScreenData(), GB_SCREEN_W * sizeof(uint32_t));

    SDL_RenderTexture(renderer_, texture_, nullptr, nullptr);
    SDL_RenderPresent(renderer_);
}

void SDLFrontend::PumpAudio() {
    if (!audioEnabled_ || !audioStream_)
        return;

    // Drain the emulator's ring buffer completely every frame: a throttled frame yields
    // ~804 sample frames, and anything left behind overflows the ring and gets dropped
    if (const size_t samplesRead = gameboy_->ReadAudioSamples(audioBuffer_.data(), AUDIO_BUFFER_SIZE);
        samplesRead > 0 && SDL_GetAudioStreamQueued(audioStream_) <= MAX_AUDIO_QUEUE_BYTES) {
        // When the queue is backed up (unthrottled/4x speed), samples are consumed but dropped
        SDL_PutAudioStreamData(audioStream_, audioBuffer_.data(), static_cast<int>(samplesRead * 2 * sizeof(float)));
    }
    if (throttled_) {
        const auto queuedFrames = static_cast<int>(SDL_GetAudioStreamQueued(audioStream_) / (2 * sizeof(float)));
        const double error = static_cast<double>(queuedFrames - AUDIO_TARGET_QUEUE_FRAMES) / AUDIO_TARGET_QUEUE_FRAMES;
        const auto ratio = static_cast<float>(std::clamp(1.0 + 0.005 * error, 0.995, 1.005));
        SDL_SetAudioStreamFrequencyRatio(audioStream_, ratio);
    } else {
        SDL_SetAudioStreamFrequencyRatio(audioStream_, 1.0f);
    }
}

void SDLFrontend::SaveScreenshot() const {
    try {
        std::ofstream file(romPath_ + ".screen", std::ios::binary | std::ios::trunc);
        if (!file.is_open())
            throw std::runtime_error("Could not open " + romPath_ + ".screen");
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
        if (!file.is_open())
            throw std::runtime_error("Failed to save: " + filename);
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
        if (!session_->LoadState(state)) {
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
