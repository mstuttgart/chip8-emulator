/**
 * @file chip8_disassembler.c
 * @brief CHIP-8 disassembler implementation.
 *
 * CHIP-8 does not have an official assembly language, so the mnemonics
 * follow the assembly language developed by David Winter in his
 * CHIP-8/SCHIP manual.
 */

#include "chip8_disassembler.h"

#include <stdio.h>
#include <stdlib.h>

void chip8_disassembler_init(Chip8Disassembler *da)
{
    da->buffer = NULL;
    da->opcode = 0;
    da->size_file = 0;
    da->PC = 0x200;
}

void chip8_disassembler_free(Chip8Disassembler *da)
{
    if (da->buffer) {
        free(da->buffer);
        da->buffer = NULL;
    }
}

bool chip8_disassembler_load(Chip8Disassembler *da, const char *file)
{
    FILE *is = fopen(file, "rb");

    if (!is) {
        return false;
    }

    /* Get the length of the file */
    fseek(is, 0, SEEK_END);
    long length = ftell(is);
    fseek(is, 0, SEEK_SET);

    if (da->buffer) {
        free(da->buffer);
    }

    da->size_file = length + 0x200;

    da->buffer = calloc(da->size_file, 1);

    /* Read the ROM into the buffer, offset by 0x200 as in memory */
    size_t read = fread(&da->buffer[0x200], 1, (size_t)length, is);

    fclose(is);

    if (read != (size_t)length) {
        printf("error: only %zu could be read", read);
        return false;
    }

    return true;
}

