#define SDL_MAIN_USE_CALLBACKS 1

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <SDLFrontend.h>

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[]) {
    auto *frontend = new SDLFrontend();
    *appstate = frontend;
    return frontend->Init(argc, argv);
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event) {
    return static_cast<SDLFrontend *>(appstate)->HandleEvent(*event);
}

SDL_AppResult SDL_AppIterate(void *appstate) {
    return static_cast<SDLFrontend *>(appstate)->Iterate();
}

void SDL_AppQuit(void *appstate, SDL_AppResult) {
    delete static_cast<SDLFrontend *>(appstate);
}
