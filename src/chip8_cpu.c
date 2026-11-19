/**
 * @file chip8_cpu.c
 * @brief CHIP-8 CPU core implementation.
 */

#include "chip8_cpu.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

/** @brief Decodes the N nibble (lowest 4 bits) of the current opcode. */
static uint8_t decode_n(const Chip8CPU *cpu)
{
    return cpu->opcode & 0x000f;
}

/** @brief Decodes the NN byte (lowest 8 bits) of the current opcode. */
static uint8_t decode_nn(const Chip8CPU *cpu)
{
    return cpu->opcode & 0x00ff;
}

/** @brief Decodes the X register index of the current opcode. */
static uint8_t decode_x(const Chip8CPU *cpu)
{
    return (cpu->opcode & 0x0f00) >> 8;
}

/** @brief Decodes the Y register index of the current opcode. */
static uint8_t decode_y(const Chip8CPU *cpu)
{
    return (cpu->opcode & 0x00f0) >> 4;
}

void chip8_cpu_hard_reset(Chip8CPU *cpu)
{
    chip8_cpu_soft_reset(cpu);

    /* Clear the memory, except the fontset area */
    for (int i = 80; i < MEMORY_SIZE; i++) {
        cpu->memory[i] = 0;
    }

    static const uint8_t chip8_fontset[80] = {
        0xF0, 0x90, 0x90, 0x90, 0xF0, /* 0 */
        0x20, 0x60, 0x20, 0x20, 0x70, /* 1 */
        0xF0, 0x10, 0xF0, 0x80, 0xF0, /* 2 */
        0xF0, 0x10, 0xF0, 0x10, 0xF0, /* 3 */
        0x90, 0x90, 0xF0, 0x10, 0x10, /* 4 */
        0xF0, 0x80, 0xF0, 0x10, 0xF0, /* 5 */
        0xF0, 0x80, 0xF0, 0x90, 0xF0, /* 6 */
        0xF0, 0x10, 0x20, 0x40, 0x40, /* 7 */
        0xF0, 0x90, 0xF0, 0x90, 0xF0, /* 8 */
        0xF0, 0x90, 0xF0, 0x10, 0xF0, /* 9 */
        0xF0, 0x90, 0xF0, 0x90, 0x90, /* A */
        0xE0, 0x90, 0xE0, 0x90, 0xE0, /* B */
        0xF0, 0x80, 0x80, 0x80, 0xF0, /* C */
        0xE0, 0x90, 0x90, 0x90, 0xE0, /* D */
        0xF0, 0x80, 0xF0, 0x80, 0xF0, /* E */
        0xF0, 0x80, 0xF0, 0x80, 0x80  /* F */
    };

    /* Load the fontset */
    for (int i = 0; i < 80; i++) {
        cpu->memory[i] = chip8_fontset[i];
    }

    /* Seed the random number generator */
    srand(time(NULL));

    printf("Init CHIP8 CPU\n");
}

void chip8_cpu_soft_reset(Chip8CPU *cpu)
{
    cpu->opcode = 0;
    cpu->I = 0;
    cpu->SP = STACK_SIZE;
    cpu->PC = 0x200;

    cpu->extend_display_mode = false;

    /* Clear the stack, registers and key state vectors */
    for (int i = 0; i < 16; i++) {
        cpu->V[i] = 0;
        cpu->stack[i] = 0;
        cpu->current_key_state[i] = false;
        cpu->last_key_state[i] = false;
    }

    cpu->wait_for_key = false;

    for (int i = 0; i < DISPLAY; i++) {
        cpu->display[i] = false;
    }

    cpu->delay_timer = 0;
    cpu->sound_timer = 0;
}

bool chip8_cpu_load(Chip8CPU *cpu, const char *file_name)
{
    FILE *file = fopen(file_name, "rb");

    if (!file) {
        return false;
    }

    chip8_cpu_hard_reset(cpu);

    /* Get the length of the file */
    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    fseek(file, 0, SEEK_SET);

    if (length > (MEMORY_SIZE - 512)) {
        printf("ERROR: ROM too big for memory\n");
        fclose(file);
        return false;
    }

    printf("Loading ROM...\n");

    /* Read the file content directly into the CHIP-8 memory */
    size_t read = fread(&cpu->memory[0x200], 1, (size_t)length, file);

    fclose(file);

    if (read != (size_t)length) {
        printf("ERROR: only %zu could be read\n", read);
        return false;
    }

    return true;
}