void chip8_disassembler_disassemble(Chip8Disassembler *da)
{
    printf("=========================================\n\n");
    printf("PC\tOPCODE\tASSEMBLY\n\n");

    for (da->PC = 0x200; da->PC < da->size_file; da->PC += 2) {
        /* The opcode is the concatenation of two consecutive bytes */
        uint16_t opcode = (da->buffer[da->PC] << 8) | da->buffer[da->PC + 1];
        da->opcode = opcode;

        printf("%04X\t%04X\t", da->PC, opcode);

        switch (opcode & 0xF000) {
        case 0x000:
            switch (opcode & 0x00FF) {
            case 0x00E0: /* 00E0: clear screen */
                printf("CLS\n");
                break;

            case 0x00EE: /* 00EE: return from a subroutine */
                printf("RET\n");
                break;

            default:
                printf("UKNO\t%X\n", opcode);
            }

            break;

        case 0x1000: /* 1NNN: jump to address NNN */
            printf("JP\t%03X\n", opcode & 0xFFF);
            break;

        case 0x2000: /* 2NNN: call subroutine at NNN */
            printf("CALL\t%03X\n", opcode & 0xFFF);
            break;

        case 0x3000: /* 3XNN: skip the next instruction if VX equals NN */
            printf("SE\tV%X%02X\n", (opcode & 0x0F00) >> 8, opcode & 0x0FF);
            break;

        case 0x4000: /* 4XNN: skip the next instruction if VX doesn't equal NN */
            printf("SNE\tV%X\t%02X\n", (opcode & 0x0F00) >> 8, opcode & 0x0FF);
            break;

        case 0x5000: /* 5XY0: skip the next instruction if VX equals VY */
            printf("SE\tV%X\tV%X\n", (opcode & 0x0F00) >> 8,
                   (opcode & 0x00F0) >> 4);
            break;

        case 0x6000: /* 6XNN: set VX to NN */
            printf("LD\tV%X\t%02X\n", (opcode & 0x0F00) >> 8, opcode & 0x0FF);
            break;

        case 0x7000: /* 7XNN: add NN to VX */
            printf("ADD\tV%X\t%02X\n", (opcode & 0x0F00) >> 8, opcode & 0x0FF);
            break;

        case 0x8000:
            switch (opcode & 0x000F) {
            case 0x0000: /* 8XY0: set VX to the value of VY */
                printf("LD\tV%X\tV%X\n", (opcode & 0x0F00) >> 8,
                       (opcode & 0x00F0) >> 4);
                break;

            case 0x0001: /* 8XY1: set VX to VX or VY */
                printf("OR\tV%X\tV%X\n", (opcode & 0x0F00) >> 8,
                       (opcode & 0x00F0) >> 4);
                break;

            case 0x0002: /* 8XY2: set VX to VX and VY */
                printf("AND\tV%X\tV%X\n", (opcode & 0x0F00) >> 8,
                       (opcode & 0x00F0) >> 4);
                break;

            case 0x0003: /* 8XY3: set VX to VX xor VY */
                printf("XOR\tV%X\tV%X\n", (opcode & 0x0F00) >> 8,
                       (opcode & 0x00F0) >> 4);
                break;

            case 0x0004: /* 8XY4: add VY to VX, VF is the carry flag */
                printf("ADD\tV%X\tV%X\n", (opcode & 0x0F00) >> 8,
                       (opcode & 0x00F0) >> 4);
                break;

            case 0x0005: /* 8XY5: subtract VY from VX, VF is the borrow flag */
                printf("SUB\tV%X\tV%X\n", (opcode & 0x0F00) >> 8,
                       (opcode & 0x00F0) >> 4);
                break;

            case 0x0006: /* 8XY6: shift VX right by one, VF holds the old LSB */
                printf("SHR\tV%X\n", (opcode & 0x0F00) >> 8);
                break;

            case 0x0007: /* 8XY7: set VX to VY minus VX, VF is the borrow flag */
                printf("SUBN\tV%X\tV%X\n", (opcode & 0x0F00) >> 8,
                       (opcode & 0x00F0) >> 4);
                break;

            case 0x000E: /* 8XYE: shift VX left by one, VF holds the old MSB */
                printf("SHL\tV%X\n", (opcode & 0x0F00) >> 8);
                break;

            default:
                printf("UKNO\t%X\n", opcode);
                break;
            }

            break;

        case 0x9000: /* 9XY0: skip the next instruction if VX doesn't equal VY */
            printf("SNE\tV%X\tV%X\n", (opcode & 0x0F00) >> 8,
                   (opcode & 0x00F0) >> 4);
            break;

        case 0xA000: /* ANNN: set I to the address NNN */
            printf("LD\tI\t%03X\n", opcode & 0xFFF);
            break;

        case 0xB000: /* BNNN: jump to the address NNN plus V0 */
            printf("JP\tV0\t%03X\n", opcode & 0x0FFF);
            break;

        case 0xC000: /* CXNN: set VX to a random number masked by NN */
            printf("RND\tV%X\t%02X\n", (opcode & 0x0F00) >> 8, opcode & 0x00FF);
            break;

        case 0xD000: /* DXYN: draw an N-byte sprite at (VX, VY) */
            printf("DRW\tV%X\tV%X\t%X\n", (opcode & 0x0F00) >> 8,
                   (opcode & 0x00F0) >> 4, opcode & 0x000F);
            break;

        case 0xE000:
            switch (opcode & 0x00FF) {
            case 0x009E:
                /* EX9E: skip the next instruction if the key stored
                 * in VX is pressed */
                printf("SKP\tV%X\n", (opcode & 0x0F00) >> 8);
                break;

            case 0x00A1:
                /* EXA1: skip the next instruction if the key stored
                 * in VX isn't pressed */
                printf("SKNP\tV%X\n", (opcode & 0x0F00) >> 8);
                break;

            default:
                printf("UKNO\t%X\n", opcode);
            }

            break;

        case 0xF000:
            switch (opcode & 0x00FF) {
            case 0x0007: /* FX07: set VX to the value of the delay timer */
                printf("LD\tV%X\tDT\n", (opcode & 0x0F00) >> 8);
                break;

            case 0x000A: /* FX0A: wait for a key press and store it in VX */
                printf("LD\tV%X\tWK\n", (opcode & 0x0F00) >> 8);
                break;

            case 0x0015: /* FX15: set the delay timer to VX */
                printf("LD\tDT\tV%X\n", (opcode & 0x0F00) >> 8);
                break;

            case 0x0018: /* FX18: set the sound timer to VX */
                printf("LD\tST\tV%X\n", (opcode & 0x0F00) >> 8);
                break;

            case 0x001E: /* FX1E: add VX to I */
                printf("ADD\tI\tV%X\n", (opcode & 0x0F00) >> 8);
                break;

            case 0x0029:
                /* FX29: set I to the location of the 4x5 font sprite
                 * for the character in VX */
                printf("LD\tSPR\tV%X\n", (opcode & 0x0F00) >> 8);
                break;

            case 0x0033:
                /* FX33: store the binary-coded decimal representation
                 * of VX at the addresses I, I plus 1, and I plus 2 */
                printf("LD\tBCD\tV%X\n", (opcode & 0x0F00) >> 8);
                break;

            case 0x0055: /* FX55: store V0 to VX in memory starting at I */
                printf("LD\t[I]\tV0 - V%X\n", (opcode & 0x0F00) >> 8);
                break;

            case 0x0065: /* FX65: fill V0 to VX from memory starting at I */
                printf("LD\tV%X\t[i]\n", (opcode & 0x0F00) >> 8);
                break;

            default:
                printf("UKNO\t%X\n", opcode);
                break;
            }

            break;

        default:
            printf("UKNO\t%X\n", opcode);
            break;
        }
    }

    printf("\n=========================================\n\n");
}
