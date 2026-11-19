/**
 * @file main.c
 * @brief Command line entry point of the CHIP-8 disassembler.
 */

#include "chip8_disassembler.h"

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[])
{
    if (argc == 1) {
        printf("Usage: %s <rom-file>\n", argv[0]);
        return -1;
    }

    Chip8Disassembler da_chip8;
    chip8_disassembler_init(&da_chip8);

    if (!chip8_disassembler_load(&da_chip8, argv[1])) {
        printf("Failed to load file: %s\n", argv[1]);
        return -1;
    }

    chip8_disassembler_disassemble(&da_chip8);

    chip8_disassembler_free(&da_chip8);

#ifdef WIN32
    system("pause");
#endif

    return 0;
}
