/**
 * @file phosphor.c
 * @brief CRT phosphor persistence effect implementation.
 */

#include "phosphor.h"

#define PHOSPHOR_PERSISTENCE_MS (PHOSPHOR_HOLD_MS + PHOSPHOR_FADE_MS)

void phosphor_init(Phosphor *ph)
{
    for (int i = 0; i < DISPLAY; i++) {
        ph->off_ms[i] = PHOSPHOR_PERSISTENCE_MS;
    }
}

void phosphor_update(Phosphor *ph, const bool *display, unsigned elapsed_ms)
{
    for (int i = 0; i < DISPLAY; i++) {
        if (display[i]) {
            ph->off_ms[i] = 0;
        } else if (elapsed_ms >= (unsigned)(PHOSPHOR_PERSISTENCE_MS - ph->off_ms[i])) {
            ph->off_ms[i] = PHOSPHOR_PERSISTENCE_MS;
        } else {
            ph->off_ms[i] += elapsed_ms;
        }
    }
}

uint8_t phosphor_alpha(const Phosphor *ph, int index)
{
    uint16_t off = ph->off_ms[index];

    if (off <= PHOSPHOR_HOLD_MS) {
        return 0xFF;
    }

    if (off >= PHOSPHOR_PERSISTENCE_MS) {
        return 0;
    }

    return (uint8_t)(0xFF * (PHOSPHOR_PERSISTENCE_MS - off) / PHOSPHOR_FADE_MS);
}
