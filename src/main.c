#include <SDL3/SDL.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct {
    SDL_Window* window;
    SDL_Renderer* renderer;
} sdl_t;

typedef struct {
    uint32_t window_width;
    uint32_t window_height;
    uint32_t fg_colour;     // Foreground colour
    uint32_t bg_colour;     // Background colour
    uint32_t scale_factor;  // Amount to scale CHIP-8 pixel by (Original
                            // resolution is too small for modern displays)
} config_t;

typedef enum machine_state_type { RUNNING, PAUSED, QUIT } machine_state_t;

typedef struct {
    uint16_t opcode;  // 2 bytes of RAM = 1 opcode
    uint16_t NNN;     // address
    uint8_t NN;       // 8-bit constant
    uint8_t N;        // 4-bit (originally) constant
    uint8_t X;        // 4-bit (originally) register identifier
    uint8_t Y;        // 4-bit (originally) register identifier
} instruction_t;

typedef struct {
    machine_state_t state;
    uint8_t ram[4096];
    uint8_t V[16];       // Data registers V0-VF
    uint16_t I;          // Address register (originally 12 bits wide)
    uint16_t PC;         // Program counter
    instruction_t inst;  // Instruction being executed
    uint16_t stack[16];
    uint8_t delay_timer;    // Count down at 60hz
    uint8_t sound_timer;    // Count down at 60hz, play sound when value != 0
    bool display[64 * 32];  // Emulating original pixels ON or OFF
    bool keyboard[16];  // 16-key hexadecimal keyboard, Bool on or off key state
    const char* rom_name;  // ROM currently being emulated
} chip8_t;

bool set_config(config_t* config, int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    *config = (config_t){
        .window_width = 64,       // Original CHIP-8 resolution
        .window_height = 32,      // Original CHIP-8 resolution
        .fg_colour = 0xFFFFFFFF,  // RGBA8888
        .bg_colour = 0x000000FF,  // RGBA8888
        .scale_factor = 20,
    };

    // TODO: Implement command line arguments

    return true;
}

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

    return true;
}

bool initialise_chip8(chip8_t* chip8, const char* rom_name) {
    const uint32_t entry_point =
        0x200;  // First 512 bytes are where original interpreter was located

    // Load font
    const uint8_t font[] = {
#embed "font.bin"
    };

    memcpy(chip8->ram, font, sizeof(font));

    // Load ROM
    FILE* rom = fopen(rom_name, "rb");

    if (!rom) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "ROM file %s does not exist or is invalid\n", rom_name);
        return false;
    }

    // Check ROM size
    fseek(rom, 0, SEEK_END);
    const size_t rom_size = ftell(rom);
    const size_t max_size = sizeof chip8->ram - entry_point;
    rewind(rom);

    if (rom_size > max_size) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "ROM file %s is too big",
                     rom_name);
        return false;
    }

    if (fread(chip8->ram + entry_point, rom_size, 1, rom) != 1) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Unable to read ROM file into memory");
        return false;
    }

    fclose(rom);

    // Load defaults
    *chip8 = (chip8_t){
        .state = RUNNING,
        .PC = entry_point,
        .rom_name = rom_name,
    };

    return true;
}

void exit_cleanup(sdl_t* sdl) {
    // Renderer must be destroyed before window
    SDL_DestroyRenderer(sdl->renderer);
    SDL_DestroyWindow(sdl->window);
    SDL_Quit();
}

void clear_screen(const config_t config, const sdl_t sdl) {
    // Bit shifts and masks to extract relevant channels from 32-bit config
    // integer
    const uint8_t r = (config.bg_colour >> 24) & 0xFF;
    const uint8_t g = (config.bg_colour >> 16) & 0xFF;
    const uint8_t b = (config.bg_colour >> 8) & 0xFF;
    // Shift not needed but keeps consistent with style
    const uint8_t a = (config.bg_colour >> 0) & 0xFF;

    SDL_SetRenderDrawColor(sdl.renderer, r, g, b, a);
    SDL_RenderClear(sdl.renderer);
}

// This will do more later, currently wrapper
void update_screen(const sdl_t sdl) { SDL_RenderPresent(sdl.renderer); }

void handle_input(chip8_t* chip8) {
    SDL_Event event;

    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_EVENT_QUIT:
                chip8->state = QUIT;
                return;

            case SDL_EVENT_KEY_DOWN:
                switch (event.key.key) {
                    case SDLK_ESCAPE:
                        chip8->state = QUIT;
                        return;

                    case SDLK_SPACE:
                        if (chip8->state == RUNNING) {
                            chip8->state = PAUSED;
                        } else {
                            chip8->state = RUNNING;  // Resume
                        }
                        return;

                    default:
                        break;
                }

            case SDL_EVENT_KEY_UP:
                break;

            default:
                break;
        }
    }
}

void emulate_instruction(chip8_t* chip8) {
    // Combine two opcode RAM bytes into single value
    chip8->inst.opcode =
        (chip8->ram[chip8->PC] << 8) | (chip8->ram[chip8->PC + 1]);
    chip8->PC += 2;  // Move to next instruction

    // Obtain components of instruction format
    chip8->inst.NNN = (chip8->inst.opcode & 0x0FFF);
    chip8->inst.NN = (chip8->inst.opcode & 0x00FF);
    chip8->inst.N = (chip8->inst.opcode & 0x000F);
    chip8->inst.X = (chip8->inst.opcode >> 8) & 0x000F;
    chip8->inst.Y = (chip8->inst.opcode >> 4) & 0x000F;

    switch ((chip8->inst.opcode >> 12) & 0x000F) {
        default:
            break;  // Unimplemented instructions TODO: debug info?
    }
}

int main(int argc, char* argv[]) {
    sdl_t sdl = {0};
    config_t config = {0};
    chip8_t chip8 = {0};

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

    const char* rom_name = argv[1];
    if (!initialise_chip8(&chip8, rom_name)) {
        exit(EXIT_FAILURE);
    }

    // Main loop
    while (chip8.state != QUIT) {
        // Allow user to quit window
        // Delay for 60hz (approx)
        SDL_Delay(16);

        handle_input(&chip8);

        emulate_instruction(&chip8);

        if (chip8.state == PAUSED) continue;

        clear_screen(config, sdl);
        update_screen(sdl);
    }

    exit_cleanup(&sdl);

    return EXIT_SUCCESS;
}