void chip8_cpu_fetch(Chip8CPU *cpu)
{
    /* The opcode is the concatenation of two consecutive memory bytes */
    cpu->opcode = (cpu->memory[cpu->PC] << 8) | cpu->memory[cpu->PC + 1];
}

/** @brief Handles the 0x0NNN opcode group (CLS, RET and SCHIP screen ops). */
static void opcode_prefix_0(Chip8CPU *cpu)
{
    switch (cpu->opcode & 0x00FF) {
    case 0x00E0: /* 00E0: clear screen */
        for (int i = 0; i < DISPLAY; i++) {
            cpu->display[i] = false;
        }

        cpu->PC += 2;
        break;

    case 0x00EE: /* 00EE: return from a subroutine */
        cpu->PC = cpu->stack[cpu->SP];
        ++cpu->SP;
        break;

    case 0x00FB: /* 00FB: scroll screen 4 pixels right (SCHIP) */
        break;

    case 0x00FC: /* 00FC: scroll screen 4 pixels left (SCHIP) */
        break;

    case 0x00FE: /* 00FE: disable extended screen mode (SCHIP) */
        cpu->extend_display_mode = false;
        break;

    case 0x00FF: /* 00FF: enable extended screen mode, 128x64 (SCHIP) */
        cpu->extend_display_mode = true;
        break;

    default:
        switch (cpu->opcode & 0xFFF0) {
        case 0x00C0: /* 00CN: scroll screen N lines down (SCHIP) */
            break;

        default:
            printf("Invalid OpcodePrefix0\n");
            break;
        }

        break;
    }
}

/** @brief 1NNN: jumps to address NNN. */
static void opcode_prefix_1(Chip8CPU *cpu)
{
    cpu->PC = cpu->opcode & 0xfff;
}

/** @brief 2NNN: calls subroutine at NNN. */
static void opcode_prefix_2(Chip8CPU *cpu)
{
    cpu->SP--;
    cpu->stack[cpu->SP] = cpu->PC + 2;
    cpu->PC = cpu->opcode & 0x0fff;
}

/** @brief 3XNN: skips the next instruction if VX equals NN. */
static void opcode_prefix_3(Chip8CPU *cpu)
{
    cpu->PC += cpu->V[decode_x(cpu)] == decode_nn(cpu) ? 4 : 2;
}

/** @brief 4XNN: skips the next instruction if VX doesn't equal NN. */
static void opcode_prefix_4(Chip8CPU *cpu)
{
    cpu->PC += (cpu->V[decode_x(cpu)] != decode_nn(cpu)) ? 4 : 2;
}

/** @brief 5XY0: skips the next instruction if VX equals VY. */
static void opcode_prefix_5(Chip8CPU *cpu)
{
    cpu->PC += (cpu->V[decode_x(cpu)] == cpu->V[decode_y(cpu)]) ? 4 : 2;
}

/** @brief 6XNN: sets VX to NN. */
static void opcode_prefix_6(Chip8CPU *cpu)
{
    cpu->V[decode_x(cpu)] = decode_nn(cpu);
    cpu->PC += 2;
}

/** @brief 7XNN: adds NN to VX. */
static void opcode_prefix_7(Chip8CPU *cpu)
{
    cpu->V[decode_x(cpu)] += decode_nn(cpu);
    cpu->PC += 2;
}

