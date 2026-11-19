/**
 * @file main.c
 * @brief SDL2 frontend of the CHIP8 Emulator.
 *
 * All the emulation logic lives in chip8_cpu.c / chip8_system.c, which do
 * not depend on any graphics library. This file only renders the display
 * and forwards keyboard input to the emulator.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL.h>

#include "chip8_system.h"
#include "phosphor.h"

#define DEFAULT_PIXEL_SIZE 8 /**< Default size of a CHIP-8 pixel, in screen pixels. */

/**
 * @brief Color theme: background and pixel colors.
 */
typedef struct Theme {
    const char *name;     /**< Theme display name. */
    SDL_Color background; /**< Background color. */
    SDL_Color pixel;      /**< Pixel (foreground) color. */
} Theme;

/** @brief Available color themes, selectable with the F1-F5 keys. */
static const Theme themes[] = {
    {"Default",       {0x21, 0x24, 0x35, 0xff}, {0xdd, 0xff, 0x55, 0xff}},
    {"TELMAC/COSMAP", {0x00, 0x00, 0x00, 0xff}, {0xff, 0xff, 0xff, 0xff}},
    {"HP48",          {0xca, 0xdd, 0x5c, 0xff}, {0x3c, 0x41, 0x76, 0xff}},
    {"YellowBlues",   {0x3c, 0x41, 0x76, 0xff}, {0xca, 0xdd, 0x5c, 0xff}},
    {"LaraG",         {0xd2, 0xd4, 0xe0, 0xff}, {0xb8, 0x41, 0xa3, 0xff}}
};

/**
 * @brief Maps an SDL scancode to a CHIP-8 keypad key.
 *
 * CHIP-8 keypad to PC keyboard mapping:
 *
 *     chip8            PC
 *   1 2 3 C         1 2 3 4
 *   4 5 6 D   <->   Q W E R
 *   7 8 9 E         A S D F
 *   A 0 B F         Z X C V
 *
 * @param scancode SDL scancode.
 * @return Keypad key index (0x0 - 0xF), or -1 if the key is not mapped.
 */
static int map_scancode_to_key(SDL_Scancode scancode)
{
    switch (scancode) {
    case SDL_SCANCODE_1: return 0x1;
    case SDL_SCANCODE_2: return 0x2;
    case SDL_SCANCODE_3: return 0x3;
    case SDL_SCANCODE_4: return 0xC;

    case SDL_SCANCODE_Q: return 0x4;
    case SDL_SCANCODE_W: return 0x5;
    case SDL_SCANCODE_E: return 0x6;
    case SDL_SCANCODE_R: return 0xD;

    case SDL_SCANCODE_A: return 0x7;
    case SDL_SCANCODE_S: return 0x8;
    case SDL_SCANCODE_D: return 0x9;
    case SDL_SCANCODE_F: return 0xE;

    case SDL_SCANCODE_Z: return 0xA;
    case SDL_SCANCODE_X: return 0x0;
    case SDL_SCANCODE_C: return 0xB;
    case SDL_SCANCODE_V: return 0xF;

    default: return -1;
    }
}

/**
 * @brief Resizes the window to fit the display with the given pixel size.
 * @param window SDL window.
 * @param pixel_size Size of a CHIP-8 pixel, in screen pixels.
 */
static void resize_window(SDL_Window *window, int pixel_size)
{
    SDL_SetWindowSize(window, DISPLAY_W * pixel_size, DISPLAY_H * pixel_size);
}

/**
 * @brief Renders the CHIP-8 display with the given theme.
 *
 * Applies the phosphor persistence effect (see phosphor.h) to avoid the
 * flicker caused by the XOR-based sprite drawing of CHIP-8 games.
 *
 * @param renderer SDL renderer.
 * @param sys System instance.
 * @param theme Active color theme.
 */
static void render_display(SDL_Renderer *renderer, const Chip8System *sys,
                           const Theme *theme)
{
    static Phosphor phosphor;
    static Uint64 last_render;

    /* Time elapsed since the last rendered frame */
    Uint64 now = SDL_GetTicks64();
    unsigned elapsed = last_render ? (unsigned)(now - last_render)
                                   : PHOSPHOR_HOLD_MS + PHOSPHOR_FADE_MS;
    last_render = now;

    phosphor_update(&phosphor, chip8_system_get_display(sys), elapsed);

    SDL_SetRenderDrawColor(renderer, theme->background.r, theme->background.g,
                           theme->background.b, theme->background.a);
    SDL_RenderClear(renderer);

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    int w;
    int h;
    SDL_GetRendererOutputSize(renderer, &w, &h);

    /* Pixel size derived from the current window size */
    float pixel_w = (float)w / DISPLAY_W;
    float pixel_h = (float)h / DISPLAY_H;

    for (int y = 0; y < DISPLAY_H; y++) {
        int mult = y << 6; /* * 64 */

        for (int x = 0; x < DISPLAY_W; x++) {
            uint8_t alpha = phosphor_alpha(&phosphor, mult + x);

            if (alpha > 0) {
                SDL_FRect rect = {x * pixel_w, y * pixel_h, pixel_w, pixel_h};
                SDL_SetRenderDrawColor(renderer, theme->pixel.r, theme->pixel.g,
                                       theme->pixel.b, alpha);
                SDL_RenderFillRectF(renderer, &rect);
            }
        }
    }

    SDL_RenderPresent(renderer);
}

/**
 * @brief Prints the command line usage and the key bindings.
 * @param prog Program name (argv[0]).
 */
