#include <SDL3/SDL.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef DEBUG
#define DEBUG_PRINT(...) printf(__VA_ARGS__)
#else
#define DEBUG_PRINT(...) \
    do {                 \
    } while (0)
#endif

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
    uint16_t* stack_ptr;
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

    // Load defaults
    *chip8 = (chip8_t){
        .state = RUNNING,
        .PC = entry_point,
        .rom_name = rom_name,
        .stack_ptr = &chip8->stack[0],
    };

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

void update_screen(const sdl_t sdl, const config_t config,
                   const chip8_t chip8) {
    SDL_FRect rect = {
        .x = 0, .y = 0, .w = config.scale_factor, .h = config.scale_factor};

    const uint8_t fg_r = (config.fg_colour >> 24) & 0xFF;
    const uint8_t fg_g = (config.fg_colour >> 16) & 0xFF;
    const uint8_t fg_b = (config.fg_colour >> 8) & 0xFF;
    const uint8_t fg_a = (config.fg_colour >> 0) & 0xFF;

    const uint8_t bg_r = (config.bg_colour >> 24) & 0xFF;
    const uint8_t bg_g = (config.bg_colour >> 16) & 0xFF;
    const uint8_t bg_b = (config.bg_colour >> 8) & 0xFF;
    const uint8_t bg_a = (config.bg_colour >> 0) & 0xFF;

    for (uint32_t i = 0; i < sizeof(chip8.display); i++) {
        rect.x = (i % config.window_width) * config.scale_factor;
        rect.y = (i / config.window_width) * config.scale_factor;

        if (chip8.display[i]) {
            // Pixel is on: draw foreground colour
            SDL_SetRenderDrawColor(sdl.renderer, fg_r, fg_g, fg_b, fg_a);
            SDL_RenderFillRect(sdl.renderer, &rect);
        } else {
            // Pixel is off: draw background colour
            SDL_SetRenderDrawColor(sdl.renderer, bg_r, bg_g, bg_b, bg_a);
            SDL_RenderFillRect(sdl.renderer, &rect);
        }
    }

    SDL_RenderPresent(sdl.renderer);
}

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

void emulate_instruction(chip8_t* chip8, const config_t config) {
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

    DEBUG_PRINT("Address: 0x%04X, Opcode: 0x%04X, Desc: ", chip8->PC - 2,
                chip8->inst.opcode);

    switch ((chip8->inst.opcode >> 12) & 0x000F) {
        case 0x000:
            if (chip8->inst.NN == 0xE0) {
                // 0x00E0: Clear screen
                memset(&chip8->display[0], 0, sizeof(chip8->display));

                DEBUG_PRINT("Clear screen\n");

            } else if (chip8->inst.NN == 0xEE) {
                // 0xEE: Return from a subroutine
                chip8->stack_ptr--;
                chip8->PC = *chip8->stack_ptr;

                DEBUG_PRINT("Return to address 0x%04X\n", chip8->PC);
            }
            break;

        case 0x001:
            // 0x1NNN: Jumps to address NNN
            chip8->PC = chip8->inst.NNN;

            DEBUG_PRINT("Jump to address 0x%04X\n", chip8->inst.NNN);
            break;

        case 0x002:
            // 0x2NNN: Calls a subroutine at NNN
            *chip8->stack_ptr = chip8->PC;
            chip8->stack_ptr++;
            chip8->PC = chip8->inst.NNN;

            DEBUG_PRINT("Call subroutine at 0x%04X\n", chip8->inst.NNN);
            break;

        case 0x006:
            // 0x6XNN: Sets VX to NN
            chip8->V[chip8->inst.X] = chip8->inst.NN;

            DEBUG_PRINT("Set register V%X = 0x%02X\n", chip8->inst.X,
                        chip8->inst.NN);
            break;

        case 0x007:
            // 0x7XNN: Adds NN to VX
            chip8->V[chip8->inst.X] += chip8->inst.NN;

            DEBUG_PRINT("V%X += 0x%02X, RES: 0x%02X\n", chip8->inst.X,
                        chip8->inst.NN, chip8->V[chip8->inst.X]);
            break;

        case 0x00A:

            // 0xANNN: Set I to NNN
            chip8->I = chip8->inst.NNN;

            DEBUG_PRINT("Set I to %04X\n", chip8->I);
            break;

        case 0x00D:
            // 0xDXYN: Draws a sprite at coord (VX, VY).
            //  Sprite has a width of 8 pixels and a height of N pixels
            uint8_t X_coord = chip8->V[chip8->inst.X] % config.window_width;
            uint8_t Y_coord = chip8->V[chip8->inst.Y] % config.window_height;
            const uint8_t X_origin = X_coord;

            chip8->V[0xF] = 0;  // Init carry flag

            // Loop over every row of sprite
            for (uint8_t i = 0; i < chip8->inst.N; i++) {
                const uint8_t sprite_data = chip8->ram[chip8->I + i];
                X_coord = X_origin;

                for (int8_t j = 7; j >= 0; j--) {
                    // Set VF if sprite pixel and display pixel is on
                    // (collision)
                    bool* pixel = &chip8->display[Y_coord * 64 + X_coord];
                    bool sprite_bit = (sprite_data & (1 << j));

                    if (sprite_bit && *pixel) {
                        chip8->V[0xF] = 1;
                    }

                    *pixel ^= sprite_bit;

                    // If hit right edge of screen stop drawing
                    if (X_coord++ >= 64) break;
                }
                // If hit bottom edge of screen stop drawing
                if (Y_coord++ >= 32) break;
            }

            DEBUG_PRINT(
                "Draw %u height sprite at coords V%X (0x%02X), V%X (0x%02X). "
                "From memory location 0x%04X. Set VF if any collisions\n",
                chip8->inst.N, chip8->inst.X, chip8->V[chip8->inst.X],
                chip8->inst.Y, chip8->V[chip8->inst.Y], chip8->I);
            break;

        default:
            DEBUG_PRINT("Unimplemented instruction\n");
            break;
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

    clear_screen(config, sdl);

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

        if (chip8.state == PAUSED) continue;

        emulate_instruction(&chip8, config);

        update_screen(sdl, config, chip8);
    }

    exit_cleanup(&sdl);

    return EXIT_SUCCESS;
}
