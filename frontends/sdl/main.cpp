#define SDL_MAIN_USE_CALLBACKS 1

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <exception>

#include "SDLFrontend.h"

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[]) {
    try {
        auto *frontend = new SDLFrontend();
        *appstate = frontend;
        return frontend->Init(argc, argv);
    } catch (const std::exception &error) {
        SDL_Log("Initialization failed: %s", error.what());
        return SDL_APP_FAILURE;
    }
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event) {
    try {
        return static_cast<SDLFrontend *>(appstate)->HandleEvent(*event);
    } catch (const std::exception &error) {
        SDL_Log("Event handling failed: %s", error.what());
        return SDL_APP_FAILURE;
    }
}

SDL_AppResult SDL_AppIterate(void *appstate) {
    try {
        return static_cast<SDLFrontend *>(appstate)->Iterate();
    } catch (const std::exception &error) {
        SDL_Log("Emulation failed: %s", error.what());
        return SDL_APP_FAILURE;
    }
}

void SDL_AppQuit(void *appstate, SDL_AppResult) { delete static_cast<SDLFrontend *>(appstate); }
