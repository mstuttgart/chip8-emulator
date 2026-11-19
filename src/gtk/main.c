/**
 * @file main.c
 * @brief GTK4 frontend of the CHIP8 Emulator.
 *
 * All the emulation logic lives in chip8_cpu.c / chip8_system.c, which do
 * not depend on any graphics library. This frontend provides a full GUI:
 * menu bar, native file chooser, color themes and window scaling, so the
 * emulator does not need to be launched from the command line.
 */

#include <stdlib.h>

#include <gtk/gtk.h>

#include "chip8_system.h"
#include "phosphor.h"

#define DEFAULT_PIXEL_SIZE 8 /**< Default size of a CHIP-8 pixel, in screen pixels. */
#define MIN_PIXEL_SIZE     4 /**< Minimum size of a CHIP-8 pixel (x0.5 scale). */

/** @brief Converts an 8-bit color channel to the 0.0 - 1.0 range. */
#define RGB(r, g, b) {(r) / 255.0, (g) / 255.0, (b) / 255.0}

/**
 * @brief Color theme: background and pixel colors.
 */
typedef struct Theme {
    const char *name; /**< Theme display name. */
    double bg[3];     /**< Background color (RGB, 0.0 - 1.0). */
    double px[3];     /**< Pixel (foreground) color (RGB, 0.0 - 1.0). */
} Theme;

/** @brief Available color themes, selectable from the View menu or F1-F5. */
static const Theme themes[] = {
    {"Default",       RGB(0x21, 0x24, 0x35), RGB(0xdd, 0xff, 0x55)},
    {"TELMAC/COSMAP", RGB(0x00, 0x00, 0x00), RGB(0xff, 0xff, 0xff)},
    {"HP48",          RGB(0xca, 0xdd, 0x5c), RGB(0x3c, 0x41, 0x76)},
    {"YellowBlues",   RGB(0x3c, 0x41, 0x76), RGB(0xca, 0xdd, 0x5c)},
    {"LaraG",         RGB(0xd2, 0xd4, 0xe0), RGB(0xb8, 0x41, 0xa3)}
};

/**
 * @brief Application state: emulator, rendering and GTK widgets.
 */
typedef struct EmuApp {
    GtkApplication *app;     /**< GTK application. */
    GtkWidget *window;       /**< Main window. */
    GtkWidget *drawing_area; /**< CHIP-8 display widget. */
    cairo_surface_t *splash; /**< Splash image shown when no ROM is loaded. */

    Chip8System system;      /**< Emulator instance. */
    Phosphor phosphor;       /**< Phosphor persistence state. */

    char *rom_path;          /**< Path of the loaded ROM, or NULL. */
    int theme_index;         /**< Active color theme. */

    gint64 last_step_us;     /**< Timestamp of the last CPU cycle, in microseconds. */
    gint64 last_timer_us;    /**< Timestamp of the last timer tick, in microseconds. */
    gint64 last_render_us;   /**< Timestamp of the last rendered frame, in microseconds. */
} EmuApp;

/**
 * @brief Resolves an application image next to the executable.
 *
 * Build outputs are placed in the repository root, where the assets/
 * directory is located. This resolution is independent of the process
 * working directory, so the dialog assets also work when launched from a
 * desktop entry.
 *
 * @param file_name Image file name.
 * @return Newly allocated path, or NULL on error. Free with g_free().
 */
static char *image_path(const char *file_name)
{
    char *executable = g_file_read_link("/proc/self/exe", NULL);

    if (!executable) {
        return g_build_filename("assets", file_name, NULL);
    }

    char *directory = g_path_get_dirname(executable);
    char *path = g_build_filename(directory, "assets", file_name, NULL);

    g_free(directory);
    g_free(executable);

    return path;
}

/**
 * @brief Applies application-specific styling and registers the image icon.
 * @param emu Application state.
 */
