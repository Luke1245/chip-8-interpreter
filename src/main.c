#include <SDL3/SDL.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

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

        case 0x003:
            // 0x3XNN: Skips next instruction if VX == NN
            if (chip8->V[chip8->inst.X] == chip8->inst.NN) {
                chip8->PC += 2;
            }

            DEBUG_PRINT("Skip next INST if VX (V%0X) (0x%02X) == NN (0x%02X)\n",
                        chip8->inst.X, chip8->V[chip8->inst.X], chip8->inst.NN);
            break;

        case 0x004:
            // 0x4XNN: Skips next instruction if VX != NN
            if (chip8->V[chip8->inst.X] != chip8->inst.NN) {
                chip8->PC += 2;
            }

            DEBUG_PRINT("Skip next INST if VX (V%0X) (%02X) != NN (%02X)\n",
                        chip8->inst.X, chip8->V[chip8->inst.X], chip8->inst.NN);
            break;

        case 0x005:
            if (chip8->inst.N != 0) {
                break;
            }

            // 0x5XY0: Skips next instruction if VX == VY
            if (chip8->V[chip8->inst.X] == chip8->V[chip8->inst.Y]) {
                chip8->PC += 2;
            }

            DEBUG_PRINT(
                "Skip next INST if VX (V%0X) (0x%02X) == VY (V%0X) (0x%02X)\n",
                chip8->inst.X, chip8->V[chip8->inst.X], chip8->inst.Y,
                chip8->V[chip8->inst.Y]);
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

        case 0x008:
            const uint8_t VX = chip8->V[chip8->inst.X];
            const uint8_t VY = chip8->V[chip8->inst.Y];
            bool flag;

            switch (chip8->inst.N) {
                case 0x0:
                    // 0x8XY0: Sets VX to the value of VY
                    chip8->V[chip8->inst.X] = VY;

                    DEBUG_PRINT("Set V%X (0x%02X) to value of V%X (0x%02X)\n",
                                chip8->inst.X, VX, chip8->inst.Y, VY);
                    break;

                case 0x1:
                    // 0x8XY1: Sets VX to VX bitwise OR VY
                    chip8->V[chip8->inst.X] |= VY;

                    DEBUG_PRINT(
                        "Set V%X (0x%02X) to bitwise OR V%X (0x%02X): RES = "
                        "0x%02X\n",
                        chip8->inst.X, VX, chip8->inst.Y, VY,
                        chip8->V[chip8->inst.X]);
                    break;

                case 0x2:
                    // 0x8XY2: Sets VX to VX bitwise AND VY
                    chip8->V[chip8->inst.X] &= VY;

                    DEBUG_PRINT(
                        "Set V%X (0x%02X) to bitwise AND V%X (0x%02X): RES = "
                        "0x%02X\n",
                        chip8->inst.X, VX, chip8->inst.Y, VY,
                        chip8->V[chip8->inst.X]);
                    break;

                case 0x3:
                    // 0x8XY3: Sets VX to VX bitwise XOR VY
                    chip8->V[chip8->inst.X] ^= VY;

                    DEBUG_PRINT(
                        "Set V%X (%02X) to bitwise XOR V%X (%02X): RES = "
                        "%02X\n",
                        chip8->inst.X, VX, chip8->inst.Y, VY,
                        chip8->V[chip8->inst.X]);
                    break;

                case 0x4:
                    // 0x8XY4: Adds VY to VX: VF set to 1 when overflow and 0
                    // when not
                    flag = (VX + VY) > 0xFF;

                    chip8->V[chip8->inst.X] += VY;
                    chip8->V[0xF] = flag;

                    DEBUG_PRINT(
                        "Added V%X (0x%02X) to V%X (0x%02X), overflow and VF = "
                        "1 "
                        "if RES (0x%02X) > 255 (0xFF)\n",
                        chip8->inst.Y, VY, chip8->inst.X, VX,
                        chip8->V[chip8->inst.X]);
                    break;

                case 0x5:
                    // 0x8XY5: Subtract VY from VX: VF set to 0 when underflow
                    // and 1 when not
                    flag = VX >= VY;

                    chip8->V[chip8->inst.X] -= VY;
                    chip8->V[0xF] = flag;

                    DEBUG_PRINT(
                        "Substracted V%X (0x%02X) from V%X (0x%02X), overflow "
                        "and "
                        "VF = 1 if V%X >= V%X, RES = 0x%02X\n",
                        chip8->inst.Y, VY, chip8->inst.X, VX, chip8->inst.X,
                        chip8->inst.Y, chip8->V[chip8->inst.X]);
                    break;

                case 0x6:
                    // 0x8XY6: Shift VX to right by 1, store LSB of VX before
                    // shift in VF Mask off top 7 bits
                    uint8_t lsb = VX & 0x01;

                    chip8->V[chip8->inst.X] >>= 1;
                    chip8->V[0xF] = lsb;

                    DEBUG_PRINT(
                        "Shift V%X (%02X) right by 1, set VF = LSB of VX pre "
                        "shift (%X), RES = %02X\n",
                        chip8->inst.X, VX, lsb, chip8->V[chip8->inst.X]);
                    break;

                case 0x7:
                    // 0x8XY7: Sets VX to VY - VX. VF unset if underflow, set if
                    // not
                    flag = VY >= VX;

                    chip8->V[chip8->inst.X] = VY - VX;
                    chip8->V[0xF] = flag;

                    DEBUG_PRINT(
                        "Set V%X to V%X (%02X) - V%X (%02X), set VF if VY >= "
                        "VX, RES: %02X, VF: %02X\n",
                        chip8->inst.X, chip8->inst.Y, VY, chip8->inst.X, VX,
                        chip8->V[chip8->inst.X], chip8->V[0xF]);
                    break;

                case 0xE:
                    // 0x8XYE: Shift VX to left by 1, set VF if MSB if set,
                    // unset if MSB is unset Shift MSB to LSB, mask off top 7
                    // bits (avoids extra conditional code for setting VF)
                    uint8_t msb = (VX >> 7) & 0x01;

                    chip8->V[chip8->inst.X] <<= 1;
                    chip8->V[0xF] = msb;

                    DEBUG_PRINT(
                        "Shift V%X (0x%02X) left by 1, set VF if MSB (0x%X) is "
                        "set, unset if 0, RES: 0x%02X\n",
                        chip8->inst.X, VX, msb, chip8->V[chip8->inst.X]);
                    break;

                default:
                    DEBUG_PRINT("Incorrect opcode\n");
            }
            break;

        case 0x009:
            // 0x9XY0: Skip next instruction if VX != VY

            if (chip8->V[chip8->inst.X] != chip8->V[chip8->inst.Y]) {
                chip8->PC += 2;
            }

            DEBUG_PRINT(
                "Skip next instruction if V%X (0x%02X) != V%X (0x%02X)\n",
                chip8->inst.X, chip8->V[chip8->inst.X], chip8->inst.Y,
                chip8->V[chip8->inst.Y]);
            break;

        case 0x00A:
            // 0xANNN: Set I to NNN
            chip8->I = chip8->inst.NNN;

            DEBUG_PRINT("Set I to 0x%04X\n", chip8->I);
            break;

        case 0x00B:
            // 0xBNNN: Jump to address NNN + V0
            chip8->PC = chip8->inst.NNN + chip8->V[0x0];

            DEBUG_PRINT(
                "Jump to address NNN (0x%04X) + V0 (0x%02X): RES = 0x%0X\n",
                chip8->inst.NNN, chip8->V[0x0], chip8->PC);
            break;

        case 0x00C:
            // 0xCXNN: Set VX to result of bitwise and operation on a random
            // number
            uint8_t rand_val = rand() % 256;
            chip8->V[chip8->inst.X] = rand_val & chip8->inst.NN;

            DEBUG_PRINT(
                "Set V%X to rand_val (0x%04X) bitwise AND NN (0x%02X) RES = "
                "0x%02X\n",
                chip8->inst.X, rand_val, chip8->inst.NN,
                chip8->V[chip8->inst.X]);
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

        case 0x00E:
            uint8_t key = chip8->V[chip8->inst.X];
            if (chip8->inst.NN == 0x9E) {
                // 0xEX9E: Skip next instruction if key stored in VX is pressed
                if (chip8->keyboard[key]) {
                    chip8->PC += 2;

                    DEBUG_PRINT(
                        "Skip next instruction if key in V%X is pressed: Key "
                        "value: %d\n",
                        chip8->inst.X, chip8->keyboard[key]);
                }
            } else if (chip8->inst.NN == 0xA1) {
                // 0xEXA1: Skip next instruction if key stored in VX is not
                // pressed
                if (!chip8->keyboard[key]) {
                    chip8->PC += 2;

                    DEBUG_PRINT(
                        "Skip next instruction if key in V%X is NOT pressed: "
                        "Key value: %d\n",
                        chip8->inst.X, chip8->keyboard[key]);
                }
            }
            break;

        case 0x00F:
            switch (chip8->inst.NN) {
                case 0x07:
                    // 0xFX07: Set VX to value of the delay timer
                    chip8->V[chip8->inst.X] = chip8->delay_timer;
                    DEBUG_PRINT("Set V%X to value of delay timer (%02X)\n",
                                chip8->inst.X, chip8->delay_timer);
                    break;

                case 0x0A:
                    // 0xFX0A: Await key press, key pressed and stored in VX
                    // (instruction processing halted until next key event)
                    bool key_press = false;
                    for (uint8_t i = 0; i < sizeof chip8->keyboard; i++) {
                        if (chip8->keyboard[i]) {
                            chip8->V[chip8->inst.X] = i;
                            key_press = true;
                            break;
                        }
                    }
                    if (!key_press) {
                        // If no key press, repeat instruction until key press
                        chip8->PC -= 2;
                    }

                    DEBUG_PRINT(
                        "Await key press: pressed key? %d, key is: 0x%02X\n",
                        key_press, chip8->V[chip8->inst.X]);
                    break;

                case 0x15:
                    // 0xFX15: Sets delay timer to VX
                    chip8->delay_timer = chip8->V[chip8->inst.X];

                    DEBUG_PRINT("Set delay timer to V%X (0x%02X)\n",
                                chip8->inst.X, chip8->V[chip8->inst.X]);
                    break;

                case 0x18:
                    // 0xFX18: Sets sound timer to VX
                    chip8->sound_timer = chip8->V[chip8->inst.X];

                    DEBUG_PRINT("Set sound timer to V%X (0x%02X)\n",
                                chip8->inst.X, chip8->V[chip8->inst.X]);
                    break;

                case 0x1E:
                    // 0xFX1E: Add VX to I: VF is uneffected
                    chip8->I += chip8->V[chip8->inst.X];

                    DEBUG_PRINT(
                        "Add V%X (0x%02X) to I (0x%02X), RES = 0x%02X \n",
                        chip8->inst.X, chip8->V[chip8->inst.X],
                        (chip8->I - chip8->V[chip8->inst.X]), chip8->I);
                    break;

                case 0x29:
                    // 0xFX29: Sets I to location of the sprite for the
                    // character in VX
                    chip8->I = chip8->V[chip8->inst.X] * 5;

                    DEBUG_PRINT(
                        "Set I to location of sprite in memory for character "
                        "in V%X (0x%02X) RES = VX * 5 (0x%02X)\n",
                        chip8->inst.X, chip8->V[chip8->inst.X],
                        chip8->V[chip8->inst.X] * 5);
                    break;

                case 0x33:
                    // 0xFX33: Store binary-coded decimal representation of VX,
                    // hundreds at I, tens at I+1, ones at I+2
                    uint8_t bcd = chip8->V[chip8->inst.X];
                    for (int8_t i = 2; i >= 0; i--) {
                        chip8->ram[chip8->I + i] = bcd % 10;
                        bcd /= 10;
                    }

                    DEBUG_PRINT(
                        "Store BCD representation of V%X (0x%02X) at memory "
                        "from I (0x%04X)\n",
                        chip8->inst.X, chip8->V[chip8->inst.X], chip8->I);
                    break;

                case 0x55:
                    // 0xFX55: Stores from V0 to VX (inclusive) in memory,
                    // starting at address I
                    for (uint8_t i = 0; i <= chip8->inst.X; i++) {
                        chip8->ram[chip8->I + i] = chip8->V[i];
                    }

                    DEBUG_PRINT(
                        "Store from V0-V%X inclusive at memory from I "
                        "(0x%04X)\n",
                        chip8->inst.X, chip8->I);
                    break;
                case 0x65:
                    // 0xFX55: Fills from V0 to VX (inclusive) with values from
                    // memory, starting at address I
                    for (uint8_t i = 0; i <= chip8->inst.X; i++) {
                        chip8->V[i] = chip8->ram[chip8->I + i];
                    }

                    DEBUG_PRINT(
                        "Fills from V0-V%X inclusive with values from memory "
                        "starting at I (0x%04X)\n",
                        chip8->inst.X, chip8->I);
                    break;

                default:
                    break;
            }
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
        SDL_Delay(16);

        handle_input(&chip8);

        if (chip8.state == PAUSED) continue;

        emulate_instruction(&chip8, config);

        update_screen(sdl, config, chip8);
    }

    exit_cleanup(&sdl);

    return EXIT_SUCCESS;
}
