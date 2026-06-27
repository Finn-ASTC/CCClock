#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "clk_app_config.h"
#include "clk_ascii_render.h"
#include "clk_clock.h"
#include "clk_file_util.h"
#include "clk_fs_watch.h"
#include "clk_json.h"
#include "clk_key_io.h"
#include "clk_menu.h"
#include "clk_menu_instance.h"
#include "clk_menu_theme.h"
#include "clk_term.h"

#define APP_CONFIG "assets/config/app_config.json"
#define CLK_MENU_DEFAULT_WIDTH 65
#define CLK_MENU_DEFAULT_HEIGHT 35
#define CLK_FRAME_MS 16
#define CLK_HOTRELOAD_TICKS 30

enum {
    CLK_ITEM_TFMT = 1,
    CLK_ITEM_FONT = 2,
    CLK_ITEM_THEME = 3,
    CLK_ITEM_QUIT = 4,
};

/* ── recenter ── */

static void recenter_clock(clk_ascii_render* render, const char* time_format, int term_w,
                           int term_h) {
    int cw, ch;
    if (!clk_ascii_render_get_size(render, time_format, &cw, &ch))
        return;
    clk_ascii_render_set_pos(render, (term_w - cw) / 2, (term_h - ch) / 2);
}

static void recenter_menu(clk_menu_instance* instance, int term_w, int term_h) {
    if (!instance)
        return;
    clk_menu_instance_set_position(instance, (term_w - instance->tex.tex_w) / 2,
                                   (term_h - instance->tex.tex_h) / 2);
}

/* ── menu io ── */

static clk_menu_input translate_menu_key(clk_key_event key_event) {
    switch (key_event.key) {
        case CLK_KEY_UP:
        case 'k':
            return CLK_MENU_INPUT_PREV_ITEM;
        case CLK_KEY_DOWN:
        case 'j':
            return CLK_MENU_INPUT_NEXT_ITEM;
        case CLK_KEY_LEFT:
        case 'h':
            return CLK_MENU_INPUT_DEC_VALUE;
        case CLK_KEY_RIGHT:
        case 'l':
            return CLK_MENU_INPUT_INC_VALUE;
        case '\t':
            return CLK_MENU_INPUT_NEXT_TAB;
        case '\r':
        case '\n':
            return CLK_MENU_INPUT_CONFIRM;
        default:
            return CLK_MENU_INPUT_NONE;
    }
}

/* ── focus ── */

typedef enum { FOCUS_CLOCK, FOCUS_MENU } focus_t;

