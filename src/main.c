#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h> 

#include <SDL3/SDL.h>

// Initialise SDL
bool initialise_sdl(void) {
    if (!SDL_SetAppMetadata("CHIP-8 Interpreter", NULL, NULL)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failure setting SDL metadata %s\n", SDL_GetError());
        return false;
    }

    if (!SDL_InitSubSystem(SDL_INIT_AUDIO | SDL_INIT_VIDEO)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failure initialising SDL subsytems %s\n", SDL_GetError());
        return false;
    } 

    return true;
}

void exit_cleanup(void) {
    SDL_Quit();
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    if (!initialise_sdl()) {
        exit(EXIT_FAILURE);
    }

    exit_cleanup();

    return EXIT_SUCCESS;
}
