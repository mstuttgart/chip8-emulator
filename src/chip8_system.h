/**
 * @file chip8_system.h
 * @brief Emulation control layer, independent of any graphics frontend.
 *
 * The frontend is responsible for calling chip8_system_step() every
 * CHIP8_STEP_INTERVAL_MS, calling chip8_system_update_timers() every
 * CHIP8_TIMER_INTERVAL_MS, rendering the display returned by
 * chip8_system_get_display() and forwarding keyboard input through
 * chip8_system_set_key().
 */

#ifndef CHIP8_SYSTEM_H
#define CHIP8_SYSTEM_H

#include "chip8_cpu.h"

#define CHIP8_STEP_INTERVAL_MS  2  /**< Interval between CPU cycles (500 Hz), in milliseconds. */
#define CHIP8_TIMER_INTERVAL_MS 16 /**< Interval between timer ticks (~60 Hz), in milliseconds. */

/**
 * @brief Emulation state: CPU plus execution control flags.
 */
typedef struct Chip8System {
    Chip8CPU cpu;  /**< CHIP-8 CPU instance. */
    bool started;  /**< True while the emulation is running. */
    bool paused;   /**< True while the emulation is paused. */
} Chip8System;

/**
 * @brief Initializes the system with the emulation stopped.
 * @param sys System instance.
 */
void chip8_system_init(Chip8System *sys);

/**
 * @brief Loads a ROM and resets the CPU.
 * @param sys System instance.
 * @param file_name Path to the ROM file.
 * @return true on success, false otherwise.
 */
bool chip8_system_load_rom(Chip8System *sys, const char *file_name);

/**
 * @brief Starts the emulation.
 * @param sys System instance.
 */
void chip8_system_start(Chip8System *sys);

/**
 * @brief Resumes the emulation after a pause.
 * @param sys System instance.
 */
void chip8_system_resume(Chip8System *sys);

/**
 * @brief Pauses the emulation.
 * @param sys System instance.
 */
void chip8_system_pause(Chip8System *sys);

/**
 * @brief Stops the emulation.
 * @param sys System instance.
 */
void chip8_system_stop(Chip8System *sys);

/**
 * @brief Resets registers and display only, keeping the loaded ROM.
 * @param sys System instance.
 */
void chip8_system_soft_reset(Chip8System *sys);

/**
 * @brief Resets the whole machine and stops the emulation.
 * @param sys System instance.
 */
void chip8_system_hard_reset(Chip8System *sys);

/**
 * @brief Runs one CPU cycle (fetch + execute) if the emulation is active.
 * @param sys System instance.
 */
void chip8_system_step(Chip8System *sys);

/**
 * @brief Decrements the delay and sound timers if the emulation is active.
 *
 * Must be called at ~60 Hz (every CHIP8_TIMER_INTERVAL_MS), independently
 * of the CPU cycle rate.
 *
 * @param sys System instance.
 */
void chip8_system_update_timers(Chip8System *sys);

/**
 * @brief Checks whether the emulation is paused.
 * @param sys System instance.
 * @return true if paused.
 */
bool chip8_system_is_paused(const Chip8System *sys);

/**
 * @brief Checks whether the emulation has been started.
 * @param sys System instance.
 * @return true if started.
 */
bool chip8_system_is_started(const Chip8System *sys);

/**
 * @brief Returns the display buffer (DISPLAY_W * DISPLAY_H pixels).
 * @param sys System instance.
 * @return Pointer to the display buffer.
 */
const bool *chip8_system_get_display(const Chip8System *sys);

/**
 * @brief Updates the state of a CHIP-8 keypad key (0x0 - 0xF).
 * @param sys System instance.
 * @param key Key index.
 * @param state true if pressed, false if released.
 */
void chip8_system_set_key(Chip8System *sys, int key, bool state);

#endif /* CHIP8_SYSTEM_H */
