#include "config.h"

#include <stdbool.h>
#include <stdint.h>

bool set_config(config_t* config, int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    *config = (config_t){
        .window_width = 64,       // Original CHIP-8 resolution
        .window_height = 32,      // Original CHIP-8 resolution
        .fg_colour = 0xFFFFFFFF,  // RGBA8888
        .bg_colour = 0x000000FF,  // RGBA8888
        .scale_factor = 20,
        .clock_rate = 700,  // Number of instructions to emulate per second
        .square_wave_freq = 440,  // 440hz for middle A
        .volume = 3000,
    };

    // TODO: Implement command line arguments

    return true;
}
