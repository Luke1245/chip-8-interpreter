#include "chip8.h"

#include <SDL3/SDL.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "debug.h"

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
        0xF0, 0x90, 0x90, 0x90, 0xF0,  // 0
        0x20, 0x60, 0x20, 0x20, 0x70,  // 1
        0xF0, 0x10, 0xF0, 0x80, 0xF0,  // 2
        0xF0, 0x10, 0xF0, 0x10, 0xF0,  // 3
        0x90, 0x90, 0xF0, 0x10, 0x10,  // 4
        0xF0, 0x80, 0xF0, 0x10, 0xF0,  // 5
        0xF0, 0x80, 0xF0, 0x90, 0xF0,  // 6
        0xF0, 0x10, 0x20, 0x40, 0x40,  // 7
        0xF0, 0x90, 0xF0, 0x90, 0xF0,  // 8
        0xF0, 0x90, 0xF0, 0x10, 0xF0,  // 9
        0xF0, 0x90, 0xF0, 0x90, 0x90,  // A
        0xE0, 0x90, 0xE0, 0x90, 0xE0,  // B
        0xF0, 0x80, 0x80, 0x80, 0xF0,  // C
        0xE0, 0x90, 0x90, 0x90, 0xE0,  // D
        0xF0, 0x80, 0xF0, 0x80, 0xF0,  // E
        0xF0, 0x80, 0xF0, 0x80, 0x80   // F
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

void emulate_instruction(chip8_t* chip8, const config_t* config) {
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
                    if (config->extension == CHIP8) {
                        // Original CHIP-8 spec resets the flag register to zero
                        chip8->V[0xF] = 0x0;
                    }

                    DEBUG_PRINT(
                        "Set V%X (0x%02X) to bitwise OR V%X (0x%02X): RES = "
                        "0x%02X\n",
                        chip8->inst.X, VX, chip8->inst.Y, VY,
                        chip8->V[chip8->inst.X]);
                    break;

                case 0x2:
                    // 0x8XY2: Sets VX to VX bitwise AND VY
                    chip8->V[chip8->inst.X] &= VY;
                    if (config->extension == CHIP8) {
                        // Original CHIP-8 spec resets the flag register to zero
                        chip8->V[0xF] = 0x0;
                    }

                    DEBUG_PRINT(
                        "Set V%X (0x%02X) to bitwise AND V%X (0x%02X): RES = "
                        "0x%02X\n",
                        chip8->inst.X, VX, chip8->inst.Y, VY,
                        chip8->V[chip8->inst.X]);
                    break;

                case 0x3:
                    // 0x8XY3: Sets VX to VX bitwise XOR VY
                    chip8->V[chip8->inst.X] ^= VY;
                    if (config->extension == CHIP8) {
                        // Original CHIP-8 spec resets the flag register to zero
                        chip8->V[0xF] = 0x0;
                    }

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
            uint8_t X_coord = chip8->V[chip8->inst.X] % config->window_width;
            uint8_t Y_coord = chip8->V[chip8->inst.Y] % config->window_height;
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
                        if (config->extension != SUPERCHIP) {
                            chip8->ram[chip8->I] = chip8->V[i];
                            chip8->I++;
                        } else {
                            chip8->ram[chip8->I + i] = chip8->V[i];
                        }
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
                        if (config->extension != SUPERCHIP) {
                            chip8->V[i] = chip8->ram[chip8->I];
                            chip8->I++;
                        } else {
                            chip8->V[i] = chip8->ram[chip8->I + i];
                        }
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

bool update_timers(chip8_t* chip8) {
    if (chip8->delay_timer > 0) {
        chip8->delay_timer--;
    }

    if (chip8->sound_timer > 0) {
        chip8->sound_timer--;
        return true;
    } else {
        return false;
    }
}
