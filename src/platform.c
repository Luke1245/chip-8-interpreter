#include "platform.h"

#include <SDL3/SDL.h>

#include "chip8.h"

bool initialise_sdl(sdl_t* sdl, config_t config) {
    if (!SDL_SetAppMetadata("CHIP-8 Interpreter", NULL, NULL)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Failure setting SDL metadata %s\n", SDL_GetError());
        return false;
    }

    if (!SDL_InitSubSystem(SDL_INIT_AUDIO | SDL_INIT_VIDEO)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Failure initialising SDL subsytems %s\n", SDL_GetError());
        return false;
    }

    SDL_CreateWindowAndRenderer("CHIP-8 Interpreter",
                                config.window_width * config.scale_factor,
                                config.window_height * config.scale_factor, 0,
                                &(sdl->window), &(sdl->renderer));
    if (!sdl->window || !sdl->renderer) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Failure creating SDL window and renderer %s\n",
                     SDL_GetError());
        return false;
    }

    // Pick system default playback device for stream
    // SDL3 uses logical devices, so this will adapt to new devices
    sdl->audio_spec = (SDL_AudioSpec){
        .format = SDL_AUDIO_S16,  // Signed 16 bit
        .channels = 1,
        .freq = 44100,  // 44100 Hz
    };
    // start of wave
    sdl->sample_index = 0;

    sdl->audio_stream = SDL_OpenAudioDeviceStream(
        SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &sdl->audio_spec, NULL, NULL);
    if (!sdl->audio_stream) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Failure to open audio device stream %s\n",
                     SDL_GetError());
        return false;
    }

    return true;
}

void exit_cleanup(sdl_t* sdl) {
    // Renderer must be destroyed before window
    SDL_DestroyRenderer(sdl->renderer);
    SDL_DestroyWindow(sdl->window);
    SDL_DestroyAudioStream(sdl->audio_stream);
    SDL_Quit();
}

void clear_screen(const config_t* config, const sdl_t* sdl) {
    // Bit shifts and masks to extract relevant channels from 32-bit config
    // integer
    const uint8_t r = (config->bg_colour >> 24) & 0xFF;
    const uint8_t g = (config->bg_colour >> 16) & 0xFF;
    const uint8_t b = (config->bg_colour >> 8) & 0xFF;
    // Shift not needed but keeps consistent with style
    const uint8_t a = (config->bg_colour >> 0) & 0xFF;

    SDL_SetRenderDrawColor(sdl->renderer, r, g, b, a);
    SDL_RenderClear(sdl->renderer);
}

void update_screen(const sdl_t* sdl, const config_t* config,
                   const chip8_t* chip8) {
    SDL_FRect rect = {
        .x = 0, .y = 0, .w = config->scale_factor, .h = config->scale_factor};

    const uint8_t fg_r = (config->fg_colour >> 24) & 0xFF;
    const uint8_t fg_g = (config->fg_colour >> 16) & 0xFF;
    const uint8_t fg_b = (config->fg_colour >> 8) & 0xFF;
    const uint8_t fg_a = (config->fg_colour >> 0) & 0xFF;

    const uint8_t bg_r = (config->bg_colour >> 24) & 0xFF;
    const uint8_t bg_g = (config->bg_colour >> 16) & 0xFF;
    const uint8_t bg_b = (config->bg_colour >> 8) & 0xFF;
    const uint8_t bg_a = (config->bg_colour >> 0) & 0xFF;

    for (uint32_t i = 0; i < sizeof(chip8->display); i++) {
        rect.x = (i % config->window_width) * config->scale_factor;
        rect.y = (i / config->window_width) * config->scale_factor;

        if (chip8->display[i]) {
            // Pixel is on: draw foreground colour
            SDL_SetRenderDrawColor(sdl->renderer, fg_r, fg_g, fg_b, fg_a);
            SDL_RenderFillRect(sdl->renderer, &rect);
        } else {
            // Pixel is off: draw background colour
            SDL_SetRenderDrawColor(sdl->renderer, bg_r, bg_g, bg_b, bg_a);
            SDL_RenderFillRect(sdl->renderer, &rect);
        }
    }

    SDL_RenderPresent(sdl->renderer);
}

