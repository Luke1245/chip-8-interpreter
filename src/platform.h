#ifndef PLATFORM_H
#define PLATFORM_H

#include <SDL3/SDL.h>
#include <stdbool.h>

#include "config.h"
#include "chip8.h"

typedef struct sdl {
    SDL_Window* window;
    SDL_Renderer* renderer;
    SDL_AudioSpec audio_spec;
    SDL_AudioStream* audio_stream;
    uint64_t sample_index;  // current square wave phase, needed to persist
                            // between frames
} sdl_t;

bool initialise_sdl(sdl_t* sdl, config_t config);
void exit_cleanup(sdl_t* sdl);
void clear_screen(const config_t* config, const sdl_t* sdl);
void update_screen(const sdl_t* sdl, const config_t* config,
                   const chip8_t* chip8);
void handle_input(chip8_t* chip8);
void play_audio(sdl_t* sdl, const config_t* config, bool playing);

#endif
