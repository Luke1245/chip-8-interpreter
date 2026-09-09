# CHIP-8 Interpreter

A CHIP-8 Interpreter written in C23 with SDL3 for graphics, audio, and input.

CHIP-8 is an interpreted programming language from the 1970s. Initially used for the [COSMAC VIP](https://en.wikipedia.org/wiki/COSMAC_VIP) and [Telmac 1800](https://en.wikipedia.org/wiki/Telmac_1800). CHIP-8 is intended to use less memory than other languages at the time, while still being a helpful abstraction up from machine code. This project implements its Opcodes, memory map, keyboard and display from the original spec. This allows it to run ROMs written for it in the 70s and 80s. 

> **Status**: Complete. All functionality has been implemented.

![A gif of the classic Brick video game running on the CHIP-8 interpreter](docs/assets/brick_chip8.gif)

## Features
- Full implementation of CHIP-8 instruction set, with 35-opcodes
- 64 $\times$ 32 original pixel display with scale factors and custom colours
- 60 Hz delay and sound timers, independent of CPU execution speed
- Hexadecimal keypad with mappings to scancode EN-US QWERTY layout
- Switch between extension behaviour through CLI

## Setup
### Dependencies
- C23-capable compiler (built with Apple clang version 21.0.0)
- SDL3 (built against SDL 3.4.12)
- Make

### Build
While packages for SDL3 do seem to exist. I had to [build from source](https://github.com/libsdl-org/SDL/blob/main/INSTALL.md)

```sh
git clone https://github.com/Luke1245/chip-8-interpreter/
cd chip-8-interpreter
make
```

### Run
```sh
./build/release/main <rom_path> [-s scale factor] [-e extension]
```

## References
- [Cowgod's Chip-8 Technical Reference](http://devernay.free.fr/hacks/chip8/C8TECH10.HTM)
- [Tobias V. I. Langhoff's High Level Guide](https://tobiasvl.github.io/blog/write-a-chip-8-emulator/)
- [Kripod's ROM repository ](https://github.com/kripod/chip8-roms)
- [Timendus, CHIP-8 Test Suite](https://github.com/Timendus/chip8-test-suite)

## License

Released under the MIT License. See [LICENSE](LICENSE) for details.
