#define SDL_MAIN_USE_CALLBACKS 1

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <print>
#include <vector>
#include <memory>
#include <Gameboy.h>

constexpr int GB_SCREEN_W = 160;
constexpr int GB_SCREEN_H = 144;
constexpr int WINDOW_SCALE = 3;
constexpr int WINDOW_W = GB_SCREEN_W * WINDOW_SCALE;
constexpr int WINDOW_H = GB_SCREEN_H * WINDOW_SCALE;

static SDL_Window *window = nullptr;
static SDL_Renderer *renderer = nullptr;
static SDL_Texture *texture = nullptr;
static std::unique_ptr<Gameboy> gameboy = nullptr;

static Uint32 packRGBA(const SDL_PixelFormat *fmt, const Uint8 r, const Uint8 g, const Uint8 b) {
    constexpr Uint8 a = 0xFF;
    return SDL_MapRGBA(SDL_GetPixelFormatDetails(*fmt), nullptr, r, g, b, a);
}

SDL_AppResult SDL_AppInit(void ** /*appstate*/, int argc, char *argv[]) {
    SDL_SetAppMetadata("StarGBC", "0.0.1", "com.srikur.stargbc");

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("Couldn't initialise SDL: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    if (!SDL_CreateWindowAndRenderer("StarGBC",
                                     WINDOW_W,
                                     WINDOW_H,
                                     SDL_WINDOW_RESIZABLE,
                                     &window,
                                     &renderer)) {
        SDL_Log("Couldn't create window/renderer: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    if (!SDL_SetRenderLogicalPresentation(renderer,
                                          GB_SCREEN_W,
                                          GB_SCREEN_H,
                                          SDL_LOGICAL_PRESENTATION_INTEGER_SCALE)) {
        // SDL3
        SDL_Log("Couldn't enable logical presentation: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    texture = SDL_CreateTexture(renderer,
                                SDL_PIXELFORMAT_RGBA32,
                                SDL_TEXTUREACCESS_STREAMING,
                                GB_SCREEN_W,
                                GB_SCREEN_H);
    if (!texture) {
        SDL_Log("Couldn't create streaming texture: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    if (argc < 2) {
        std::printf("USAGE: StarGBC [options] romName\n");
        std::printf("Options:\n\t--gbc\t\tRun in GBC mode\n\t--gb\t\tRun in GB mode\n");
        std::printf("\t--bios\t\tPath to BIOS ROM\n");
        std::printf("If a mode is not supplied, the cartridge mode will be used instead");
        return SDL_APP_FAILURE;
    }

    const std::vector<std::string_view> args(argv + 1, argv + argc);
    GameboySettings settings{};
    for (std::size_t i = 0; i < args.size(); ++i) {
        if (args[i] == "--gbc") {
            settings.mode = Mode::GBC;
        } else if (args[i] == "--gb") {
            settings.mode = Mode::GB;
        } else if (args[i] == "--debugStart") {
            settings.debugStart = true;
        } else if (args[i] == "--bios") {
            if (i + 1 < args.size()) {
                settings.biosPath = args[++i];
            } else {
                std::fprintf(stderr, "Error: --bios requires a path argument\n");
                return SDL_APP_FAILURE;
            }
        } else if (i == args.size() - 1 ||
                   args[i].ends_with(".gb") || args[i].ends_with(".gbc")) {
            settings.romName = args[i];
        } else {
            std::fprintf(stderr, "Unknown argument: %s\n", std::string(args[i]).c_str());
            return SDL_APP_FAILURE;
        }
    }

    gameboy = Gameboy::init(settings);
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void *, SDL_Event *event) {
    switch (event->type) {
        case SDL_EVENT_QUIT:
            return SDL_APP_SUCCESS;

        case SDL_EVENT_KEY_DOWN: {
            switch (event->key.key) {
                case SDLK_ESCAPE: return SDL_APP_SUCCESS;
                case SDLK_Z: gameboy->KeyDown(Keys::A);
                    break;
                case SDLK_X: gameboy->KeyDown(Keys::B);
                    break;
                case SDLK_RETURN: gameboy->KeyDown(Keys::Start);
                    break;
                case SDLK_BACKSPACE: gameboy->KeyDown(Keys::Select);
                    break;
                case SDLK_RIGHT: gameboy->KeyDown(Keys::Right);
                    break;
                case SDLK_LEFT: gameboy->KeyDown(Keys::Left);
                    break;
                case SDLK_UP: gameboy->KeyDown(Keys::Up);
                    break;
                case SDLK_DOWN: gameboy->KeyDown(Keys::Down);
                    break;
                case SDLK_SPACE: gameboy->SetThrottle(false);
                    break;
                case SDLK_M: gameboy->ToggleSpeed();
                    break;
                case SDLK_P: gameboy->SetPaused(true);
                    break;
                case SDLK_R: gameboy->SetPaused(false);
                    break;
                case SDLK_F8: {
                    if (gameboy->IsPaused()) {
                        gameboy->DebugNextInstruction();
                    }
                }
                // Save states with Shift + 1-9, load with Ctrl + 1-9
                case SDLK_1:
                    if (event->key.mod & SDL_KMOD_LSHIFT) gameboy->SaveState(1);
                    else if (event->key.mod & SDL_KMOD_LCTRL) gameboy->LoadState(1);
                    break;
                case SDLK_2:
                    if (event->key.mod & SDL_KMOD_LSHIFT) gameboy->SaveState(2);
                    else if (event->key.mod & SDL_KMOD_LCTRL) gameboy->LoadState(2);
                    break;
                case SDLK_3:
                    if (event->key.mod & SDL_KMOD_LSHIFT) gameboy->SaveState(3);
                    else if (event->key.mod & SDL_KMOD_LCTRL) gameboy->LoadState(3);
                    break;
                case SDLK_4:
                    if (event->key.mod & SDL_KMOD_LSHIFT) gameboy->SaveState(4);
                    else if (event->key.mod & SDL_KMOD_LCTRL) gameboy->LoadState(4);
                    break;
                case SDLK_5:
                    if (event->key.mod & SDL_KMOD_LSHIFT) gameboy->SaveState(5);
                    else if (event->key.mod & SDL_KMOD_LCTRL) gameboy->LoadState(5);
                    break;
                case SDLK_6:
                    if (event->key.mod & SDL_KMOD_LSHIFT) gameboy->SaveState(6);
                    else if (event->key.mod & SDL_KMOD_LCTRL) gameboy->LoadState(6);
                    break;
                case SDLK_7:
                    if (event->key.mod & SDL_KMOD_LSHIFT) gameboy->SaveState(7);
                    else if (event->key.mod & SDL_KMOD_LCTRL) gameboy->LoadState(7);
                    break;
                default: break;
            }
            break;
        }

        case SDL_EVENT_KEY_UP: {
            switch (event->key.key) {
                case SDLK_Z: gameboy->KeyUp(Keys::A);
                    break;
                case SDLK_X: gameboy->KeyUp(Keys::B);
                    break;
                case SDLK_RETURN: gameboy->KeyUp(Keys::Start);
                    break;
                case SDLK_BACKSPACE: gameboy->KeyUp(Keys::Select);
                    break;
                case SDLK_RIGHT: gameboy->KeyUp(Keys::Right);
                    break;
                case SDLK_LEFT: gameboy->KeyUp(Keys::Left);
                    break;
                case SDLK_UP: gameboy->KeyUp(Keys::Up);
                    break;
                case SDLK_DOWN: gameboy->KeyUp(Keys::Down);
                    break;
                case SDLK_SPACE: gameboy->SetThrottle(true);
                    break;
                default: break;
            }
            break;
        }

        default: break;
    }
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void *) {
    gameboy->UpdateEmulator();

    if (gameboy->CheckVBlank()) {
        SDL_Surface *surface = nullptr;
        if (!SDL_LockTextureToSurface(texture, nullptr, &surface)) {
            SDL_Log("Couldn't lock texture: %s", SDL_GetError());
            return SDL_APP_FAILURE;
        }

        for (int y = 0; y < GB_SCREEN_H; ++y) {
            auto *row = reinterpret_cast<Uint32 *>(
                static_cast<Uint8 *>(surface->pixels) + y * surface->pitch);

            for (int x = 0; x < GB_SCREEN_W; ++x) {
                auto [r, g, b] = gameboy->GetPixel(y, x);
                row[x] = packRGBA(&surface->format, r, g, b);
            }
        }
        SDL_UnlockTexture(texture);

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
        SDL_RenderClear(renderer);

        SDL_RenderTexture(renderer, texture, nullptr, nullptr);
        SDL_RenderPresent(renderer);
    }
    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *, SDL_AppResult) {
    // if (gameboy) { gameboy->Save(); }

    if (texture) {
        SDL_DestroyTexture(texture);
        texture = nullptr;
    }
    if (renderer) {
        SDL_DestroyRenderer(renderer);
        renderer = nullptr;
    }
    if (window) {
        SDL_DestroyWindow(window);
        window = nullptr;
    }
    SDL_Quit();
}
