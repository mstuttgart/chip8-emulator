/**
 * @file phosphor.h
 * @brief CRT phosphor persistence effect, shared by the display frontends.
 *
 * CHIP-8 games move sprites by erasing and redrawing them (DXYN draws with
 * XOR), so the display buffer briefly holds an "erased" state. Presenting a
 * frame at that exact moment makes sprites flicker. To avoid it, a pixel
 * that turns off keeps full brightness for PHOSPHOR_HOLD_MS (long enough to
 * bridge the erase/redraw gap of sprites that stay in place, like the PONG
 * paddles) and then fades out quickly over PHOSPHOR_FADE_MS (short enough
 * that moving sprites, like the PONG ball, leave no visible trail).
 */

#ifndef PHOSPHOR_H
#define PHOSPHOR_H

#include "chip8_cpu.h"

#define PHOSPHOR_HOLD_MS 70 /**< Full-brightness hold time after a pixel turns off. */
#define PHOSPHOR_FADE_MS 40 /**< Fade-out duration after the hold period. */

/**
 * @brief Per-pixel phosphor persistence state.
 */
typedef struct Phosphor {
    /** Time each pixel has been off, in milliseconds, saturated at
     *  PHOSPHOR_HOLD_MS + PHOSPHOR_FADE_MS. */
    uint16_t off_ms[DISPLAY];
} Phosphor;

/**
 * @brief Initializes the phosphor state with all pixels fully faded out.
 * @param ph Phosphor instance.
 */
void phosphor_init(Phosphor *ph);

/**
 * @brief Updates the phosphor state from the current display buffer.
 * @param ph Phosphor instance.
 * @param display Display buffer with DISPLAY pixels.
 * @param elapsed_ms Time elapsed since the last update, in milliseconds.
 */
void phosphor_update(Phosphor *ph, const bool *display, unsigned elapsed_ms);

/**
 * @brief Returns the brightness of a pixel.
 * @param ph Phosphor instance.
 * @param index Pixel index in the display buffer.
 * @return Brightness from 0 (off) to 255 (full).
 */
uint8_t phosphor_alpha(const Phosphor *ph, int index);

#endif /* PHOSPHOR_H */
