#define SDL_MAIN_USE_CALLBACKS 1

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <print>
#include <vector>
#include <Gameboy.h>

constexpr int SCREEN_WIDTH = 160;
constexpr int SCREEN_HEIGHT = 144;

static SDL_Window *window = nullptr;
static SDL_Renderer *renderer = nullptr;
static SDL_Texture *texture = nullptr;
static SDL_Texture *converted_texture = nullptr;
static int converted_texture_width = 0;
static int converted_texture_height = 0;

static std::unique_ptr<Gameboy> gameboy = nullptr;

SDL_AppResult SDL_AppInit(void **appstate, const int argc, char *argv[]) {
    SDL_SetAppMetadata("StarGBC", "0.0.1", "com.srikur.stargbc");

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("Couldn't initialize SDL: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    if (!SDL_CreateWindowAndRenderer("StarGBC", SCREEN_WIDTH, SCREEN_HEIGHT, 0, &window, &renderer)) {
        SDL_Log("Couldn't create window/renderer: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    texture = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_RGBA8888,
        SDL_TEXTUREACCESS_STREAMING,
        SCREEN_WIDTH,
        SCREEN_HEIGHT
    );
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
    for (size_t i = 0; i < args.size(); ++i) {
        if (args[i] == "--gbc") {
            settings.mode = Mode::GBC;
        } else if (args[i] == "--gb") {
            settings.mode = Mode::GB;
        } else if (args[i] == "--bios") {
            if (i + 1 < args.size()) {
                settings.biosPath = args[++i];
            } else {
                std::fprintf(stderr, "Error: --bios requires a path argument\n");
                return SDL_APP_FAILURE;
            }
        } else if (i == args.size() - 1 || args[i].ends_with(".gb") || args[i].ends_with(".gbc")) {
            settings.romName = args[i];
        } else {
            std::fprintf(stderr, "Unknown argument: %s\n", std::string(args[i]).c_str());
            return SDL_APP_FAILURE;
        }
    }

    gameboy = Gameboy::init(settings);

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event) {
    switch (event->type) {
        case SDL_EVENT_QUIT:
            return SDL_APP_SUCCESS;
        case SDL_EVENT_KEY_DOWN:
            if (event->key.key == SDLK_ESCAPE) { return SDL_APP_SUCCESS; }
            if (event->key.key == SDLK_Z) { gameboy->KeyDown(Keys::A); } else if (
                event->key.key == SDLK_X) { gameboy->KeyDown(Keys::B); } else if (
                event->key.key == SDLK_RETURN) { gameboy->KeyDown(Keys::Start); } else if (
                event->key.key == SDLK_BACKSPACE) { gameboy->KeyDown(Keys::Select); } else if (
                event->key.key == SDLK_RIGHT) { gameboy->KeyDown(Keys::Right); } else if (
                event->key.key == SDLK_LEFT) { gameboy->KeyDown(Keys::Left); } else if (event->key.key == SDLK_UP) {
                gameboy->KeyDown(Keys::Up);
            } else if (event->key.key == SDLK_DOWN) { gameboy->KeyDown(Keys::Down); }
            else if (event->key.key == SDLK_SPACE) { gameboy->SetThrottle(false); }
            else if (event->key.key == SDLK_M) { gameboy->ToggleSpeed(); }
            break;
        case SDL_EVENT_KEY_UP:
            if (event->key.key == SDLK_Z) { gameboy->KeyUp(Keys::A); } else if (
                event->key.key == SDLK_X) { gameboy->KeyUp(Keys::B); } else if (
                event->key.key == SDLK_RETURN) { gameboy->KeyUp(Keys::Start); } else if (
                event->key.key == SDLK_BACKSPACE) { gameboy->KeyUp(Keys::Select); } else if (
                event->key.key == SDLK_RIGHT) { gameboy->KeyUp(Keys::Right); } else if (
                event->key.key == SDLK_LEFT) { gameboy->KeyUp(Keys::Left); } else if (event->key.key == SDLK_UP) {
                gameboy->KeyUp(Keys::Up);
            } else if (event->key.key == SDLK_DOWN) { gameboy->KeyUp(Keys::Down); }
            else if (event->key.key == SDLK_SPACE) { gameboy->SetThrottle(true); }
            break;
        default: break;
    }
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void *appstate) {
    SDL_FRect rect;
    SDL_Surface *surface = nullptr;

    gameboy->UpdateEmulator();

    if (gameboy->CheckVBlank() && SDL_LockTextureToSurface(texture, nullptr, &surface)) {
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
        SDL_RenderClear(renderer);

        surface = SDL_RenderReadPixels(renderer, nullptr);

        if (surface == nullptr) {
            SDL_Log("Couldn't read pixels: %s", SDL_GetError());
            return SDL_APP_FAILURE;
        }

        if ((surface->w != converted_texture_width) || (surface->h != converted_texture_height)) {
            SDL_DestroyTexture(converted_texture);
            converted_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING,
                                                  surface->w, surface->h);
            if (!converted_texture) {
                SDL_Log("Couldn't (re)create conversion texture: %s", SDL_GetError());
                return SDL_APP_FAILURE;
            }
            converted_texture_width = surface->w;
            converted_texture_height = surface->h;
        }

        for (int scanline = 0; scanline < surface->h; scanline++) {
            auto *pixels = reinterpret_cast<uint32_t *>(
                static_cast<uint8_t *>(surface->pixels) + scanline * surface->pitch);
            for (int pixel = 0; pixel < surface->w; pixel++) {
                auto *pixelData = reinterpret_cast<uint8_t *>(&pixels[pixel]);
                auto [r, g, b] = gameboy->GetPixel(scanline, pixel);
                pixelData[0] = r;
                pixelData[1] = g;
                pixelData[2] = b;
                pixelData[3] = 255;
            }
        }

        rect.x = 0;
        rect.y = 0;
        rect.w = static_cast<float>(surface->w);
        rect.h = static_cast<float>(surface->h);

        SDL_UpdateTexture(converted_texture, nullptr, surface->pixels, surface->pitch);
        SDL_DestroySurface(surface);

        SDL_RenderTexture(renderer, converted_texture, nullptr, &rect);
        SDL_RenderPresent(renderer);
    }

    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *appstate, SDL_AppResult result) {
    if (gameboy != nullptr) {
        gameboy->Save();
    }
    if (converted_texture != nullptr) {
        SDL_DestroyTexture(converted_texture);
        converted_texture = nullptr;
    }
    if (texture != nullptr) {
        SDL_DestroyTexture(texture);
        texture = nullptr;
    }
    if (renderer != nullptr) {
        SDL_DestroyRenderer(renderer);
        renderer = nullptr;
    }
    if (window != nullptr) {
        SDL_DestroyWindow(window);
        window = nullptr;
    }
    SDL_Quit();
}
