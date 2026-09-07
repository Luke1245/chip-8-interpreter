#ifndef CHIP8_H
#define CHIP8_H

#include <stdint.h>

#include "config.h"

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

bool initialise_chip8(chip8_t* chip8, const char* rom_name);
void emulate_instruction(chip8_t* chip8, const config_t config);
void update_timers(chip8_t* chip8);

#endif