void handle_input(chip8_t* chip8) {
    SDL_Event event;

    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_EVENT_QUIT:
                chip8->state = QUIT;
                return;

            case SDL_EVENT_KEY_DOWN:
                // Use scancodes for portability between non QWERTY keyboard
                // layouts
                switch (event.key.scancode) {
                    case SDL_SCANCODE_ESCAPE:
                        chip8->state = QUIT;
                        return;

                    case SDL_SCANCODE_SPACE:
                        if (chip8->state == RUNNING) {
                            chip8->state = PAUSED;
                        } else {
                            chip8->state = RUNNING;  // Resume
                        }
                        return;

                    case SDL_SCANCODE_1:
                        chip8->keyboard[0x1] = true;
                        break;
                    case SDL_SCANCODE_2:
                        chip8->keyboard[0x2] = true;
                        break;
                    case SDL_SCANCODE_3:
                        chip8->keyboard[0x3] = true;
                        break;
                    case SDL_SCANCODE_4:
                        chip8->keyboard[0xC] = true;
                        break;

                    case SDL_SCANCODE_Q:
                        chip8->keyboard[0x4] = true;
                        break;
                    case SDL_SCANCODE_W:
                        chip8->keyboard[0x5] = true;
                        break;
                    case SDL_SCANCODE_E:
                        chip8->keyboard[0x6] = true;
                        break;
                    case SDL_SCANCODE_R:
                        chip8->keyboard[0xD] = true;
                        break;

                    case SDL_SCANCODE_A:
                        chip8->keyboard[0x7] = true;
                        break;
                    case SDL_SCANCODE_S:
                        chip8->keyboard[0x8] = true;
                        break;
                    case SDL_SCANCODE_D:
                        chip8->keyboard[0x9] = true;
                        break;
                    case SDL_SCANCODE_F:
                        chip8->keyboard[0xE] = true;
                        break;

                    case SDL_SCANCODE_Z:
                        chip8->keyboard[0xA] = true;
                        break;
                    case SDL_SCANCODE_X:
                        chip8->keyboard[0x0] = true;
                        break;
                    case SDL_SCANCODE_C:
                        chip8->keyboard[0xB] = true;
                        break;
                    case SDL_SCANCODE_V:
                        chip8->keyboard[0xF] = true;
                        break;

                    default:
                        break;
                }
                break;

            case SDL_EVENT_KEY_UP:
                switch (event.key.scancode) {
                    case SDL_SCANCODE_1:
                        chip8->keyboard[0x1] = false;
                        break;
                    case SDL_SCANCODE_2:
                        chip8->keyboard[0x2] = false;
                        break;
                    case SDL_SCANCODE_3:
                        chip8->keyboard[0x3] = false;
                        break;
                    case SDL_SCANCODE_4:
                        chip8->keyboard[0xC] = false;
                        break;

                    case SDL_SCANCODE_Q:
                        chip8->keyboard[0x4] = false;
                        break;
                    case SDL_SCANCODE_W:
                        chip8->keyboard[0x5] = false;
                        break;
                    case SDL_SCANCODE_E:
                        chip8->keyboard[0x6] = false;
                        break;
                    case SDL_SCANCODE_R:
                        chip8->keyboard[0xD] = false;
                        break;

                    case SDL_SCANCODE_A:
                        chip8->keyboard[0x7] = false;
                        break;
                    case SDL_SCANCODE_S:
                        chip8->keyboard[0x8] = false;
                        break;
                    case SDL_SCANCODE_D:
                        chip8->keyboard[0x9] = false;
                        break;
                    case SDL_SCANCODE_F:
                        chip8->keyboard[0xE] = false;
                        break;

                    case SDL_SCANCODE_Z:
                        chip8->keyboard[0xA] = false;
                        break;
                    case SDL_SCANCODE_X:
                        chip8->keyboard[0x0] = false;
                        break;
                    case SDL_SCANCODE_C:
                        chip8->keyboard[0xB] = false;
                        break;
                    case SDL_SCANCODE_V:
                        chip8->keyboard[0xF] = false;
                        break;

                    default:
                        break;
                }
                break;

            default:
                break;
        }
    }
}

void play_audio(sdl_t *sdl, const config_t* config, bool playing) {
    if (!playing) {
        SDL_PauseAudioStreamDevice(sdl->audio_stream);
        SDL_ClearAudioStream(sdl->audio_stream);
        // Reset for fresh wave
        sdl->sample_index = 0;
        return;
    }

    const int sample_rate = sdl->audio_spec.freq;

    // Wave spends half samples hi have samples lo
    const uint64_t period = sample_rate / config->square_wave_freq;
    const uint64_t half_period = period / 2;

    // Keep roughly 2 frames in queue
    const int samples_per_frame = sample_rate / 60;
    const int target_samples = samples_per_frame * 2;
    // SDL expects data in bytes not samples
    const int target_bytes = target_samples * (int) sizeof (int16_t);

    // While there is less data in the stream than the target
    while (SDL_GetAudioStreamQueued(sdl->audio_stream) < target_bytes) {
        int16_t chunk[512];

        for (int i = 0; i < 512; i++) {
            const bool high = (sdl->sample_index / half_period) % 2;
            sdl->sample_index++;
            chunk[i] = high ? config->volume : -config->volume;
        }

        SDL_PutAudioStreamData(sdl->audio_stream, chunk, sizeof(chunk));
    }

    SDL_ResumeAudioStreamDevice(sdl->audio_stream);
}
