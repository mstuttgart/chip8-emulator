/**
 * @file chip8_disassembler.h
 * @brief Command line disassembler for CHIP-8 ROMs.
 *
 * Reference documentation:
 * - CHIP-8 instruction set:
 *   https://multigesture.net/wp-content/uploads/mirror/goldroad/chip8_instruction_set.shtml
 * - Cowgod's CHIP-8 technical reference:
 *   http://devernay.free.fr/hacks/chip8/C8TECH10.HTM
 */

#ifndef CHIP8_DISASSEMBLER_H
#define CHIP8_DISASSEMBLER_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Disassembler state: ROM buffer and current position.
 */
typedef struct Chip8Disassembler {
    uint8_t *buffer;        /**< ROM content, offset by 0x200 as in memory. */
    uint16_t opcode;        /**< Currently decoded opcode. */
    unsigned int size_file; /**< Buffer size (ROM length + 0x200). */
    uint16_t PC;            /**< Current program counter. */
} Chip8Disassembler;

/**
 * @brief Initializes the disassembler.
 * @param da Disassembler instance.
 */
void chip8_disassembler_init(Chip8Disassembler *da);

/**
 * @brief Releases the disassembler resources.
 * @param da Disassembler instance.
 */
void chip8_disassembler_free(Chip8Disassembler *da);

/**
 * @brief Loads a ROM to be disassembled.
 * @param da Disassembler instance.
 * @param file Path to the ROM file.
 * @return true on success, false otherwise.
 */
bool chip8_disassembler_load(Chip8Disassembler *da, const char *file);

/**
 * @brief Prints the disassembly of the loaded ROM to stdout.
 * @param da Disassembler instance.
 */
void chip8_disassembler_disassemble(Chip8Disassembler *da);

#endif /* CHIP8_DISASSEMBLER_H */
