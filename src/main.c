#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h> 
#include <stdint.h>

#include <SDL3/SDL.h>

typedef struct sdl_type {
    SDL_Window *window;
    SDL_Renderer *renderer; 
} sdl_t;

typedef struct config_type {
    uint32_t window_width;
    uint32_t window_height;
    uint32_t fg_colour; // Foreground colour 
    uint32_t bg_colour; // Background colour
    uint32_t scale_factor; // Amount to scale CHIP-8 pixel by (Original resolution is too small for modern displays)
} config_t;

bool set_config(config_t *config, int argc, char * argv[]) {
    // Set defaults
    config->window_width = 64;
    config->window_height = 32;
    config->fg_colour = 0xFFFFFFFF; // WHITE 
    config->bg_colour = 0xFF0000FF; // RED 
    config->scale_factor = 20;

    // Supress compiler warnings 
    // TODO: Implement command line arguments
    for (int i = 1; i < argc; i++) {
        (void)argv[i]; 
    }

    return true;
}

bool initialise_sdl(sdl_t *sdl, config_t config) {
    if (!SDL_SetAppMetadata("CHIP-8 Interpreter", NULL, NULL)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failure setting SDL metadata %s\n", SDL_GetError());
        return false;
    }

    if (!SDL_InitSubSystem(SDL_INIT_AUDIO | SDL_INIT_VIDEO)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failure initialising SDL subsytems %s\n", SDL_GetError());
        return false;
    } 

    SDL_CreateWindowAndRenderer("CHIP-8 Interpreter", config.window_width * config.scale_factor, config.window_height * config.scale_factor, 0, &(sdl->window), &(sdl->renderer));
    if (!sdl->window || !sdl->renderer) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failure creating SDL window and renderer %s\n", SDL_GetError());
        return false;
    }

    return true;
}

void exit_cleanup(sdl_t *sdl) {
    SDL_DestroyRenderer(sdl->renderer);
    SDL_DestroyWindow(sdl->window);
    SDL_Quit();
}

void clear_screen(const config_t config, const sdl_t sdl) {
    const uint8_t r = (config.bg_colour >> 24) & 0xFF;
    const uint8_t g = (config.bg_colour >> 16) & 0xFF;
    const uint8_t b = (config.bg_colour >> 8) & 0xFF;
    const uint8_t a = (config.bg_colour >> 0) & 0xFF;

    SDL_SetRenderDrawColor(sdl.renderer, r, g, b, a);
    SDL_RenderClear(sdl.renderer);
}

void update_screen(const sdl_t sdl) {
    SDL_RenderPresent(sdl.renderer);
}

int main(int argc, char * argv[]) {
    (void)argc;
    (void)argv;

    sdl_t sdl = {0};
    config_t config = {0};

    // Ensure that config is correctly set
    if (!set_config(&config, argc, argv)) {
        exit(EXIT_FAILURE);
    }

    // Ensure SDL initialises succesfully 
    if (!initialise_sdl(&sdl, config)) {
        exit(EXIT_FAILURE);
    }

    SDL_Event event = {0};
    // Main loop
    while (true) {
        SDL_PollEvent(&event);
        if (event.type == SDL_EVENT_QUIT) {
            break;
        }

        // Delay for 60hz (approx)
        SDL_Delay(16);

        clear_screen(config, sdl);
        update_screen(sdl);
    }

    exit_cleanup(&sdl);

    return EXIT_SUCCESS;
}
