// ReSharper disable CppParameterMayBeConstPtrOrRef
#define SDL_MAIN_USE_CALLBACKS 1

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_render.h>
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
static bool useNearest = true;

SDL_AppResult SDL_AppInit(void ** /*appstate*/, int argc, char *argv[]) {
    SDL_SetAppMetadata("StarGBC", "0.0.1", "com.srikur.stargbc");

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("Couldn't initialise SDL: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    const std::vector<std::string_view> args(argv + 1, argv + argc);
    GameboySettings settings{};
    for (std::size_t i = 0; i < args.size(); ++i) {
        if (args[i] == "--anti-aliasing") {
            useNearest = false;
        } else if (args[i] == "--gbc") {
            settings.mode = Mode::CGB_GBC;
        } else if (args[i] == "--gb") {
            settings.mode = Mode::DMG;
        } else if (args[i] == "--debugStart") {
            settings.debugStart = true;
        } else if (args[i] == "--realRTC") {
            settings.realRTC = true;
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
            std::fprintf(stderr, "USAGE: StarGBC [options] romFile\n"
                         "Options:\n"
                         "  --gbc | --gb        force gbc/dmg mode\n"
                         "  --bios <path>       external BIOS ROM\n"
                         "  --no-aliasing       nearest-neighbour pixels");
            return SDL_APP_FAILURE;
        }
    }
    if (settings.romName.empty() || (!settings.romName.ends_with(".gb") &&
                                     !settings.romName.ends_with(".gbc"))) {
        std::fprintf(stderr, "Error: no ROM specified");
        return SDL_APP_FAILURE;
    }

    constexpr int winW = GB_SCREEN_W * WINDOW_SCALE;
    constexpr int winH = GB_SCREEN_H * WINDOW_SCALE;

    if (!SDL_CreateWindowAndRenderer("StarGBC", winW, winH,
                                     SDL_WINDOW_RESIZABLE,
                                     &window, &renderer)) {
        SDL_Log("CreateWindowAndRenderer: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    SDL_SetRenderLogicalPresentation(renderer,
                                     GB_SCREEN_W, GB_SCREEN_H,
                                     SDL_LOGICAL_PRESENTATION_INTEGER_SCALE);

    texture = SDL_CreateTexture(renderer,
                                SDL_PIXELFORMAT_RGBA32,
                                SDL_TEXTUREACCESS_STREAMING,
                                GB_SCREEN_W, GB_SCREEN_H);
    if (!texture) {
        SDL_Log("CreateTexture: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    SDL_SetTextureScaleMode(texture,
                            useNearest ? SDL_SCALEMODE_NEAREST : SDL_SCALEMODE_LINEAR);
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

    if (gameboy->ShouldRender()) {
        SDL_UpdateTexture(texture,
                          nullptr,
                          gameboy->GetScreenData(),
                          GB_SCREEN_W * sizeof(uint32_t));

        SDL_RenderTexture(renderer, texture, nullptr, nullptr);
        SDL_RenderPresent(renderer);
    }
    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *, SDL_AppResult) {
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
