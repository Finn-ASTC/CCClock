#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "clk_app_config.h"
#include "clk_app_setup.h"
#include "clk_file_util.h"
#include "clk_fs_watch.h"
#include "clk_input_box.h"
#include "clk_json.h"
#include "clk_key_io.h"
#include "clk_menu.h"
#include "clk_menu_instance.h"
#include "clk_term.h"

#define CLK_CONFIG_PATH "assets/config/app_config.json"
#define CLK_MENU_DEFAULT_WIDTH 65
#define CLK_MENU_DEFAULT_HEIGHT 35
#define CLK_FRAME_MS 16
#define CLK_HOTRELOAD_TICKS 30
#define CLK_MENU_Z_ORDER 5
#define CLK_INPUT_BOX_Z_ORDER 10
#define CLK_CLOCK_FORMAT_TRANSLATED_MAX 128

#define ENTITY_ALARM 0
#define ENTITY_POMODORO 1
#define ENTITY_SEGMENT 2

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

static int edit_entity_type = 0;
static int edit_entity_id = 0;
static int edit_entity_id2 = 0;

static void main_save_config(const clk_app_config* cfg, time_t* last_mtime) {
    clk_app_config_save(cfg, CLK_CONFIG_PATH);
    struct stat statbuf;
    if (stat(CLK_CONFIG_PATH, &statbuf) == 0)
        *last_mtime = statbuf.st_mtime;
}

static void main_open_input_box(clk_input_box** box, const char* initial, int entity_type,
                                int entity_id, int entity_id2, int term_w, int term_h) {
    *box = clk_input_box_create(initial, CLK_CLOCK_NAME_MAX - 1);
    clk_input_box_set_position(*box, (term_w - CLK_INPUT_BOX_WIDTH) / 2,
                               (term_h - CLK_INPUT_BOX_HEIGHT) / 2);
    clk_input_box_add_to_term(*box);
    clk_input_box_set_z_order(*box, CLK_INPUT_BOX_Z_ORDER);
    char* buf;
    size_t max;
    size_t* len;
    size_t* pos;
    clk_input_box_get_buffer(*box, &buf, &max, &len, &pos);
    clk_key_io_set_input(buf, max, len, pos);
    edit_entity_type = entity_type;
    edit_entity_id = entity_id;
    edit_entity_id2 = entity_id2;
    clk_term_cursor_set_shape(CLK_CURSOR_BAR_BLINK);
    clk_term_cursor_show();
}

typedef enum { CLK_FOCUS_CLOCK, CLK_FOCUS_MENU, CLK_FOCUS_INPUT_BOX } clk_focus;

