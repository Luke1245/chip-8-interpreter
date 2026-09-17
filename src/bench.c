#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "chip8.h"
#include "config.h"

static double fetch_time(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);

    double timestamp = (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
    return timestamp;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <rom> [instructions]\n", argv[0]);
        return EXIT_FAILURE;
    }

    // Set defaults
    uint64_t instructions = 100000000ULL;
    if (argc > 2) {
        instructions = strtoull(argv[2], NULL, 10);
    }

    const config_t config = {
        .window_width = 64,
        .window_height = 32,
        .extension = CHIP8,
    };

    chip8_t chip8 = {0};
    if (!initialise_chip8(&chip8, argv[1])) {
        return EXIT_FAILURE;
    }

    // Warm-up
    for (uint64_t i = 0; i < 1000000; i++) {
        emulate_instruction(&chip8, &config);
    }

    const double t0 = fetch_time();

    for (uint64_t i = 0; i < instructions; i++) {
        emulate_instruction(&chip8, &config);
    }

    const double t1 = fetch_time();

    const double secs = t1 - t0;

    unsigned checksum = chip8.I + chip8.PC;
    for (int r = 0; r < 16; r++) checksum += chip8.V[r];

    printf("instructions: %llu\n", (unsigned long long)instructions);
    printf("elapsed:      %.3f s\n", secs);
    printf("throughput:   %.1f M instr/s\n", (double)instructions / secs / 1e6);
    printf("per-instruction:    %.2f ns\n", secs / (double)instructions * 1e9);
    printf("checksum:     %u\n", checksum);
    return EXIT_SUCCESS;
}
