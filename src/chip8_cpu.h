/**
 * @file chip8_cpu.h
 * @brief CHIP-8 CPU core: registers, memory, display buffer and opcode
 *        interpreter. Independent of any graphics library.
 *
 * Reference documentation:
 * - CHIP-8 instruction set:
 *   https://multigesture.net/wp-content/uploads/mirror/goldroad/chip8_instruction_set.shtml
 * - Cowgod's CHIP-8 technical reference:
 *   http://devernay.free.fr/hacks/chip8/C8TECH10.HTM
 */

#ifndef CHIP8_CPU_H
#define CHIP8_CPU_H

#include <stdbool.h>
#include <stdint.h>

#define V_SIZE        16    /**< Number of general purpose registers. */
#define STACK_SIZE    16    /**< Call stack depth. */
#define MEMORY_SIZE   4096  /**< Total memory size (4K). */
#define DISPLAY       2048  /**< Display buffer size (64 * 32). */
#define DISPLAY_W     64    /**< Display width in pixels. */
#define DISPLAY_H     32    /**< Display height in pixels. */
#define DISPLAY_EXT_W 128   /**< Extended (SCHIP) display width in pixels. */
#define DISPLAY_EXT_H 64    /**< Extended (SCHIP) display height in pixels. */
#define KEY_SIZE      16    /**< Number of keys in the hexadecimal keypad. */

/**
 * @brief CHIP-8 CPU state: registers, memory, display and keypad.
 */
typedef struct Chip8CPU {
    uint16_t opcode;                  /**< Currently fetched opcode. */
    uint16_t I;                       /**< Index register. */
    uint16_t PC;                      /**< Program counter. */
    uint16_t SP;                      /**< Stack pointer. */
    uint16_t stack[STACK_SIZE];       /**< Call stack. */

    uint8_t V[V_SIZE];                /**< General purpose registers V0-VF. */
    uint8_t memory[MEMORY_SIZE];      /**< System memory. */

    uint8_t delay_timer;              /**< Delay timer register. */
    uint8_t sound_timer;              /**< Sound timer register. */

    bool display[DISPLAY];            /**< Monochrome display buffer. */
    bool extend_display_mode;         /**< SCHIP extended display mode flag. */

    bool current_key_state[KEY_SIZE]; /**< Current state of each keypad key. */
    bool last_key_state[KEY_SIZE];    /**< Previous keypad state, used by FX0A. */
    bool wait_for_key;                /**< True while FX0A waits for a key press. */
} Chip8CPU;

/**
 * @brief Loads a ROM into the CHIP-8 memory, starting at address 0x200.
 *
 * The CPU is hard reset before the ROM is copied into memory.
 *
 * @param cpu CPU instance.
 * @param file_name Path to the ROM file.
 * @return true on success, false otherwise.
 */
bool chip8_cpu_load(Chip8CPU *cpu, const char *file_name);

/**
 * @brief Fetches the next opcode from memory.
 * @param cpu CPU instance.
 */
void chip8_cpu_fetch(Chip8CPU *cpu);

/**
 * @brief Executes the current opcode.
 * @param cpu CPU instance.
 */
void chip8_cpu_execute(Chip8CPU *cpu);

/**
 * @brief Decrements the delay and sound timers by one tick.
 *
 * On the original hardware the timers decrement at 60 Hz, independently of
 * the CPU speed, so this must be called at that rate rather than once per
 * executed instruction.
 *
 * @param cpu CPU instance.
 */
void chip8_cpu_update_timers(Chip8CPU *cpu);

/**
 * @brief Resets the whole machine: registers, memory and fontset.
 * @param cpu CPU instance.
 */
void chip8_cpu_hard_reset(Chip8CPU *cpu);

/**
 * @brief Resets registers, stack, display and timers, keeping the memory.
 * @param cpu CPU instance.
 */
void chip8_cpu_soft_reset(Chip8CPU *cpu);

/**
 * @brief Returns the display buffer (DISPLAY_W * DISPLAY_H pixels).
 * @param cpu CPU instance.
 * @return Pointer to the display buffer.
 */
const bool *chip8_cpu_get_display(const Chip8CPU *cpu);

/**
 * @brief Updates the state of a keypad key (0x0 - 0xF).
 * @param cpu CPU instance.
 * @param key Key index.
 * @param state true if pressed, false if released.
 */
void chip8_cpu_set_key(Chip8CPU *cpu, int key, bool state);

#endif /* CHIP8_CPU_H */
