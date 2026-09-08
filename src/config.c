#include "config.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

bool set_config(config_t* config, int argc, char* argv[], char** rom_name) {
    *config = (config_t){
        .window_width = 64,       // Original CHIP-8 resolution
        .window_height = 32,      // Original CHIP-8 resolution
        .fg_colour = 0xFFFFFFFF,  // RGBA8888
        .bg_colour = 0x000000FF,  // RGBA8888
        .scale_factor = 20,
        .clock_rate = 700,  // Number of instructions to emulate per second
        .square_wave_freq = 440,  // 440hz for middle A
        .volume = 3000,
        .extension = CHIP8,  // Current CHIP-8 extension for opcode quirks
    };

    if (argc < 2 || argv[1][0] == '-') {
        fprintf(stderr,
                "Usage %s <rom_name> [-s scale_factor] [-e extension]\n",
                argv[0]);
        exit(EXIT_FAILURE);
    }

    *rom_name = argv[1];

    int opt;
    while ((opt = getopt(argc - 1, argv + 1, "s:e:")) != -1) {
        switch (opt) {
            case 's':
                // Convert string argument to uint32_t for scale_factor
                config->scale_factor = (uint32_t) strtol(optarg, NULL, 10);
                break;
            case 'e':
                if (strcmp("CHIP8", optarg) == 0) {
                    config->extension = CHIP8;
                } else if (strcmp("SUPERCHIP", optarg) == 0) {
                    config->extension = SUPERCHIP;
                } else if (strcmp("XOCHIP", optarg) == 0) {
                    config->extension = XOCHIP;
                }
                break;
        }
    }

    return true;
}