static void configure_style_and_icons(EmuApp *emu)
{
    GdkDisplay *display = gtk_widget_get_display(emu->window);
    GtkCssProvider *provider = gtk_css_provider_new();
    const char *css =
        "window,"
        "window.background,"
        "window.background.csd,"
        "window.background.solid-csd {"
        "  border-radius: 0;"
        "}"
        "window > box,"
        "window > box > menubar,"
        "menubar {"
        "  border: 0;"
        "  border-radius: 0 0 0 0;"
        "  border-top-left-radius: 0;"
        "  border-top-right-radius: 0;"
        "  box-shadow: none;"
        "}"
        "menubar > item {"
        "  border: 0;"
        "  border-radius: 0;"
        "  box-shadow: none;"
        "}"
        "menubar > item:hover,"
        "menubar > item:selected {"
        "  border-radius: 0;"
        "}";

    gtk_css_provider_load_from_string(provider, css);
    gtk_style_context_add_provider_for_display(
        display, GTK_STYLE_PROVIDER(provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(provider);

    char *icon_file = image_path("icon.png");
    char *icon_directory = g_path_get_dirname(icon_file);
    GtkIconTheme *icon_theme = gtk_icon_theme_get_for_display(display);

    gtk_icon_theme_add_search_path(icon_theme, icon_directory);
    gtk_window_set_icon_name(GTK_WINDOW(emu->window), "icon");

    g_free(icon_directory);
    g_free(icon_file);
}

/**
 * @brief Maps a GDK keyval to a CHIP-8 keypad key.
 *
 * CHIP-8 keypad to PC keyboard mapping:
 *
 *     chip8            PC
 *   1 2 3 C         1 2 3 4
 *   4 5 6 D   <->   Q W E R
 *   7 8 9 E         A S D F
 *   A 0 B F         Z X C V
 *
 * @param keyval GDK keyval.
 * @return Keypad key index (0x0 - 0xF), or -1 if the key is not mapped.
 */
static int map_keyval_to_key(guint keyval)
{
    switch (gdk_keyval_to_lower(keyval)) {
    case GDK_KEY_1: return 0x1;
    case GDK_KEY_2: return 0x2;
    case GDK_KEY_3: return 0x3;
    case GDK_KEY_4: return 0xC;

    case GDK_KEY_q: return 0x4;
    case GDK_KEY_w: return 0x5;
    case GDK_KEY_e: return 0x6;
    case GDK_KEY_r: return 0xD;

    case GDK_KEY_a: return 0x7;
    case GDK_KEY_s: return 0x8;
    case GDK_KEY_d: return 0x9;
    case GDK_KEY_f: return 0xE;

    case GDK_KEY_z: return 0xA;
    case GDK_KEY_x: return 0x0;
    case GDK_KEY_c: return 0xB;
    case GDK_KEY_v: return 0xF;

    default: return -1;
    }
}

/**
 * @brief Loads a ROM, updates the window title and starts the emulation.
 * @param emu Application state.
 * @param path Path to the ROM file.
 */
static void load_rom(EmuApp *emu, const char *path)
{
    chip8_system_stop(&emu->system);

    if (!chip8_system_load_rom(&emu->system, path)) {
        g_printerr("Failed to load ROM: %s\n", path);
        return;
    }

    g_free(emu->rom_path);
    emu->rom_path = g_strdup(path);

    char *base = g_path_get_basename(path);
    gtk_window_set_title(GTK_WINDOW(emu->window), base);
    g_free(base);

    /* Reset the timing accumulators to avoid a catch-up burst */
    emu->last_step_us = 0;
    emu->last_timer_us = 0;

    chip8_system_start(&emu->system);
}

/**
 * @brief Tick callback: runs CPU cycles and timer ticks, then redraws.
 */
static gboolean tick_cb(GtkWidget *widget, GdkFrameClock *clock, gpointer data)
{
    EmuApp *emu = data;
    gint64 now = gdk_frame_clock_get_frame_time(clock);

    if (emu->last_step_us == 0) {
        emu->last_step_us = now;
        emu->last_timer_us = now;
    }

    /* Clamp the catch-up window to avoid bursts after long stalls */
    if (now - emu->last_step_us > 100000) {
        emu->last_step_us = now - 100000;
    }

    if (now - emu->last_timer_us > 100000) {
        emu->last_timer_us = now - 100000;
    }

    /* Run one CPU cycle every CHIP8_STEP_INTERVAL_MS */
    while (now - emu->last_step_us >= CHIP8_STEP_INTERVAL_MS * 1000) {
        chip8_system_step(&emu->system);
        emu->last_step_us += CHIP8_STEP_INTERVAL_MS * 1000;
    }

    /* Decrement the delay and sound timers at ~60 Hz */
    while (now - emu->last_timer_us >= CHIP8_TIMER_INTERVAL_MS * 1000) {
        chip8_system_update_timers(&emu->system);
        emu->last_timer_us += CHIP8_TIMER_INTERVAL_MS * 1000;
    }

    gtk_widget_queue_draw(widget);

    return G_SOURCE_CONTINUE;
}

/**
 * @brief Draw callback: renders the CHIP-8 display with the active theme.
 *
 * Applies the phosphor persistence effect (see phosphor.h) to avoid the
 * flicker caused by the XOR-based sprite drawing of CHIP-8 games.
 */
static void draw_cb(GtkDrawingArea *area, cairo_t *cr, int width, int height,
                    gpointer data)
{
    (void)area;

    EmuApp *emu = data;
    const Theme *theme = &themes[emu->theme_index];

    /* Time elapsed since the last rendered frame */
    gint64 now = g_get_monotonic_time();
    unsigned elapsed = emu->last_render_us
                           ? (unsigned)((now - emu->last_render_us) / 1000)
                           : PHOSPHOR_HOLD_MS + PHOSPHOR_FADE_MS;
    emu->last_render_us = now;

    phosphor_update(&emu->phosphor, chip8_system_get_display(&emu->system),
                    elapsed);

    cairo_set_source_rgb(cr, theme->bg[0], theme->bg[1], theme->bg[2]);
    cairo_paint(cr);

    if (!emu->rom_path) {
        if (emu->splash) {
            double image_width = cairo_image_surface_get_width(emu->splash);
            double image_height = cairo_image_surface_get_height(emu->splash);
            double x = (width - image_width) / 2.0;
            double y = (height - image_height) / 2.0;

            cairo_set_source_surface(cr, emu->splash, x, y);
            cairo_paint(cr);
        }

        return;
    }

    /* Pixel size derived from the current widget size */
    double pixel_w = (double)width / DISPLAY_W;
    double pixel_h = (double)height / DISPLAY_H;

    for (int y = 0; y < DISPLAY_H; y++) {
        int mult = y << 6; /* * 64 */

        for (int x = 0; x < DISPLAY_W; x++) {
            uint8_t alpha = phosphor_alpha(&emu->phosphor, mult + x);

            if (alpha > 0) {
                cairo_set_source_rgba(cr, theme->px[0], theme->px[1],
                                      theme->px[2], alpha / 255.0);
                cairo_rectangle(cr, x * pixel_w, y * pixel_h, pixel_w, pixel_h);
                cairo_fill(cr);
            }
        }
    }
}

/**
 * @brief Keyboard press handler: keypad, start (Space) and pause (P).
 */
static gboolean on_key_pressed(GtkEventControllerKey *controller, guint keyval,
                               guint keycode, GdkModifierType state,
                               gpointer data)
{
    (void)controller;
    (void)keycode;

    EmuApp *emu = data;

    /* Let menu accelerators handle Ctrl shortcuts */
    if (state & GDK_CONTROL_MASK) {
        return FALSE;
    }

    switch (gdk_keyval_to_lower(keyval)) {
    case GDK_KEY_space:
        chip8_system_start(&emu->system);
        return TRUE;

    case GDK_KEY_p:
        if (chip8_system_is_paused(&emu->system)) {
            chip8_system_resume(&emu->system);
        } else {
            chip8_system_pause(&emu->system);
        }

        return TRUE;

    default: {
        int key = map_keyval_to_key(keyval);

        if (key >= 0) {
            chip8_system_set_key(&emu->system, key, true);
            return TRUE;
        }

        return FALSE;
    }
    }
}

/**
 * @brief Keyboard release handler for the CHIP-8 keypad.
 */
static void on_key_released(GtkEventControllerKey *controller, guint keyval,
                            guint keycode, GdkModifierType state, gpointer data)
{
    (void)controller;
    (void)keycode;
    (void)state;

    EmuApp *emu = data;
    int key = map_keyval_to_key(keyval);

    if (key >= 0) {
        chip8_system_set_key(&emu->system, key, false);
    }
}

#if GTK_CHECK_VERSION(4, 10, 0)

/** @brief Completion callback of the GTK >= 4.10 file dialog. */
static void on_open_ready(GObject *source, GAsyncResult *result, gpointer data)
{
    EmuApp *emu = data;
    GFile *file = gtk_file_dialog_open_finish(GTK_FILE_DIALOG(source), result,
                                              NULL);

    if (file) {
        char *path = g_file_get_path(file);

        if (path) {
            load_rom(emu, path);
            g_free(path);
        }

        g_object_unref(file);
    }
}

#else

/** @brief Response callback of the GTK < 4.10 native file chooser. */
static void on_native_response(GtkNativeDialog *native, int response,
                               gpointer data)
{
    EmuApp *emu = data;

    if (response == GTK_RESPONSE_ACCEPT) {
        GFile *file = gtk_file_chooser_get_file(GTK_FILE_CHOOSER(native));
        char *path = g_file_get_path(file);

        if (path) {
            load_rom(emu, path);
            g_free(path);
        }

        g_object_unref(file);
    }

    g_object_unref(native);
}

#endif

/** @brief File > Open ROM image: shows the native file chooser. */
static void action_open(GSimpleAction *action, GVariant *param, gpointer data)
{
    (void)action;
    (void)param;

    EmuApp *emu = data;

#if GTK_CHECK_VERSION(4, 10, 0)
    GtkFileDialog *dialog = gtk_file_dialog_new();

    gtk_file_dialog_set_title(dialog, "Open ROM image");
    gtk_file_dialog_open(dialog, GTK_WINDOW(emu->window), NULL, on_open_ready,
                         emu);
    g_object_unref(dialog);
#else
    GtkFileChooserNative *native =
        gtk_file_chooser_native_new("Open ROM image", GTK_WINDOW(emu->window),
                                    GTK_FILE_CHOOSER_ACTION_OPEN, "_Open",
                                    "_Cancel");

    g_signal_connect(native, "response", G_CALLBACK(on_native_response), emu);
    gtk_native_dialog_show(GTK_NATIVE_DIALOG(native));
#endif
}

/** @brief File > Quit. */
static void action_quit(GSimpleAction *action, GVariant *param, gpointer data)
{
    (void)action;
    (void)param;

    EmuApp *emu = data;
    g_application_quit(G_APPLICATION(emu->app));
}

/** @brief Emulation > Run/Pause. */
static void action_pause(GSimpleAction *action, GVariant *param, gpointer data)
{
    (void)action;
    (void)param;

    EmuApp *emu = data;

    if (chip8_system_is_paused(&emu->system)) {
        chip8_system_resume(&emu->system);
    } else {
        chip8_system_pause(&emu->system);
    }
}

/** @brief Emulation > Soft Reset: resets registers, keeping the ROM. */
static void action_soft_reset(GSimpleAction *action, GVariant *param,
                              gpointer data)
{
    (void)action;
    (void)param;

    EmuApp *emu = data;
    chip8_system_soft_reset(&emu->system);
}

/** @brief Emulation > Hard Reset: reloads the ROM and restarts. */
static void action_hard_reset(GSimpleAction *action, GVariant *param,
                              gpointer data)
{
    (void)action;
    (void)param;

    EmuApp *emu = data;

    chip8_system_hard_reset(&emu->system);

    if (emu->rom_path) {
        char *path = g_strdup(emu->rom_path);
        load_rom(emu, path);
        g_free(path);
    }
}

/** @brief View > Theme: radio action with the theme index as a string. */
static void action_theme(GSimpleAction *action, GVariant *param, gpointer data)
{
    EmuApp *emu = data;

    emu->theme_index = atoi(g_variant_get_string(param, NULL));
    g_simple_action_set_state(action, param);
}

/** @brief View > Window size: radio action with the pixel size as a string. */
static void action_scale(GSimpleAction *action, GVariant *param, gpointer data)
{
    EmuApp *emu = data;
    int pixel_size = atoi(g_variant_get_string(param, NULL));

    gtk_window_set_default_size(GTK_WINDOW(emu->window),
                                DISPLAY_W * pixel_size,
                                DISPLAY_H * pixel_size);
    g_simple_action_set_state(action, param);
}

/** @brief Help > About. */
static void action_about(GSimpleAction *action, GVariant *param, gpointer data)
{
    (void)action;
    (void)param;

    EmuApp *emu = data;
    const char *authors[] = {"Michell Stuttgart", NULL};
    GError *error = NULL;
    char *logo_path = image_path("space.png");
    GdkPixbuf *logo_image =
        gdk_pixbuf_new_from_file_at_scale(logo_path, 292, 240, TRUE, &error);
    GdkTexture *logo =
        logo_image ? gdk_texture_new_for_pixbuf(logo_image) : NULL;

    if (error) {
        g_printerr("Failed to load About logo: %s\n", error->message);
        g_clear_error(&error);
    }

    gtk_show_about_dialog(GTK_WINDOW(emu->window),
                          "program-name", "CHIP8 Emulator",
                          "logo", logo,
                          "icon-name", "icon",
                          "comments",
                          "A free and open source RCA COSMAC VIP emulator "
                          "and CHIP-8 interpreter for GNU/Linux.",
                          "copyright",
                          "Copyright (C) 2014-2016 Michell Stuttgart Faria\n"
                          "Internal RCA COSMAC VIP Operating System, CHIP-8 "
                          "interpreters (C) Joseph Weisbecker 1977-1990(?)",
                          "authors", authors,
                          NULL);

    g_clear_object(&logo);
    g_clear_object(&logo_image);
    g_free(logo_path);
}

static const GActionEntry app_actions[] = {
    {.name = "open", .activate = action_open},
    {.name = "quit", .activate = action_quit},
    {.name = "pause", .activate = action_pause},
    {.name = "soft-reset", .activate = action_soft_reset},
    {.name = "hard-reset", .activate = action_hard_reset},
    {.name = "about", .activate = action_about},
    {.name = "theme", .activate = action_theme,
     .parameter_type = "s", .state = "'0'"},
    {.name = "scale", .activate = action_scale,
     .parameter_type = "s", .state = "'8'"},
};

/** @brief Registers an accelerator for a (detailed) action name. */
static void set_accel(GtkApplication *app, const char *action,
                      const char *accel)
{
    const char *accels[] = {accel, NULL};
    gtk_application_set_accels_for_action(app, action, accels);
}

/** @brief Appends a radio menu item bound to a string-target action. */
static void append_radio_item(GMenu *menu, const char *label,
                              const char *action, const char *target)
{
    GMenuItem *item = g_menu_item_new(label, NULL);

    g_menu_item_set_action_and_target_value(item, action,
                                            g_variant_new_string(target));
    g_menu_append_item(menu, item);
    g_object_unref(item);
}

/** @brief Builds the menu bar: File, Emulation, View and Help. */
static GMenuModel *build_menubar(void)
{
    GMenu *menubar = g_menu_new();

    GMenu *file_menu = g_menu_new();
    g_menu_append(file_menu, "_Open ROM image…", "app.open");
    g_menu_append(file_menu, "_Quit", "app.quit");
    g_menu_append_submenu(menubar, "_File", G_MENU_MODEL(file_menu));
    g_object_unref(file_menu);

    GMenu *emulation_menu = g_menu_new();
    g_menu_append(emulation_menu, "_Run/Pause", "app.pause");
    g_menu_append(emulation_menu, "_Soft Reset", "app.soft-reset");
    g_menu_append(emulation_menu, "_Hard Reset", "app.hard-reset");
    g_menu_append_submenu(menubar, "E_mulation", G_MENU_MODEL(emulation_menu));
    g_object_unref(emulation_menu);

    GMenu *size_menu = g_menu_new();
    append_radio_item(size_menu, "x0.5", "app.scale", "4");
    append_radio_item(size_menu, "x1", "app.scale", "8");
    append_radio_item(size_menu, "x2", "app.scale", "16");

    GMenu *theme_menu = g_menu_new();

    for (unsigned i = 0; i < G_N_ELEMENTS(themes); i++) {
        char target[4];
        g_snprintf(target, sizeof(target), "%u", i);
        append_radio_item(theme_menu, themes[i].name, "app.theme", target);
    }

    GMenu *view_menu = g_menu_new();
    g_menu_append_submenu(view_menu, "_Window size", G_MENU_MODEL(size_menu));
    g_menu_append_submenu(view_menu, "_Theme", G_MENU_MODEL(theme_menu));
    g_menu_append_submenu(menubar, "V_iew", G_MENU_MODEL(view_menu));
    g_object_unref(size_menu);
    g_object_unref(theme_menu);
    g_object_unref(view_menu);

    GMenu *help_menu = g_menu_new();
    g_menu_append(help_menu, "_About", "app.about");
    g_menu_append_submenu(menubar, "_Help", G_MENU_MODEL(help_menu));
    g_object_unref(help_menu);

    return G_MENU_MODEL(menubar);
}

/** @brief Application startup: actions, menu bar and accelerators. */
static void on_startup(GApplication *app, gpointer data)
{
    EmuApp *emu = data;

    g_action_map_add_action_entries(G_ACTION_MAP(app), app_actions,
                                    G_N_ELEMENTS(app_actions), emu);

    GMenuModel *menubar = build_menubar();
    gtk_application_set_menubar(GTK_APPLICATION(app), menubar);
    g_object_unref(menubar);

    set_accel(GTK_APPLICATION(app), "app.open", "<Control>o");
    set_accel(GTK_APPLICATION(app), "app.quit", "<Control>q");
    set_accel(GTK_APPLICATION(app), "app.pause", "<Control>p");
    set_accel(GTK_APPLICATION(app), "app.soft-reset", "<Control>u");
    set_accel(GTK_APPLICATION(app), "app.hard-reset", "<Control>r");

    set_accel(GTK_APPLICATION(app), "app.scale('4')", "<Control>1");
    set_accel(GTK_APPLICATION(app), "app.scale('8')", "<Control>2");
    set_accel(GTK_APPLICATION(app), "app.scale('16')", "<Control>3");

    for (unsigned i = 0; i < G_N_ELEMENTS(themes); i++) {
        char detailed[24];
        char accel[4];
        g_snprintf(detailed, sizeof(detailed), "app.theme('%u')", i);
        g_snprintf(accel, sizeof(accel), "F%u", i + 1);
        set_accel(GTK_APPLICATION(app), detailed, accel);
    }
}

/** @brief Creates the main window on the first activation. */
static void ensure_window(EmuApp *emu)
{
    if (emu->window) {
        return;
    }

    emu->window = gtk_application_window_new(emu->app);
    gtk_window_set_title(GTK_WINDOW(emu->window), "CHIP8 Emulator");
    gtk_window_set_default_size(GTK_WINDOW(emu->window),
                                DISPLAY_W * DEFAULT_PIXEL_SIZE,
                                DISPLAY_H * DEFAULT_PIXEL_SIZE);
    gtk_application_window_set_show_menubar(GTK_APPLICATION_WINDOW(emu->window),
                                            TRUE);

    emu->drawing_area = gtk_drawing_area_new();
    gtk_drawing_area_set_content_width(GTK_DRAWING_AREA(emu->drawing_area),
                                       DISPLAY_W * MIN_PIXEL_SIZE);
    gtk_drawing_area_set_content_height(GTK_DRAWING_AREA(emu->drawing_area),
                                        DISPLAY_H * MIN_PIXEL_SIZE);
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(emu->drawing_area),
                                   draw_cb, emu, NULL);
    gtk_window_set_child(GTK_WINDOW(emu->window), emu->drawing_area);

    char *splash_path = image_path("space.png");
    emu->splash = cairo_image_surface_create_from_png(splash_path);
    g_free(splash_path);

    if (cairo_surface_status(emu->splash) != CAIRO_STATUS_SUCCESS) {
        g_printerr("Failed to load splash image: %s\n",
                   cairo_status_to_string(cairo_surface_status(emu->splash)));
        cairo_surface_destroy(emu->splash);
        emu->splash = NULL;
    }

    GtkEventController *keys = gtk_event_controller_key_new();
    g_signal_connect(keys, "key-pressed", G_CALLBACK(on_key_pressed), emu);
    g_signal_connect(keys, "key-released", G_CALLBACK(on_key_released), emu);
    gtk_widget_add_controller(emu->window, keys);

    configure_style_and_icons(emu);
    gtk_widget_add_tick_callback(emu->drawing_area, tick_cb, emu, NULL);
}

/** @brief Application activation without files: opens an empty window. */
static void on_activate(GApplication *app, gpointer data)
{
    (void)app;

    EmuApp *emu = data;
    ensure_window(emu);
    gtk_window_present(GTK_WINDOW(emu->window));
}

/** @brief Application activation with a ROM passed on the command line. */
static void on_open(GApplication *app, GFile **files, int n_files,
                    const char *hint, gpointer data)
{
    (void)app;
    (void)hint;

    EmuApp *emu = data;
    ensure_window(emu);

    if (n_files > 0) {
        char *path = g_file_get_path(files[0]);

        if (path) {
            load_rom(emu, path);
            g_free(path);
        }
    }

    gtk_window_present(GTK_WINDOW(emu->window));
}

int main(int argc, char *argv[])
{
    EmuApp emu = {0};

    chip8_system_init(&emu.system);
    chip8_system_hard_reset(&emu.system);
    phosphor_init(&emu.phosphor);

    emu.app = gtk_application_new("io.github.mstuttgart.chip8emulator",
                                  G_APPLICATION_HANDLES_OPEN);

    g_signal_connect(emu.app, "startup", G_CALLBACK(on_startup), &emu);
    g_signal_connect(emu.app, "activate", G_CALLBACK(on_activate), &emu);
    g_signal_connect(emu.app, "open", G_CALLBACK(on_open), &emu);

    int status = g_application_run(G_APPLICATION(emu.app), argc, argv);

    g_object_unref(emu.app);
    if (emu.splash) {
        cairo_surface_destroy(emu.splash);
    }
    g_free(emu.rom_path);

    return status;
}