int main(void) {
    if (!clk_term_init()) {
        printf("term init fail\n");
        return 1;
    }

    /* ================================================================
     *  Load config
     * ================================================================ */

    clk_app_config cfg;
    if (!clk_app_config_load(&cfg, APP_CONFIG)) {
        clk_term_close();
        printf("config load fail\n");
        return 1;
    }

    /* ================================================================
     *  Renderer
     * ================================================================ */

    clk_ascii_render render;
    if (!clk_ascii_render_create(&render, cfg.fonts.paths[cfg.fonts.idx])) {
        clk_app_config_deinit(&cfg);
        clk_term_close();
        printf("render create fail\n");
        return 1;
    }
    clk_ascii_render_set_z_order(&render, 0);
    clk_ascii_render_add_to_term(&render);

    /* ================================================================
     *  Menu
     * ================================================================ */

    clk_menu* menu = clk_menu_create();
    clk_menu_add_tab(menu, 0, "clock");
    clk_menu_add_item_str(menu, 0, CLK_ITEM_TFMT, "time format", cfg.time_formats.idx,
                          cfg.time_formats.options, cfg.time_formats.count);
    clk_menu_add_item_str(menu, 0, CLK_ITEM_FONT, "font", cfg.fonts.idx,
                          (const char**)cfg.fonts.names, cfg.fonts.count);
    clk_menu_add_item_str(menu, 0, CLK_ITEM_THEME, "menu theme", cfg.themes.idx,
                          (const char**)cfg.themes.names, cfg.themes.count);
    clk_menu_add_item_action(menu, 0, CLK_ITEM_QUIT, "quit");

    clk_menu_theme theme;
    memset(&theme, 0, sizeof(theme));
    clk_menu_theme_load(cfg.themes.paths[cfg.themes.idx], &theme);

    clk_menu_instance* menu_inst = clk_menu_instance_create(menu, &theme);
    clk_menu_instance_set_size(menu_inst, CLK_MENU_DEFAULT_WIDTH, CLK_MENU_DEFAULT_HEIGHT);
    clk_menu_instance_set_visible(menu_inst, false);
    clk_menu_instance_add_to_term(menu_inst);
    clk_sprite_set_z(menu_inst->sprite, 5);

    /* ================================================================
     *  Initial layout
     * ================================================================ */

    int term_width, term_height;
    clk_term_get_size(&term_width, &term_height);
    recenter_clock(&render, cfg.time_formats.current, term_width, term_height);
    recenter_menu(menu_inst, term_width, term_height);

    /* ================================================================
     *  Hot-reload state
     * ================================================================ */

    time_t last_app = 0, last_font = 0, last_theme = 0;
    clk_fs_file_changed(APP_CONFIG, &last_app);
    int hot_tick = 0;

    /* ================================================================
     *  Main loop
     * ================================================================ */

    focus_t focus = FOCUS_CLOCK;
    bool running = true;

    while (running) {
        clk_key_event key_event = clk_get_key_event();

        switch (focus) {
            case FOCUS_CLOCK:
                if (key_event.key == 's' || key_event.key == 'S') {
                    clk_menu_instance_set_visible(menu_inst, true);
                    focus = FOCUS_MENU;
                    continue;
                }
                if (key_event.key == 'f' || key_event.key == 'F') {
                    cfg.time_formats.idx = (cfg.time_formats.idx + 1) % cfg.time_formats.count;
                    clk_cfg_time_formats_switch(&cfg.time_formats);
                    clk_menu_set_value_str(menu, 0, CLK_ITEM_TFMT,
                                           cfg.time_formats.strings[cfg.time_formats.idx]);
                }
                if (key_event.key == 'r' || key_event.key == 'R') {
                    cfg.fonts.idx = (cfg.fonts.idx + 1) % cfg.fonts.count;
                    clk_ascii_render_change_font(&render, cfg.fonts.paths[cfg.fonts.idx]);
                    clk_menu_set_value_str(menu, 0, CLK_ITEM_FONT, cfg.fonts.names[cfg.fonts.idx]);
                }
                if (key_event.key == 'q' || key_event.key == 'Q')
                    running = false;
                break;

            case FOCUS_MENU:
                if (key_event.key == 'q' || key_event.key == 'Q') {
                    clk_menu_instance_set_visible(menu_inst, false);
                    focus = FOCUS_CLOCK;
                    continue;
                }
                {
                    clk_menu_event mev =
                        clk_menu_instance_handle_input(menu_inst, translate_menu_key(key_event));
                    if (mev.type == CLK_MENU_EVENT_SUBMIT && mev.item_id == CLK_ITEM_QUIT)
                        running = false;
                    if (mev.type == CLK_MENU_EVENT_VALUE_CHANGED) {
                        switch (mev.item_id) {
                            case CLK_ITEM_TFMT:
                                cfg.time_formats.idx = clk_menu_find_index(
                                    mev.value.str, cfg.time_formats.options, cfg.time_formats.count,
                                    cfg.time_formats.idx);
                                clk_cfg_time_formats_switch(&cfg.time_formats);
                                break;
                            case CLK_ITEM_FONT:
                                cfg.fonts.idx = clk_menu_find_index(mev.value.str,
                                                                    (const char**)cfg.fonts.names,
                                                                    cfg.fonts.count, cfg.fonts.idx);
                                clk_ascii_render_change_font(&render,
                                                             cfg.fonts.paths[cfg.fonts.idx]);
                                break;
                            case CLK_ITEM_THEME:
                                cfg.themes.idx = clk_menu_find_index(
                                    mev.value.str, (const char**)cfg.themes.names, cfg.themes.count,
                                    cfg.themes.idx);
                                clk_menu_instance_change_theme(menu_inst,
                                                               cfg.themes.paths[cfg.themes.idx]);
                                break;
                        }
                    }
                }
                break;
        }

        if (clk_term_size_changed()) {
            clk_term_get_size(&term_width, &term_height);
        }

        recenter_clock(&render, cfg.time_formats.current, term_width, term_height);
        recenter_menu(menu_inst, term_width, term_height);

        {
            char translated[128];
            char time_str[CLK_CLOCK_FORMAT_MAX_LENGTH];
            if (clk_clock_translate_format(cfg.time_formats.current, translated,
                                           sizeof(translated)) &&
                clk_clock_format_now(translated, time_str, sizeof(time_str)))
                clk_ascii_render_update(&render, time_str);
        }
        clk_menu_instance_render(menu_inst);
        clk_term_update();
        clk_term_draw();
        clk_term_sleep_ms(CLK_FRAME_MS);

        if (++hot_tick > CLK_HOTRELOAD_TICKS) {
            hot_tick = 0;

            if (clk_fs_file_changed(APP_CONFIG, &last_app)) {
                char* raw = clk_file_read_all(APP_CONFIG, NULL);
                if (raw) {
                    clk_json_value* new_json = clk_json_parse(raw);
                    free(raw);
                    if (new_json) {
                        clk_json_free(cfg.json);
                        cfg.json = new_json;
                        clk_app_config_reload(&cfg, menu, 0, CLK_ITEM_TFMT, CLK_ITEM_FONT,
                                              CLK_ITEM_THEME);
                    }
                }
            }
            if (cfg.fonts.idx >= 0 && cfg.fonts.idx < cfg.fonts.count &&
                clk_fs_file_changed(cfg.fonts.paths[cfg.fonts.idx], &last_font))
                clk_ascii_render_change_font(&render, cfg.fonts.paths[cfg.fonts.idx]);
            if (cfg.themes.idx >= 0 && cfg.themes.idx < cfg.themes.count &&
                clk_fs_file_changed(cfg.themes.paths[cfg.themes.idx], &last_theme))
                clk_menu_instance_change_theme(menu_inst, cfg.themes.paths[cfg.themes.idx]);

            clk_cfg_fonts_sync(&cfg.fonts, menu, 0, CLK_ITEM_FONT);
            clk_cfg_themes_sync(&cfg.themes, menu, 0, CLK_ITEM_THEME);
        }
    }

    /* ================================================================
     *  Save + cleanup
     * ================================================================ */

    {
        clk_json_value* fobj = clk_json_object_get(cfg.json, "fonts");
        clk_json_value* tobj = clk_json_object_get(cfg.json, "time_format");
        clk_json_value* mobj = clk_json_object_get(cfg.json, "menu");
        if (fobj)
            clk_json_object_set_string(fobj, "font", cfg.fonts.names[cfg.fonts.idx]);
        if (tobj)
            clk_json_object_set_string(tobj, "selected_time_format",
                                       cfg.time_formats.strings[cfg.time_formats.idx]);
        if (mobj)
            clk_json_object_set_string(mobj, "theme", cfg.themes.names[cfg.themes.idx]);
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