static void print_usage(const char *prog)
{
    printf("Usage: %s <rom-file>\n\n", prog);
    printf("Keys:\n");
    printf("  1 2 3 4 / Q W E R / A S D F / Z X C V : chip8 keypad\n");
    printf("  Space  : start emulation (after hard reset)\n");
    printf("  P      : pause/resume\n");
    printf("  Ctrl+U : soft reset\n");
    printf("  Ctrl+R : hard reset (reload ROM)\n");
    printf("  Ctrl+1 / Ctrl+2 / Ctrl+3 : window size x0.5 / x1 / x2\n");
    printf("  F1 - F5 : color themes\n");
    printf("  Ctrl+Q or Esc : quit\n");
}

int main(int argc, char *argv[])
{
    if (argc < 2) {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    const char *rom_file = argv[1];

    Chip8System chip8_system;
    chip8_system_init(&chip8_system);

    if (!chip8_system_load_rom(&chip8_system, rom_file)) {
        printf("ROM loaded fail!\n");
        return EXIT_FAILURE;
    }

    printf("%s ROM loaded with success!\n", rom_file);

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        printf("ERROR: SDL_Init: %s\n", SDL_GetError());
        return EXIT_FAILURE;
    }

    SDL_Window *window =
        SDL_CreateWindow("CHIP8 Emulator",
                         SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                         DISPLAY_W * DEFAULT_PIXEL_SIZE,
                         DISPLAY_H * DEFAULT_PIXEL_SIZE,
                         SDL_WINDOW_SHOWN);

    if (!window) {
        printf("ERROR: SDL_CreateWindow: %s\n", SDL_GetError());
        SDL_Quit();
        return EXIT_FAILURE;
    }

    SDL_Renderer *renderer =
        SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED |
                           SDL_RENDERER_PRESENTVSYNC);

    if (!renderer) {
        renderer = SDL_CreateRenderer(window, -1, 0);
    }

    if (!renderer) {
        printf("ERROR: SDL_CreateRenderer: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return EXIT_FAILURE;
    }

    const Theme *theme = &themes[0];

    chip8_system_start(&chip8_system);

    bool quit = false;
    Uint64 last_step = SDL_GetTicks64();
    Uint64 last_timer_tick = last_step;

    while (!quit) {
        SDL_Event event;

        while (SDL_PollEvent(&event)) {
            switch (event.type) {
            case SDL_QUIT:
                quit = true;
                break;

            case SDL_KEYDOWN: {
                SDL_Keymod mod = SDL_GetModState();
                SDL_Scancode code = event.key.keysym.scancode;
                bool ctrl = (mod & KMOD_CTRL) != 0;

                if (ctrl) {
                    switch (code) {
                    case SDL_SCANCODE_Q:
                        quit = true;
                        break;

                    case SDL_SCANCODE_U: /* soft reset */
                        chip8_system_soft_reset(&chip8_system);
                        break;

                    case SDL_SCANCODE_R: /* hard reset: reload the ROM */
                        chip8_system_hard_reset(&chip8_system);
                        chip8_system_load_rom(&chip8_system, rom_file);
                        break;

                    case SDL_SCANCODE_1:
                        resize_window(window, DEFAULT_PIXEL_SIZE / 2);
                        break;

                    case SDL_SCANCODE_2:
                        resize_window(window, DEFAULT_PIXEL_SIZE);
                        break;

                    case SDL_SCANCODE_3:
                        resize_window(window, DEFAULT_PIXEL_SIZE * 2);
                        break;

                    default:
                        break;
                    }

                    break;
                }

                switch (code) {
                case SDL_SCANCODE_ESCAPE:
                    quit = true;
                    break;

                case SDL_SCANCODE_SPACE:
                    if (!event.key.repeat) {
                        chip8_system_start(&chip8_system);
                    }

                    break;

                case SDL_SCANCODE_P:
                    if (!event.key.repeat) {
                        if (chip8_system_is_paused(&chip8_system)) {
                            chip8_system_resume(&chip8_system);
                        } else {
                            chip8_system_pause(&chip8_system);
                        }
                    }

                    break;

                case SDL_SCANCODE_F1:
                case SDL_SCANCODE_F2:
                case SDL_SCANCODE_F3:
                case SDL_SCANCODE_F4:
                case SDL_SCANCODE_F5:
                    theme = &themes[code - SDL_SCANCODE_F1];
                    break;

                default: {
                    int key = map_scancode_to_key(code);

                    if (key >= 0) {
                        chip8_system_set_key(&chip8_system, key, true);
                    }

                    break;
                }
                }

                break;
            }

            case SDL_KEYUP: {
                int key = map_scancode_to_key(event.key.keysym.scancode);

                if (key >= 0) {
                    chip8_system_set_key(&chip8_system, key, false);
                }

                break;
            }

            default:
                break;
            }
        }

        /* Run one CPU cycle every CHIP8_STEP_INTERVAL_MS */
        Uint64 now = SDL_GetTicks64();

        while (now - last_step >= CHIP8_STEP_INTERVAL_MS) {
            chip8_system_step(&chip8_system);
            last_step += CHIP8_STEP_INTERVAL_MS;
        }

        /* Decrement the delay and sound timers at ~60 Hz */
        while (now - last_timer_tick >= CHIP8_TIMER_INTERVAL_MS) {
            chip8_system_update_timers(&chip8_system);
            last_timer_tick += CHIP8_TIMER_INTERVAL_MS;
        }

        render_display(renderer, &chip8_system, theme);

        SDL_Delay(1);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return EXIT_SUCCESS;
}
