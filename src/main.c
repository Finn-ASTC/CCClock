#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include "clk_app_config.h"
#include "clk_app_setup.h"
#include "clk_file_util.h"
#include "clk_fs_watch.h"
#include "clk_json.h"
#include "clk_key_io.h"
#include "clk_menu.h"
#include "clk_menu_instance.h"
#include "clk_term.h"

#define APP_CONFIG "assets/config/app_config.json"
#define CLK_MENU_DEFAULT_WIDTH 65
#define CLK_MENU_DEFAULT_HEIGHT 35
#define CLK_FRAME_MS 16
#define CLK_HOTRELOAD_TICKS 30
#define CLK_MENU_Z_ORDER 5
#define CLK_CLOCK_FORMAT_TRANSLATED_MAX 128

/* ------------------------------------------------------------------
 *  Layout helpers
 * ------------------------------------------------------------------ */

static void recenter_clock(clk_ascii_render* render, const char* time_format, int term_width,
                           int term_height) {
    int clock_w, clock_h;
    if (!clk_ascii_render_get_size(render, time_format, &clock_w, &clock_h))
        return;
    clk_ascii_render_set_pos(render, (term_width - clock_w) / 2, (term_height - clock_h) / 2);
}

static void recenter_menu(clk_menu_instance* instance, int term_width, int term_height) {
    if (!instance)
        return;
    clk_menu_instance_set_position(instance, (term_width - instance->tex.tex_w) / 2,
                                   (term_height - instance->tex.tex_h) / 2);
}

/* ------------------------------------------------------------------
 *  Menu input translation
 * ------------------------------------------------------------------ */

static clk_menu_input translate_menu_key(clk_key_event key_event) {
    switch (key_event.key_mask) {
        case KEY_UP:
        case KEY_k_LOWER:
            return CLK_MENU_INPUT_PREV_ITEM;
        case KEY_DOWN:
        case KEY_j_LOWER:
            return CLK_MENU_INPUT_NEXT_ITEM;
        case KEY_LEFT:
        case KEY_h_LOWER:
            return CLK_MENU_INPUT_DEC_VALUE;
        case KEY_RIGHT:
        case KEY_l_LOWER:
            return CLK_MENU_INPUT_INC_VALUE;
        case KEY_TAB:
            return CLK_MENU_INPUT_NEXT_TAB;
        case KEY_ENTER:
            return CLK_MENU_INPUT_CONFIRM;
        default:
            return CLK_MENU_INPUT_NONE;
    }
}

/* ------------------------------------------------------------------
 *  Focus model
 * ------------------------------------------------------------------ */

typedef enum { CLK_FOCUS_CLOCK, CLK_FOCUS_MENU } clk_focus;