/** @brief Handles the 8XY* opcode group (arithmetic and bitwise operations). */
static void opcode_prefix_8(Chip8CPU *cpu)
{
    uint8_t idx = decode_x(cpu);
    uint8_t idy = decode_y(cpu);

    switch (cpu->opcode & 0x000F) {
    case 0x0000: /* 8XY0: sets VX to the value of VY */
        cpu->V[idx] = cpu->V[idy];
        break;

    case 0x0001: /* 8XY1: sets VX to VX or VY */
        cpu->V[idx] = cpu->V[idx] | cpu->V[idy];
        break;

    case 0x0002: /* 8XY2: sets VX to VX and VY */
        cpu->V[idx] = cpu->V[idx] & cpu->V[idy];
        break;

    case 0x0003: /* 8XY3: sets VX to VX xor VY */
        cpu->V[idx] = cpu->V[idx] ^ cpu->V[idy];
        break;

    case 0x0004:
        /* 8XY4: adds VY to VX. VF is set to 1 when there's a carry,
         * and to 0 when there isn't. */
        cpu->V[0xf] = (cpu->V[idy] > (0xff - cpu->V[idx])) ? 1 : 0;
        cpu->V[idx] += cpu->V[idy];
        break;

    case 0x0005:
        /* 8XY5: VY is subtracted from VX. VF is set to 0 when there's
         * a borrow, and 1 when there isn't. */
        cpu->V[0xf] = (cpu->V[idy] > cpu->V[idx]) ? 0 : 1;
        cpu->V[idx] -= cpu->V[idy];
        break;

    case 0x0006:
        /* 8XY6: shifts VY right by one and stores the result in VX.
         * VF is set to the least significant bit of VY before the shift. */
        cpu->V[0xf] = cpu->V[idy] & 0x1;
        cpu->V[idx] = cpu->V[idy] >> 1;
        break;

    case 0x0007:
        /* 8XY7: sets VX to VY minus VX. VF is set to 0 when there's
         * a borrow, and 1 when there isn't. */
        cpu->V[0xf] = (cpu->V[idx] > cpu->V[idy]) ? 0 : 1;
        cpu->V[idx] = cpu->V[idy] - cpu->V[idx];
        break;

    case 0x000E:
        /* 8XYE: shifts VY left by one and stores the result in VX.
         * VF is set to the most significant bit of VY before the shift. */
        cpu->V[0xf] = (cpu->V[idy] & 0x80) == 0x80 ? 1 : 0;
        cpu->V[idx] = cpu->V[idy] << 1;
        break;

    default:
        break;
    }

    cpu->PC += 2;
}

/** @brief 9XY0: skips the next instruction if VX doesn't equal VY. */
static void opcode_prefix_9(Chip8CPU *cpu)
{
    cpu->PC += cpu->V[decode_x(cpu)] != cpu->V[decode_y(cpu)] ? 4 : 2;
}

/** @brief ANNN: sets I to the address NNN. */
static void opcode_prefix_a(Chip8CPU *cpu)
{
    cpu->I = cpu->opcode & 0x0fff;
    cpu->PC += 2;
}

/** @brief BNNN: jumps to the address NNN plus V0. */
static void opcode_prefix_b(Chip8CPU *cpu)
{
    cpu->PC = (cpu->opcode & 0x0fff) + cpu->V[0];
}

/** @brief CXNN: sets VX to a random number masked by NN. */
static void opcode_prefix_c(Chip8CPU *cpu)
{
    cpu->V[decode_x(cpu)] = decode_nn(cpu) & (rand() % 0xff);
    cpu->PC += 2;
}

/**
 * @brief DXYN: draws an N-byte sprite from memory location I at (VX, VY)
 *        and sets VF when a collision happens.
 */
static void opcode_prefix_d(Chip8CPU *cpu)
{
    uint16_t x = cpu->V[decode_x(cpu)];
    uint16_t y = cpu->V[decode_y(cpu)];

    /* Sprite height */
    uint16_t lines = decode_n(cpu);

    uint8_t spr_byte;
    uint16_t px;
    uint16_t py;

    cpu->V[0xF] = 0;

    for (int i = 0; i < lines; i++) {
        spr_byte = cpu->memory[cpu->I + i];

        py = (y + i) << 6; /* * 64 */

        for (int j = 0; j < 8; j++) {
            px = (x + j);

            if ((spr_byte & (0x80 >> j)) != 0) {
                if (cpu->display[px + py]) {
                    cpu->V[0xf] = 1;
                }

                cpu->display[px + py] ^= true;
            }
        }
    }

    cpu->PC += 2;
}

/** @brief Handles the EX** opcode group (keypad conditional skips). */
static void opcode_prefix_e(Chip8CPU *cpu)
{
    switch (cpu->opcode & 0x00FF) {
    case 0x009E:
        /* EX9E: skips the next instruction if the key stored in VX
         * is pressed. */
        cpu->PC += (cpu->current_key_state[cpu->V[decode_x(cpu)]]) ? 4 : 2;
        break;

    case 0x00A1:
        /* EXA1: skips the next instruction if the key stored in VX
         * isn't pressed. */
        cpu->PC += (!cpu->current_key_state[cpu->V[decode_x(cpu)]]) ? 4 : 2;
        break;

    default:
        break;
    }
}

