#include <SDL3/SDL.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "chip8.h"
#include "config.h"
#include "platform.h"

int main(int argc, char* argv[]) {
    sdl_t sdl = {0};
    config_t config = {0};
    chip8_t chip8 = {0};

    // TODO: Move CLI args handling to config.c
    if (argc < 2) {
        fprintf(stderr, "Usage %s <rom_name>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // Ensure that config is correctly set
    if (!set_config(&config, argc, argv)) {
        exit(EXIT_FAILURE);
    }

    // Ensure SDL initialises succesfully
    if (!initialise_sdl(&sdl, config)) {
        exit(EXIT_FAILURE);
    }

    clear_screen(&config, &sdl);

    // Seed random number generator
    srand(time(NULL));

    const char* rom_name = argv[1];
    if (!initialise_chip8(&chip8, rom_name)) {
        exit(EXIT_FAILURE);
    }

    // Main loop
    while (chip8.state != QUIT) {
        // Allow user to quit window
        // Delay for 60hz (approx)
        handle_input(&chip8, &config);

        if (chip8.state == PAUSED) continue;

        // Fetch time before executing instructions
        const uint64_t start_time = SDL_GetPerformanceCounter();

        // Emulate instructions for given frame (60hz)
        for (uint32_t i = 0; i < config.clock_rate / 60; i++) {
            emulate_instruction(&chip8, &config);
        }

        // Fetch time after executing instructions
        const uint64_t end_time = SDL_GetPerformanceCounter();

        const double elapsed_time = (double)((end_time - start_time) * 1000) /
                                    SDL_GetPerformanceFrequency();

        // Delay for approx 60hz, or actual time elapsed
        SDL_Delay(16.67f > elapsed_time ? 16.67f - elapsed_time : 0);

        update_screen(&sdl, &config, &chip8);
        const bool playing = update_timers(&chip8);
        play_audio(&sdl, &config, playing);
    }

    exit_cleanup(&sdl);

    return EXIT_SUCCESS;
}
