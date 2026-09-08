# CHIP-8 Interpreter

A CHIP-8 Interpreter written in C23 with SDL3 for graphics, audio, and input.

CHIP-8 is an interpreted programming language from the 1970s. Initially used for the [COSMAC VIP](https://en.wikipedia.org/wiki/COSMAC_VIP) and [Telmac 1800](https://en.wikipedia.org/wiki/Telmac_1800). CHIP-8 is intended to use less memory than other languages at the time, while still being a helpful abstraction up from machine code. This project implements its Opcodes, memory map, keyboard and display from the original spec. This allows it to run ROMs written for it in the 70s and 80s. 

> **Status**: In development. All functionality has been implemented, apart from configurable modes for interpreter quirks.

![A gif of the classic Brick video game running on the CHIP-8 interpreter](docs/assets/brick_chip8.gif)

## References
- [Cowgod's Chip-8 Technical Reference](http://devernay.free.fr/hacks/chip8/C8TECH10.HTM)
- [Tobias V. I. Langhoff's High Level Guide](https://tobiasvl.github.io/blog/write-a-chip-8-emulator/)
- [Kripod's ROM repository ](https://github.com/kripod/chip8-roms)
- [Timendus, CHIP-8 Test Suite](https://github.com/Timendus/chip8-test-suite)

## License

Released under the MIT License. See [LICENSE](LICENSE) for details.