/** @brief Handles the FX** opcode group (timers, keypad and memory ops). */
static void opcode_prefix_f(Chip8CPU *cpu)
{
    unsigned char idx = decode_x(cpu);

    switch (cpu->opcode & 0x00FF) {
    case 0x0007: /* FX07: sets VX to the value of the delay timer */
        cpu->V[idx] = cpu->delay_timer;
        break;

    case 0x000A: /* FX0A: a key press is awaited, and then stored in VX */
        if (!cpu->wait_for_key) {
            /* Save the current state of each key */
            for (int i = 0; i < KEY_SIZE; i++) {
                cpu->last_key_state[i] = cpu->current_key_state[i];
            }

            /* Start waiting for a key press */
            cpu->wait_for_key = true;
            return;
        } else {
            /* Check if any key changed state */
            for (int i = 0; i < KEY_SIZE; i++) {
                if (!cpu->last_key_state[i] && cpu->current_key_state[i]) {
                    cpu->V[idx] = i;
                    cpu->PC += 2;
                    cpu->wait_for_key = false;
                    return;
                }

                /* Allows detecting a key that was pressed, released
                 * and then pressed again */
                cpu->last_key_state[i] = cpu->current_key_state[i];
            }

            return;
        }

        break;

    case 0x0015: /* FX15: sets the delay timer to VX */
        cpu->delay_timer = cpu->V[idx];
        break;

    case 0x0018: /* FX18: sets the sound timer to VX */
        cpu->sound_timer = cpu->V[idx];
        break;

    case 0x001E: /* FX1E: adds VX to I */
        cpu->V[0xf] = (cpu->I + cpu->V[idx] > 0xfff) ? 1 : 0;
        cpu->I += (uint16_t)cpu->V[idx];
        break;

    case 0x0029:
        /* FX29: sets I to the location of the sprite for the character
         * in VX. Characters 0-F (in hexadecimal) are represented by a
         * 4x5 font. */
        cpu->I = (uint16_t)cpu->V[idx] * 0x5;
        break;

    case 0x0033: {
        /* FX33: stores the binary-coded decimal representation of VX
         * at the addresses I, I plus 1, and I plus 2 */
        uint8_t value = cpu->V[idx];

        cpu->memory[cpu->I] = value / 100;
        value = value % 100;
        cpu->memory[cpu->I + 1] = value / 10;
        cpu->memory[cpu->I + 2] = value % 10;
        break;
    }

    case 0x0055:
        /* FX55: stores V0 to VX in memory starting at address I */
        for (int i = 0; i <= idx; ++i) {
            cpu->memory[cpu->I + i] = cpu->V[i];
        }

        break;

    case 0x0065:
        /* FX65: fills V0 to VX with values from memory starting at
         * address I */
        for (int i = 0; i <= idx; ++i) {
            cpu->V[i] = cpu->memory[cpu->I + i];
        }

        break;

    default:
        break;
    }

    cpu->PC += 2;
}

/** @brief Opcode handler function type. */
typedef void (*opcode_func)(Chip8CPU *);

/** @brief Dispatch table indexed by the most significant opcode nibble. */
static const opcode_func opcode_map[16] = {
    opcode_prefix_0,
    opcode_prefix_1,
    opcode_prefix_2,
    opcode_prefix_3,
    opcode_prefix_4,
    opcode_prefix_5,
    opcode_prefix_6,
    opcode_prefix_7,
    opcode_prefix_8,
    opcode_prefix_9,
    opcode_prefix_a,
    opcode_prefix_b,
    opcode_prefix_c,
    opcode_prefix_d,
    opcode_prefix_e,
    opcode_prefix_f
};

void chip8_cpu_execute(Chip8CPU *cpu)
{
    /* Dispatch to the handler of the current opcode */
    opcode_map[(cpu->opcode & 0xF000) >> 12](cpu);
}

void chip8_cpu_update_timers(Chip8CPU *cpu)
{
    if (cpu->delay_timer != 0) {
        --cpu->delay_timer;
    }

    if (cpu->sound_timer > 0) {
        --cpu->sound_timer;

        if (cpu->sound_timer == 1) {
            printf("BEEP!\n");
        }
    }
}

const bool *chip8_cpu_get_display(const Chip8CPU *cpu)
{
    return cpu->display;
}

void chip8_cpu_set_key(Chip8CPU *cpu, int key, bool state)
{
    cpu->current_key_state[key] = state;
}
