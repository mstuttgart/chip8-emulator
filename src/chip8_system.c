/**
 * @file chip8_system.c
 * @brief Emulation control layer implementation.
 */

#include "chip8_system.h"

void chip8_system_init(Chip8System *sys)
{
    sys->started = false;
    sys->paused = false;
}

bool chip8_system_load_rom(Chip8System *sys, const char *file_name)
{
    return chip8_cpu_load(&sys->cpu, file_name);
}

void chip8_system_start(Chip8System *sys)
{
    if (sys->started) {
        return;
    }

    sys->started = true;
    sys->paused = false;
}

void chip8_system_resume(Chip8System *sys)
{
    if (!sys->paused || !sys->started) {
        return;
    }

    sys->paused = false;
}

void chip8_system_pause(Chip8System *sys)
{
    if (!sys->started) {
        return;
    }

    sys->paused = true;
}

void chip8_system_stop(Chip8System *sys)
{
    sys->started = false;
    sys->paused = false;
}

void chip8_system_soft_reset(Chip8System *sys)
{
    chip8_cpu_soft_reset(&sys->cpu);
}

void chip8_system_hard_reset(Chip8System *sys)
{
    chip8_cpu_hard_reset(&sys->cpu);
    chip8_system_stop(sys);
}

void chip8_system_step(Chip8System *sys)
{
    if (!sys->started || sys->paused) {
        return;
    }

    chip8_cpu_fetch(&sys->cpu);
    chip8_cpu_execute(&sys->cpu);
}

void chip8_system_update_timers(Chip8System *sys)
{
    if (!sys->started || sys->paused) {
        return;
    }

    chip8_cpu_update_timers(&sys->cpu);
}

bool chip8_system_is_paused(const Chip8System *sys)
{
    return sys->paused;
}

bool chip8_system_is_started(const Chip8System *sys)
{
    return sys->started;
}

const bool *chip8_system_get_display(const Chip8System *sys)
{
    return chip8_cpu_get_display(&sys->cpu);
}

void chip8_system_set_key(Chip8System *sys, int key, bool state)
{
    chip8_cpu_set_key(&sys->cpu, key, state);
}
