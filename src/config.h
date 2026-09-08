#ifndef CONFIG_H
#define CONFIG_H

#include <stdbool.h>
#include <stdint.h>

typedef enum extension_type { CHIP8, SUPERCHIP, XOCHIP } extension_t;

typedef struct {
    uint32_t window_width;
    uint32_t window_height;
    uint32_t fg_colour;         // Foreground colour
    uint32_t bg_colour;         // Background colour
    uint32_t scale_factor;      // Amount to scale CHIP-8 pixel by (Original
                                // resolution is too small for modern displays)
    uint32_t clock_rate;        // CPU hz
    uint32_t square_wave_freq;  // Frequency of the square wave
    int16_t volume;
    extension_t extension;
} config_t;

bool set_config(config_t* config, int argc, char* argv[]);

#endif