int main(void) {
    if (!clk_term_init()) {
        fprintf(stderr, "term init fail\n");
        return 1;
    }

    /* ================================================================
     *  Load config
     * ================================================================ */

    clk_app_config cfg;
    if (!clk_app_config_load(&cfg, CLK_CONFIG_PATH)) {
        clk_term_close();
        fprintf(stderr, "config load fail\n");
        return 1;
    }

    /* ================================================================
     *  Clock
     * ================================================================ */

    clk_audio_engine* audio_engine = NULL;
    clk_clock clock;
    if (!clk_app_setup_clock(&clock, &audio_engine, &cfg)) {
        clk_app_config_deinit(&cfg);
        clk_term_close();
        fprintf(stderr, "clock setup fail\n");
        return 1;
    }

    /* ---- BGM ---- */
    clk_bgm bgm;
    clk_app_setup_bgm(&bgm, audio_engine, &cfg);

    /* ================================================================
     *  Renderer
     * ================================================================ */

    clk_ascii_render render;
    if (!clk_app_setup_render(&render, &cfg.ascii_clock)) {
        clk_app_setup_clock_deinit(&clock, audio_engine);
        clk_app_config_deinit(&cfg);
        clk_term_close();
        fprintf(stderr, "render setup fail\n");
        return 1;
    }

    /* ================================================================
     *  Menu
     * ================================================================ */

    clk_menu* menu = clk_app_setup_menu(&cfg, &clock);
    if (!menu) {
        clk_ascii_render_destroy(&render);
        clk_app_setup_clock_deinit(&clock, audio_engine);
        clk_app_config_deinit(&cfg);
        clk_term_close();
        fprintf(stderr, "menu setup fail\n");
        return 1;
    }

    clk_menu_theme theme;
    if (!clk_app_setup_theme(&theme, &cfg.themes))
        fprintf(stderr, "theme load fail, using defaults\n");

    clk_menu_instance* menu_inst = clk_menu_instance_create(menu, &theme);
    if (!menu_inst) {
        clk_menu_theme_destroy(&theme);
        clk_menu_destroy(menu);
        clk_ascii_render_destroy(&render);
        clk_app_setup_clock_deinit(&clock, audio_engine);
        clk_app_config_deinit(&cfg);
        clk_term_close();
        fprintf(stderr, "menu instance create fail\n");
        return 1;
    }
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
    clk_fs_file_changed(CLK_CONFIG_PATH, &last_app_mtime);
    int reload_tick = 0;
    clk_input_box* input_box = NULL;

    /* ================================================================
     *  Main loop
     * ================================================================ */

    clk_focus focus = CLK_FOCUS_CLOCK;
    bool running = true;

    while (running) {
        clk_key_event key_event =
            (focus == CLK_FOCUS_INPUT_BOX) ? clk_input_get_key_event() : clk_normal_get_key_event();

        switch (focus) {
            case CLK_FOCUS_INPUT_BOX: {
                if (clk_input_box_handle_input(input_box, key_event)) {
                    if (clk_input_box_is_confirmed(input_box)) {
                        const char* result = clk_input_box_get_result(input_box);
                        if (result[0] != '\0') {
                            if (edit_entity_type == ENTITY_ALARM) {
                                clk_clock_alarm* a =
                                    clk_clock_find_alarm_by_id(&clock, edit_entity_id);
                                if (a)
                                    strncpy(a->name, result, CLK_CLOCK_NAME_MAX - 1);
                            } else if (edit_entity_type == ENTITY_POMODORO) {
                                clk_clock_pomodoro* po =
                                    clk_clock_find_pomodoro_by_id(&clock, edit_entity_id);
                                if (po)
                                    strncpy(po->name, result, CLK_CLOCK_NAME_MAX - 1);
                            } else {
                                clk_clock_pomodoro_segment* seg =
                                    clk_clock_pomodoro_find_segment_by_id(&clock, edit_entity_id,
                                                                          edit_entity_id2);
                                if (seg)
                                    strncpy(seg->name, result, CLK_CLOCK_NAME_MAX - 1);
                            }
                            clk_app_menu_rebuild(menu, &clock, &cfg);
                            main_save_config(&cfg, &last_app_mtime);
                        }
                    }
                    clk_key_io_set_normal();
                    clk_input_box_remove_from_term(input_box);
                    clk_input_box_destroy(input_box);
                    input_box = NULL;
                    clk_term_cursor_hide();
                    focus = CLK_FOCUS_MENU;
                    continue;
                }
                clk_input_box_render(input_box);
                break;
            }
            case CLK_FOCUS_CLOCK:
                if (key_event.key_mask == KEY_SPACE) {
                    clk_clock_stop_bell(&clock);
                    continue;
                }
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

                    if (menu_event.type == CLK_MENU_EVENT_VALUE_CHANGED &&
                        menu_event.tab_id == CLK_TAB_BASIC) {
                        switch (menu_event.item_id) {
                            case CLK_BASIC_ITEM_TIME_FORMAT:
                                cfg.ascii_clock.time_formats.index = clk_menu_find_index(
                                    menu_event.value.str, cfg.ascii_clock.time_formats.options,
                                    cfg.ascii_clock.time_formats.count,
                                    cfg.ascii_clock.time_formats.index);
                                clk_cfg_ascii_clock_theme_switch_time(&cfg.ascii_clock);
                                clk_app_config_sync_basic(&cfg);
                                main_save_config(&cfg, &last_app_mtime);
                                break;
                            case CLK_BASIC_ITEM_FONT:
                                cfg.ascii_clock.fonts.index = clk_menu_find_index(
                                    menu_event.value.str, (const char**)cfg.ascii_clock.fonts.names,
                                    cfg.ascii_clock.fonts.count, cfg.ascii_clock.fonts.index);
                                clk_ascii_render_change_font(
                                    &render,
                                    cfg.ascii_clock.fonts.paths[cfg.ascii_clock.fonts.index]);
                                clk_app_config_sync_basic(&cfg);
                                main_save_config(&cfg, &last_app_mtime);
                                break;
                            case CLK_BASIC_ITEM_THEME:
                                cfg.themes.index = clk_menu_find_index(
                                    menu_event.value.str, (const char**)cfg.themes.names,
                                    cfg.themes.count, cfg.themes.index);
                                clk_menu_instance_change_theme(menu_inst,
                                                               cfg.themes.paths[cfg.themes.index]);
                                clk_app_config_sync_basic(&cfg);
                                main_save_config(&cfg, &last_app_mtime);
                                break;
                            case CLK_BASIC_ITEM_BGM_ENABLED:
                                clk_bgm_set_enabled(&bgm, menu_event.value.b);
                                clk_app_config_sync_bgm(&cfg, &bgm);
                                main_save_config(&cfg, &last_app_mtime);
                                break;
                            case CLK_BASIC_ITEM_BGM_VOLUME:
                                clk_bgm_set_volume(&bgm, (int)menu_event.value.num);
                                clk_app_config_sync_bgm(&cfg, &bgm);
                                main_save_config(&cfg, &last_app_mtime);
                                break;
                            case CLK_BASIC_ITEM_BGM_SOUND: {
                                const char* audio_dir = NULL;
                                clk_json_value* v = clk_json_object_get(cfg.json, "audio_dir");
                                if (v && clk_json_is_string(v))
                                    clk_json_get_string(v, &audio_dir);
                                if (audio_dir && menu_event.value.str &&
                                    strcmp(menu_event.value.str, "(none)") != 0) {
                                    char path[CLK_SOUND_PATH_MAX];
                                    snprintf(path, sizeof(path), "%s/%s.mp3", audio_dir,
                                             menu_event.value.str);
                                    if (clk_bgm_load_sound(&bgm, path)) {
                                        strncpy(bgm.sound_file, menu_event.value.str,
                                                CLK_BGM_SOUND_MAX - 1);
                                        if (bgm.enabled)
                                            clk_bgm_set_enabled(&bgm, true);
                                    }
                                }
                                clk_app_config_sync_bgm(&cfg, &bgm);
                                main_save_config(&cfg, &last_app_mtime);
                                break;
                            }
                        }
                    }

                    if (menu_event.type == CLK_MENU_EVENT_VALUE_CHANGED &&
                        menu_event.tab_id == CLK_TAB_ALARM) {
                        int alarm_id, off;
                        CLK_ALARM_DECODE(menu_event.item_id, alarm_id, off);
                        clk_clock_alarm* a = clk_clock_find_alarm_by_id(&clock, alarm_id);
                        if (!a)
                            break;

                        switch (off) {
                            case CLK_ALARM_ENABLED_OFFSET:
                                a->alarm.enabled = menu_event.value.b;
                                break;
                            case CLK_ALARM_HOUR_OFFSET:
                                clk_alarm_set(&a->alarm, (int)menu_event.value.num, a->alarm.minute,
                                              0);
                                break;
                            case CLK_ALARM_MINUTE_OFFSET:
                                clk_alarm_set(&a->alarm, a->alarm.hour, (int)menu_event.value.num,
                                              0);
                                break;
                            case CLK_ALARM_REPEAT_OFFSET:
                                a->repeat_days = clk_repeat_days_from_string(menu_event.value.str);
                                break;
                            case CLK_ALARM_LOOP_OFFSET:
                                a->loop = menu_event.value.b;
                                break;
                            case CLK_ALARM_REPEAT_COUNT:
                                a->repeat_count = (int)menu_event.value.num;
                                break;
                            case CLK_ALARM_VOLUME_OFFSET:
                                a->volume = menu_event.value.num / 100.0f;
                                break;
                            case CLK_ALARM_SOUND_OFFSET: {
                                clk_audio_destroy(a->sound);
                                a->sound = NULL;
                                const char* audio_dir = NULL;
                                clk_json_value* v = clk_json_object_get(cfg.json, "audio_dir");
                                if (v && clk_json_is_string(v))
                                    clk_json_get_string(v, &audio_dir);
                                if (audio_dir && menu_event.value.str &&
                                    strcmp(menu_event.value.str, "(none)") != 0) {
                                    char path[CLK_SOUND_PATH_MAX];
                                    snprintf(path, sizeof(path), "%s/%s.mp3", audio_dir,
                                             menu_event.value.str);
                                    a->sound = clk_audio_load(audio_engine, path);
                                }
                                break;
                            }
                        }
                        clk_app_config_sync_clock(&cfg, &clock);
                        main_save_config(&cfg, &last_app_mtime);
                    }

                    if (menu_event.type == CLK_MENU_EVENT_SUBMIT &&
                        menu_event.tab_id == CLK_TAB_ALARM) {
                        int alarm_id, off;
                        CLK_ALARM_DECODE(menu_event.item_id, alarm_id, off);

                        if (off == CLK_ALARM_ADD_OFFSET) {
                            clk_clock_alarm a;
                            memset(&a, 0, sizeof(a));
                            a.id = clk_clock_next_alarm_id(&clock);
                            snprintf(a.name, sizeof(a.name), "unnamed alarm");
                            int idx = clk_clock_find_alarm_index_by_id(&clock, alarm_id);
                            clk_clock_add_alarm_at(&clock, &a,
                                                   idx >= 0 ? idx + 1 : clock.alarm_count);
                            clk_app_menu_rebuild(menu, &clock, &cfg);
                            clk_app_config_sync_clock(&cfg, &clock);
                            main_save_config(&cfg, &last_app_mtime);
                        }
                        if (off == CLK_ALARM_HEADER_OFFSET) {
                            clk_clock_alarm* a = clk_clock_find_alarm_by_id(&clock, alarm_id);
                            if (a) {
                                main_open_input_box(&input_box, a->name, ENTITY_ALARM, alarm_id, 0,
                                                    term_width, term_height);
                                focus = CLK_FOCUS_INPUT_BOX;
                                continue;
                            }
                        }
                        if (off == CLK_ALARM_DELETE_OFFSET) {
                            clk_clock_alarm* a = clk_clock_find_alarm_by_id(&clock, alarm_id);
                            if (a) {
                                clk_audio_destroy(a->sound);
                                clk_clock_remove_alarm_by_id(&clock, alarm_id);
                                clk_app_menu_rebuild(menu, &clock, &cfg);
                                clk_app_config_sync_clock(&cfg, &clock);
                                main_save_config(&cfg, &last_app_mtime);
                            }
                        }
                    }

                    if (menu_event.type == CLK_MENU_EVENT_VALUE_CHANGED &&
                        menu_event.tab_id == CLK_TAB_POMODORO) {
                        int pomodoro_id, pomodoro_offset;
                        CLK_POMO_DECODE(menu_event.item_id, pomodoro_id, pomodoro_offset);
                        clk_clock_pomodoro* po = clk_clock_find_pomodoro_by_id(&clock, pomodoro_id);
                        if (!po)
                            break;

                        if (pomodoro_offset < CLK_POMO_SEGMENT_BASE) {
                            switch (pomodoro_offset) {
                                case CLK_POMO_ENABLED_OFFSET: {
                                    int idx =
                                        clk_clock_find_pomodoro_index_by_id(&clock, pomodoro_id);
                                    if (menu_event.value.b)
                                        clk_clock_pomodoro_start(&clock, idx);
                                    else
                                        clk_clock_pomodoro_stop(&clock, idx);
                                    break;
                                }
                            }
                        } else {
                            int segment_id, field;
                            CLK_POMO_SEG_DECODE(pomodoro_offset, segment_id, field);
                            clk_clock_pomodoro_segment* seg = clk_clock_pomodoro_find_segment_by_id(
                                &clock, pomodoro_id, segment_id);
                            if (!seg)
                                break;
                            switch (field) {
                                case CLK_POMO_SEG_DURATION_OFFSET:
                                    seg->duration_seconds = (int)menu_event.value.num * 60;
                                    break;
                                case CLK_POMO_SEG_REPEAT_OFFSET:
                                    seg->repeat_count = (int)menu_event.value.num;
                                    break;
                                case CLK_POMO_SEG_VOLUME_OFFSET:
                                    seg->volume = menu_event.value.num / 100.0f;
                                    break;
                                case CLK_POMO_SEG_SOUND_OFFSET: {
                                    clk_audio_destroy(seg->sound);
                                    seg->sound = NULL;
                                    const char* audio_dir = NULL;
                                    clk_json_value* v = clk_json_object_get(cfg.json, "audio_dir");
                                    if (v && clk_json_is_string(v))
                                        clk_json_get_string(v, &audio_dir);
                                    if (audio_dir && menu_event.value.str &&
                                        strcmp(menu_event.value.str, "(none)") != 0) {
                                        char path[CLK_SOUND_PATH_MAX];
                                        snprintf(path, sizeof(path), "%s/%s.mp3", audio_dir,
                                                 menu_event.value.str);
                                        seg->sound = clk_audio_load(audio_engine, path);
                                    }
                                    break;
                                }
                            }
                        }
                        clk_app_config_sync_clock(&cfg, &clock);
                        main_save_config(&cfg, &last_app_mtime);
                    }

                    if (menu_event.type == CLK_MENU_EVENT_SUBMIT &&
                        menu_event.tab_id == CLK_TAB_POMODORO) {
                        int pomodoro_id, pomodoro_offset;
                        CLK_POMO_DECODE(menu_event.item_id, pomodoro_id, pomodoro_offset);

                        if (pomodoro_offset < CLK_POMO_SEGMENT_BASE) {
                            if (pomodoro_offset == CLK_POMO_HEADER_OFFSET) {
                                clk_clock_pomodoro* po =
                                    clk_clock_find_pomodoro_by_id(&clock, pomodoro_id);
                                if (po) {
                                    main_open_input_box(&input_box, po->name, ENTITY_POMODORO,
                                                        pomodoro_id, 0, term_width, term_height);
                                    focus = CLK_FOCUS_INPUT_BOX;
                                    continue;
                                }
                            }
                            if (pomodoro_offset == CLK_POMO_ADD_OFFSET) {
                                clk_clock_pomodoro po;
                                memset(&po, 0, sizeof(po));
                                po.id = clk_clock_next_pomodoro_id(&clock);
                                po.current_segment = -1;
                                snprintf(po.name, sizeof(po.name), "unnamed pomodoro");
                                int idx = clk_clock_find_pomodoro_index_by_id(&clock, pomodoro_id);
                                clk_clock_add_pomodoro_at(
                                    &clock, &po, idx >= 0 ? idx + 1 : clock.pomodoro_count);
                                clk_app_menu_rebuild(menu, &clock, &cfg);
                                clk_app_config_sync_clock(&cfg, &clock);
                                main_save_config(&cfg, &last_app_mtime);
                            }
                            if (pomodoro_offset == CLK_POMO_DELETE_OFFSET) {
                                clk_clock_pomodoro* po =
                                    clk_clock_find_pomodoro_by_id(&clock, pomodoro_id);
                                if (po) {
                                    for (int j = 0; j < po->segment_count; ++j)
                                        clk_audio_destroy(po->segments[j].sound);
                                    clk_clock_remove_pomodoro_by_id(&clock, pomodoro_id);
                                    clk_app_menu_rebuild(menu, &clock, &cfg);
                                    clk_app_config_sync_clock(&cfg, &clock);
                                    main_save_config(&cfg, &last_app_mtime);
                                }
                            }
                        } else {
                            int segment_id, field;
                            CLK_POMO_SEG_DECODE(pomodoro_offset, segment_id, field);

                            if (field == CLK_POMO_SEG_HEADER_OFFSET) {
                                clk_clock_pomodoro_segment* seg =
                                    clk_clock_pomodoro_find_segment_by_id(&clock, pomodoro_id,
                                                                          segment_id);
                                if (seg) {
                                    main_open_input_box(&input_box, seg->name, ENTITY_SEGMENT,
                                                        pomodoro_id, segment_id, term_width,
                                                        term_height);
                                    focus = CLK_FOCUS_INPUT_BOX;
                                    continue;
                                }
                            }
                            if (field == CLK_POMO_SEG_ADD_OFFSET) {
                                clk_clock_pomodoro* po =
                                    clk_clock_find_pomodoro_by_id(&clock, pomodoro_id);
                                int pomodoro_idx =
                                    clk_clock_find_pomodoro_index_by_id(&clock, pomodoro_id);
                                if (po && pomodoro_idx >= 0) {
                                    clk_clock_pomodoro_segment seg;
                                    memset(&seg, 0, sizeof(seg));
                                    int max_segment_id = -1;
                                    for (int j = 0; j < po->segment_count; ++j)
                                        if (po->segments[j].id > max_segment_id)
                                            max_segment_id = po->segments[j].id;
                                    seg.id = max_segment_id + 1;
                                    seg.duration_seconds = 60;
                                    snprintf(seg.name, sizeof(seg.name), "unnamed segment");
                                    int segment_idx = clk_clock_pomodoro_find_segment_index_by_id(
                                        &clock, pomodoro_id, segment_id);
                                    clk_clock_pomodoro_add_segment_at(
                                        &clock, pomodoro_idx, &seg,
                                        segment_idx >= 0 ? segment_idx + 1 : 0);
                                    clk_app_menu_rebuild(menu, &clock, &cfg);
                                    clk_app_config_sync_clock(&cfg, &clock);
                                    main_save_config(&cfg, &last_app_mtime);
                                }
                            }
                            if (field == CLK_POMO_SEG_DELETE_OFFSET) {
                                clk_clock_pomodoro* po =
                                    clk_clock_find_pomodoro_by_id(&clock, pomodoro_id);
                                int pomodoro_idx =
                                    clk_clock_find_pomodoro_index_by_id(&clock, pomodoro_id);
                                if (po && pomodoro_idx >= 0) {
                                    clk_clock_pomodoro_segment* seg =
                                        clk_clock_pomodoro_find_segment_by_id(&clock, pomodoro_id,
                                                                              segment_id);
                                    if (seg)
                                        clk_audio_destroy(seg->sound);
                                    int segment_idx = clk_clock_pomodoro_find_segment_index_by_id(
                                        &clock, pomodoro_id, segment_id);
                                    clk_clock_pomodoro_remove_segment(&clock, pomodoro_idx,
                                                                      segment_idx);
                                    clk_app_menu_rebuild(menu, &clock, &cfg);
                                    clk_app_config_sync_clock(&cfg, &clock);
                                    main_save_config(&cfg, &last_app_mtime);
                                }
                            }
                        }
                    }
                }
                break;
        }

        clk_clock_update(&clock);
        clk_audio_update();

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

            if (clk_fs_file_changed(CLK_CONFIG_PATH, &last_app_mtime)) {
                char* raw = clk_file_read_all(CLK_CONFIG_PATH, NULL);
                if (raw) {
                    clk_json_value* new_json = clk_json_parse(raw);
                    free(raw);
                    if (new_json) {
                        clk_json_free(cfg.json);
                        cfg.json = new_json;
                        clk_app_config_reload(&cfg, menu, 0, CLK_BASIC_ITEM_TIME_FORMAT,
                                              CLK_BASIC_ITEM_FONT, CLK_BASIC_ITEM_THEME);
                        clk_app_clock_diff_update(&clock, audio_engine, &cfg);
                        clk_app_menu_rebuild(menu, &clock, &cfg);
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

    clk_app_config_sync_basic(&cfg);
    clk_app_config_sync_clock(&cfg, &clock);
    main_save_config(&cfg, &last_app_mtime);

    clk_menu_instance_destroy(menu_inst);
    clk_menu_destroy(menu);
    clk_menu_theme_destroy(&theme);
    clk_ascii_render_destroy(&render);
    clk_bgm_deinit(&bgm);
    clk_app_setup_clock_deinit(&clock, audio_engine);
    clk_app_config_deinit(&cfg);
    clk_term_close();
    return 0;
}