int main(void) {
    if (!clk_term_init()) {
        fprintf(stderr, "term init fail\n");
        return 1;
    }

    /* ================================================================
     *  Load config
     * ================================================================ */

    clk_app_config cfg;
    if (!clk_app_config_load(&cfg, APP_CONFIG)) {
        clk_term_close();
        fprintf(stderr, "config load fail\n");
        return 1;
    }

    /* ================================================================
     *  Renderer
     * ================================================================ */

    clk_ascii_render render;
    if (!clk_app_setup_render(&render, &cfg.ascii_clock)) {
        clk_app_config_deinit(&cfg);
        clk_term_close();
        fprintf(stderr, "render setup fail\n");
        return 1;
    }

    /* ================================================================
     *  Menu
     * ================================================================ */

    clk_menu* menu = clk_app_setup_menu(&cfg);
    if (!menu) {
        clk_ascii_render_destroy(&render);
        clk_app_config_deinit(&cfg);
        clk_term_close();
        fprintf(stderr, "menu setup fail\n");
        return 1;
    }

    clk_menu_theme theme;
    if (!clk_app_setup_theme(&theme, &cfg.themes))
        fprintf(stderr, "theme load fail, using defaults\n");

    clk_menu_instance* menu_inst = clk_menu_instance_create(menu, &theme);
    clk_menu_instance_set_size(menu_inst, CLK_MENU_DEFAULT_WIDTH, CLK_MENU_DEFAULT_HEIGHT);
    clk_menu_instance_set_visible(menu_inst, false);
    clk_menu_instance_add_to_term(menu_inst);
    clk_sprite_set_z(menu_inst->sprite, CLK_MENU_Z_ORDER);

    /* ================================================================
     *  Initial layout
     * ================================================================ */

    int term_width, term_height;
    clk_term_get_size(&term_width, &term_height);
    recenter_clock(&render, cfg.ascii_clock.time_formats.current, term_width, term_height);
    recenter_menu(menu_inst, term_width, term_height);

    /* ================================================================
     *  Hot-reload state
     * ================================================================ */

    time_t last_app_mtime = 0, last_font_mtime = 0, last_theme_mtime = 0;
    clk_fs_file_changed(APP_CONFIG, &last_app_mtime);
    int reload_tick = 0;

    /* ================================================================
     *  Main loop
     * ================================================================ */

    clk_focus focus = CLK_FOCUS_CLOCK;
    bool running = true;

    while (running) {
        clk_key_event key_event = clk_normal_get_key_event();

        switch (focus) {
            case CLK_FOCUS_CLOCK:
                if (key_event.key_mask == KEY_s_LOWER || key_event.key_mask == KEY_S_UPPER) {
                    clk_menu_instance_set_visible(menu_inst, true);
                    focus = CLK_FOCUS_MENU;
                    continue;
                }
                if (key_event.key_mask == KEY_f_LOWER || key_event.key_mask == KEY_F_UPPER) {
                    cfg.ascii_clock.time_formats.index = (cfg.ascii_clock.time_formats.index + 1) %
                                                         cfg.ascii_clock.time_formats.count;
                    clk_cfg_ascii_clock_theme_switch_time(&cfg.ascii_clock);
                    clk_menu_set_value_str(
                        menu, 0, CLK_BASIC_ITEM_TIME_FORMAT,
                        cfg.ascii_clock.time_formats.strings[cfg.ascii_clock.time_formats.index]);
                }
                if (key_event.key_mask == KEY_r_LOWER || key_event.key_mask == KEY_R_UPPER) {
                    cfg.ascii_clock.fonts.index =
                        (cfg.ascii_clock.fonts.index + 1) % cfg.ascii_clock.fonts.count;
                    clk_ascii_render_change_font(
                        &render, cfg.ascii_clock.fonts.paths[cfg.ascii_clock.fonts.index]);
                    clk_menu_set_value_str(
                        menu, 0, CLK_BASIC_ITEM_FONT,
                        cfg.ascii_clock.fonts.names[cfg.ascii_clock.fonts.index]);
                }
                if (key_event.key_mask == KEY_q_LOWER || key_event.key_mask == KEY_Q_UPPER)
                    running = false;
                break;

            case CLK_FOCUS_MENU:
                if (key_event.key_mask == KEY_q_LOWER || key_event.key_mask == KEY_Q_UPPER) {
                    clk_menu_instance_set_visible(menu_inst, false);
                    focus = CLK_FOCUS_CLOCK;
                    continue;
                }
                {
                    clk_menu_event menu_event =
                        clk_menu_instance_handle_input(menu_inst, translate_menu_key(key_event));
                    if (menu_event.type == CLK_MENU_EVENT_SUBMIT &&
                        menu_event.item_id == CLK_BASIC_ITEM_QUIT)
                        running = false;
                    if (menu_event.type == CLK_MENU_EVENT_VALUE_CHANGED) {
                        switch (menu_event.item_id) {
                            case CLK_BASIC_ITEM_TIME_FORMAT:
                                cfg.ascii_clock.time_formats.index = clk_menu_find_index(
                                    menu_event.value.str, cfg.ascii_clock.time_formats.options,
                                    cfg.ascii_clock.time_formats.count,
                                    cfg.ascii_clock.time_formats.index);
                                clk_cfg_ascii_clock_theme_switch_time(&cfg.ascii_clock);
                                break;
                            case CLK_BASIC_ITEM_FONT:
                                cfg.ascii_clock.fonts.index = clk_menu_find_index(
                                    menu_event.value.str, (const char**)cfg.ascii_clock.fonts.names,
                                    cfg.ascii_clock.fonts.count, cfg.ascii_clock.fonts.index);
                                clk_ascii_render_change_font(
                                    &render,
                                    cfg.ascii_clock.fonts.paths[cfg.ascii_clock.fonts.index]);
                                break;
                            case CLK_BASIC_ITEM_THEME:
                                cfg.themes.index = clk_menu_find_index(
                                    menu_event.value.str, (const char**)cfg.themes.names,
                                    cfg.themes.count, cfg.themes.index);
                                clk_menu_instance_change_theme(menu_inst,
                                                               cfg.themes.paths[cfg.themes.index]);
                                break;
                        }
                    }
                }
                break;
        }

        if (clk_term_size_changed()) {
            clk_term_get_size(&term_width, &term_height);
        }

        recenter_clock(&render, cfg.ascii_clock.time_formats.current, term_width, term_height);
        recenter_menu(menu_inst, term_width, term_height);

        {
            char translated[CLK_CLOCK_FORMAT_TRANSLATED_MAX];
            char time_str[CLK_CLOCK_FORMAT_MAX_LENGTH];
            if (clk_clock_translate_format(cfg.ascii_clock.time_formats.current, translated,
                                           sizeof(translated)) &&
                clk_clock_format_now(translated, time_str, sizeof(time_str)))
                clk_ascii_render_update(&render, time_str);
        }
        clk_menu_instance_render(menu_inst);
        clk_term_update();
        clk_term_draw();
        clk_time_sleep_ms(CLK_FRAME_MS);

        if (++reload_tick > CLK_HOTRELOAD_TICKS) {
            reload_tick = 0;

            if (clk_fs_file_changed(APP_CONFIG, &last_app_mtime)) {
                char* raw = clk_file_read_all(APP_CONFIG, NULL);
                if (raw) {
                    clk_json_value* new_json = clk_json_parse(raw);
                    free(raw);
                    if (new_json) {
                        clk_json_free(cfg.json);
                        cfg.json = new_json;
                        clk_app_config_reload(&cfg, menu, 0, CLK_BASIC_ITEM_TIME_FORMAT,
                                              CLK_BASIC_ITEM_FONT, CLK_BASIC_ITEM_THEME);
                    }
                }
            }
            if (cfg.ascii_clock.fonts.index >= 0 &&
                cfg.ascii_clock.fonts.index < cfg.ascii_clock.fonts.count &&
                clk_fs_file_changed(cfg.ascii_clock.fonts.paths[cfg.ascii_clock.fonts.index],
                                    &last_font_mtime))
                clk_ascii_render_change_font(
                    &render, cfg.ascii_clock.fonts.paths[cfg.ascii_clock.fonts.index]);
            if (cfg.themes.index >= 0 && cfg.themes.index < cfg.themes.count &&
                clk_fs_file_changed(cfg.themes.paths[cfg.themes.index], &last_theme_mtime))
                clk_menu_instance_change_theme(menu_inst, cfg.themes.paths[cfg.themes.index]);

            clk_cfg_ascii_clock_theme_sync_fonts(&cfg.ascii_clock, menu, 0, CLK_BASIC_ITEM_FONT);
            clk_cfg_themes_sync(&cfg.themes, menu, 0, CLK_BASIC_ITEM_THEME);
        }
    }

    /* ================================================================
     *  Save + cleanup
     * ================================================================ */

    {
        clk_json_value* theme_obj = clk_json_object_get(cfg.json, "ascii_clock_theme");
        clk_json_value* menu_obj = clk_json_object_get(cfg.json, "menu");
        if (theme_obj) {
            clk_json_value* font_obj = clk_json_object_get(theme_obj, "fonts");
            clk_json_value* time_obj = clk_json_object_get(theme_obj, "time_format");
            if (font_obj)
                clk_json_object_set_string(
                    font_obj, "font", cfg.ascii_clock.fonts.names[cfg.ascii_clock.fonts.index]);
            if (time_obj)
                clk_json_object_set_string(
                    time_obj, "selected_time_format",
                    cfg.ascii_clock.time_formats.strings[cfg.ascii_clock.time_formats.index]);
        }
        if (menu_obj)
            clk_json_object_set_string(menu_obj, "theme", cfg.themes.names[cfg.themes.index]);
    }
    {
        char* out = clk_json_stringify_pretty(cfg.json);
        if (out) {
            FILE* file = fopen(APP_CONFIG, "w");
            if (file) {
                fputs(out, file);
                fclose(file);
            }
            free(out);
        }
    }

    clk_menu_instance_destroy(menu_inst);
    clk_menu_destroy(menu);
    clk_menu_theme_destroy(&theme);
    clk_ascii_render_destroy(&render);
    clk_app_config_deinit(&cfg);
    clk_term_close();
    return 0;
}
