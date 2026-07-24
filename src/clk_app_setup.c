#include "clk_app_setup.h"

#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "clk_fs_watch.h"

const char* clk_repeat_day_options[] = {
    "Today",  "Monday",   "Tuesday", "Wednesday", "Thursday",
    "Friday", "Saturday", "Sunday",  "Everyday",
};

/* ================================================================
 *  Internal helpers
 * ================================================================ */

static void register_basic_tab(const clk_app_config* cfg, clk_menu* menu);

static void scan_sound_options(const clk_app_config* cfg, char*** out_paths, int* out_count,
                               char*** out_names, const char*** out_opts);

static void register_alarm_tab(clk_menu* menu, clk_clock* clock, const char** sound_opts,
                               int sound_count);

static void register_pomodoro_tab(clk_menu* menu, clk_clock* clock, const char** sound_opts,
                                  int sound_count);

static void clk_setup_load_sound(clk_audio_sound** sound, const char* audio_dir,
                                 const char* sound_file, clk_audio_engine* engine);

static void clk_setup_scan_and_register_tabs(clk_menu* menu, clk_clock* clock,
                                             const clk_app_config* cfg);

/** Register the "basic" tab items (time format, font, theme, quit). */
static void register_basic_tab(const clk_app_config* cfg, clk_menu* menu) {
    clk_menu_add_tab(menu, CLK_TAB_BASIC, "basic");

    /* ---- time format ---- */
    clk_menu_add_item_str(menu, CLK_TAB_BASIC, CLK_BASIC_ITEM_TIME_FORMAT, "time format",
                          cfg->ascii_clock.time_formats.index,
                          cfg->ascii_clock.time_formats.options,
                          cfg->ascii_clock.time_formats.count);

    /* ---- font ---- */
    clk_menu_add_item_str(
        menu, CLK_TAB_BASIC, CLK_BASIC_ITEM_FONT, "font", cfg->ascii_clock.fonts.index,
        (const char* const*)cfg->ascii_clock.fonts.names, cfg->ascii_clock.fonts.count);

    /* ---- menu theme ---- */
    clk_menu_add_item_str(menu, CLK_TAB_BASIC, CLK_BASIC_ITEM_THEME, "menu theme",
                          cfg->themes.index, (const char* const*)cfg->themes.names,
                          cfg->themes.count);

    /* ---- BGM ---- */
    clk_menu_add_item_bool(menu, CLK_TAB_BASIC, CLK_BASIC_ITEM_BGM_ENABLED, "BGM enabled",
                           cfg->bgm.count > 0 ? cfg->bgm.items[0].enabled : false);
    clk_menu_add_item_int(menu, CLK_TAB_BASIC, CLK_BASIC_ITEM_BGM_VOLUME, "BGM volume",
                          cfg->bgm.count > 0 ? cfg->bgm.items[0].volume : 50, 0, 100, 5);

    {
        char** paths = NULL;
        int sound_cnt = 0;
        char** names = NULL;
        const char** opts = NULL;

        const char* audio_dir = NULL;
        clk_json_value* v = clk_json_object_get(cfg->json, "audio_dir");
        if (v && clk_json_is_string(v))
            clk_json_get_string(v, &audio_dir);
        if (audio_dir) {
            paths = clk_fs_scan_dir(audio_dir, ".mp3", &sound_cnt);
            if (paths) {
                names = clk_menu_build_names(paths, sound_cnt);
                opts = clk_menu_wrap_strings(names, sound_cnt);
            }
        }

        int sound_idx = 0;
        if (sound_cnt > 0 && cfg->bgm.count > 0)
            sound_idx = clk_menu_find_index(cfg->bgm.items[0].sound_file, opts, sound_cnt, 0);

        clk_menu_add_item_str(menu, CLK_TAB_BASIC, CLK_BASIC_ITEM_BGM_SOUND, "BGM sound", sound_idx,
                              sound_cnt > 0 ? opts : (const char*[]){"(none)"},
                              sound_cnt > 0 ? sound_cnt : 1);

        free(opts);
        if (names) {
            for (int i = 0; i < sound_cnt; ++i)
                free(names[i]);
            free(names);
        }
        if (paths) {
            for (int i = 0; i < sound_cnt; ++i)
                free(paths[i]);
            free(paths);
        }
    }

    /* ---- quit ---- */
    clk_menu_add_item_action(menu, CLK_TAB_BASIC, CLK_BASIC_ITEM_QUIT, "quit");
}

/* ================================================================
 *  Menu
 * ================================================================ */

clk_menu* clk_app_setup_menu(const clk_app_config* cfg, clk_clock* clock) {
    clk_menu* menu = clk_menu_create();
    if (!menu)
        return NULL;

    register_basic_tab(cfg, menu);
    clk_setup_scan_and_register_tabs(menu, clock, cfg);
    return menu;
}

/* ================================================================
 *  Menu rebuild (after add/delete)
 * ================================================================ */

void clk_app_menu_rebuild(clk_menu* menu, clk_clock* clock, const clk_app_config* cfg) {
    if (!menu || !clock || !cfg)
        return;
    clk_setup_scan_and_register_tabs(menu, clock, cfg);
}

/* ================================================================
 *  Render & theme
 * ================================================================ */

bool clk_app_setup_render(clk_ascii_render* render, const clk_cfg_ascii_clock_theme* ascii_clock) {
    if (!render || !ascii_clock)
        return false;
    if (ascii_clock->fonts.index < 0 || ascii_clock->fonts.index >= ascii_clock->fonts.count)
        return false;

    if (!clk_ascii_render_create(render, ascii_clock->fonts.paths[ascii_clock->fonts.index]))
        return false;

    clk_ascii_render_set_z_order(render, 0);
    clk_ascii_render_add_to_term(render);
    return true;
}

bool clk_app_setup_theme(clk_menu_theme* theme, const clk_cfg_themes* themes) {
    if (!theme || !themes)
        return false;
    if (themes->index < 0 || themes->index >= themes->count)
        return false;

    memset(theme, 0, sizeof(*theme));
    return clk_menu_theme_load(themes->paths[themes->index], theme);
}

/* ================================================================
 *  Clock
 * ================================================================ */

static void clk_setup_load_sound(clk_audio_sound** sound, const char* audio_dir,
                                 const char* sound_file, clk_audio_engine* engine) {
    if (audio_dir && sound_file && sound_file[0] != '\0') {
        char sound_path[CLK_SOUND_PATH_MAX];
        snprintf(sound_path, sizeof(sound_path), "%s/%s.mp3", audio_dir, sound_file);
        *sound = clk_audio_load(engine, sound_path);
    }
}

static void clk_setup_scan_and_register_tabs(clk_menu* menu, clk_clock* clock,
                                             const clk_app_config* cfg) {
    char** sound_paths = NULL;
    int sound_count = 0;
    char** sound_names = NULL;
    const char** sound_opts = NULL;

    scan_sound_options(cfg, &sound_paths, &sound_count, &sound_names, &sound_opts);

    const char** use_opts = sound_opts;
    int use_count = sound_count;
    const char* fallback[] = {"(none)"};
    if (use_count == 0) {
        use_opts = fallback;
        use_count = 1;
    }

    register_alarm_tab(menu, clock, use_opts, use_count);
    register_pomodoro_tab(menu, clock, use_opts, use_count);

    if (sound_count > 0) {
        free(sound_opts);
        for (int i = 0; i < sound_count; ++i)
            free(sound_names[i]);
        free(sound_names);
        for (int i = 0; i < sound_count; ++i)
            free(sound_paths[i]);
        free(sound_paths);
    }
}

bool clk_app_setup_clock(clk_clock* clock, clk_audio_engine** out_engine,
                         const clk_app_config* cfg) {
    if (!clock || !out_engine || !cfg)
        return false;

    if (!clk_audio_init(out_engine))
        return false;

    clk_clock_init(clock, *out_engine);

    const char* audio_dir = NULL;
    clk_json_value* audio_dir_val = clk_json_object_get(cfg->json, "audio_dir");
    if (audio_dir_val && clk_json_is_string(audio_dir_val))
        clk_json_get_string(audio_dir_val, &audio_dir);

    for (int i = 0; i < cfg->clock.alarms.count; ++i) {
        const clk_cfg_alarm* src = &cfg->clock.alarms.items[i];
        clk_clock_alarm alarm;
        memset(&alarm, 0, sizeof(alarm));

        alarm.id = i;
        strncpy(alarm.name, src->name, CLK_CLOCK_NAME_MAX - 1);
        clk_alarm_set(&alarm.alarm, src->hour, src->minute, 0);
        alarm.alarm.enabled = src->enabled;
        alarm.repeat_count = src->sound_repeat;
        alarm.repeat_days = src->repeat_days;
        alarm.today_date = src->today_date;
        alarm.volume = (float)src->volume / 100.0f;
        alarm.loop = src->loop;

        clk_setup_load_sound(&alarm.sound, audio_dir, src->sound_file, *out_engine);

        clk_clock_add_alarm(clock, &alarm);
    }

    for (int i = 0; i < cfg->clock.pomodoros.count; ++i) {
        const clk_cfg_pomodoro* src = &cfg->clock.pomodoros.items[i];
        clk_clock_pomodoro pomodoro;
        memset(&pomodoro, 0, sizeof(pomodoro));

        pomodoro.id = i;
        strncpy(pomodoro.name, src->name, CLK_CLOCK_NAME_MAX - 1);
        pomodoro.current_segment = -1;

        int pomo_index = clock->pomodoro_count;
        clk_clock_add_pomodoro(clock, &pomodoro);

        for (int j = 0; j < src->segment_count; ++j) {
            const clk_cfg_pomodoro_segment* src_seg = &src->segments[j];
            clk_clock_pomodoro_segment seg;
            memset(&seg, 0, sizeof(seg));

            seg.id = j;
            strncpy(seg.name, src_seg->name, CLK_CLOCK_NAME_MAX - 1);
            seg.duration_seconds = src_seg->duration_seconds;
            seg.repeat_count = src_seg->sound_repeat;
            seg.volume = (float)src_seg->volume / 100.0f;
            seg.loop = src_seg->loop;

            clk_setup_load_sound(&seg.sound, audio_dir, src_seg->sound_file, *out_engine);

            clk_clock_pomodoro_add_segment(clock, pomo_index, &seg);
        }
    }

    return true;
}

void clk_app_setup_clock_deinit(clk_clock* clock, clk_audio_engine* engine) {
    if (!clock && !engine)
        return;

    if (clock) {
        clk_clock_deinit(clock);

        for (int i = 0; i < clock->alarm_count; ++i)
            clk_audio_destroy(clock->alarms[i].sound);

        for (int i = 0; i < clock->pomodoro_count; ++i)
            for (int j = 0; j < clock->pomodoros[i].segment_count; ++j)
                clk_audio_destroy(clock->pomodoros[i].segments[j].sound);
    }

    clk_audio_shutdown(engine);
}

void clk_app_setup_bgm(clk_bgm* bgm, clk_audio_engine* engine, const clk_app_config* cfg) {
    clk_bgm_init(bgm, engine);
    if (cfg->bgm.count > 0) {
        const clk_cfg_bgm* src = &cfg->bgm.items[0];
        bgm->enabled = src->enabled;
        bgm->volume = src->volume;
        strncpy(bgm->sound_file, src->sound_file, sizeof(bgm->sound_file) - 1);

        const char* audio_dir = NULL;
        clk_json_value* v = clk_json_object_get(cfg->json, "audio_dir");
        if (v && clk_json_is_string(v))
            clk_json_get_string(v, &audio_dir);
        if (audio_dir && src->sound_file[0]) {
            char path[CLK_SOUND_PATH_MAX];
            snprintf(path, sizeof(path), "%s/%s.mp3", audio_dir, src->sound_file);
            if (clk_bgm_load_sound(bgm, path) && src->enabled)
                clk_bgm_set_enabled(bgm, true);
        }
    }
}

/* ================================================================
 *  Clock diff update (hot-reload)
 * ================================================================ */

static void diff_update_alarms(clk_clock* clock, clk_audio_engine* engine,
                               const clk_app_config* cfg, const char* audio_dir) {
    for (int i = clock->alarm_count - 1; i >= 0; --i) {
        const clk_clock_alarm* a = &clock->alarms[i];
        bool found = false;
        for (int j = 0; j < cfg->clock.alarms.count; ++j) {
            if (strcmp(a->name, cfg->clock.alarms.items[j].name) == 0) {
                found = true;
                break;
            }
        }
        if (!found) {
            clk_audio_destroy(clock->alarms[i].sound);
            clk_clock_remove_alarm(clock, i);
        }
    }

    for (int j = 0; j < cfg->clock.alarms.count; ++j) {
        const clk_cfg_alarm* src = &cfg->clock.alarms.items[j];
        clk_clock_alarm* a = clk_clock_find_alarm_by_name(clock, src->name);

        if (a) {
            bool saved_triggered = a->alarm.triggered;
            if (a->alarm.hour != src->hour || a->alarm.minute != src->minute)
                saved_triggered = false;

            clk_alarm_set(&a->alarm, src->hour, src->minute, 0);
            a->alarm.enabled = src->enabled;
            a->alarm.triggered = saved_triggered;
            a->repeat_count = src->sound_repeat;
            a->repeat_days = src->repeat_days;
            a->today_date = src->today_date;
            a->volume = (float)src->volume / 100.0f;
            a->loop = src->loop;

            char new_path[CLK_SOUND_PATH_MAX];
            new_path[0] = '\0';
            if (audio_dir && src->sound_file[0] != '\0')
                snprintf(new_path, sizeof(new_path), "%s/%s.mp3", audio_dir, src->sound_file);

            const char* old_path = clk_audio_sound_get_path(a->sound);
            if (!old_path)
                old_path = "";

            if (strcmp(old_path, new_path) != 0) {
                clk_audio_destroy(a->sound);
                a->sound = new_path[0] ? clk_audio_load(engine, new_path) : NULL;
            }
        } else {
            clk_clock_alarm alarm;
            memset(&alarm, 0, sizeof(alarm));

            alarm.id = clk_clock_next_alarm_id(clock);
            strncpy(alarm.name, src->name, CLK_CLOCK_NAME_MAX - 1);
            clk_alarm_set(&alarm.alarm, src->hour, src->minute, 0);
            alarm.alarm.enabled = src->enabled;
            alarm.repeat_count = src->sound_repeat;
            alarm.repeat_days = src->repeat_days;
            alarm.today_date = src->today_date;
            alarm.volume = (float)src->volume / 100.0f;
            alarm.loop = src->loop;

            if (audio_dir && src->sound_file[0] != '\0') {
                char full_path[CLK_SOUND_PATH_MAX];
                snprintf(full_path, sizeof(full_path), "%s/%s.mp3", audio_dir, src->sound_file);
                alarm.sound = clk_audio_load(engine, full_path);
            }

            clk_clock_add_alarm(clock, &alarm);
        }
    }
}

static void diff_update_pomodoros(clk_clock* clock, clk_audio_engine* engine,
                                  const clk_app_config* cfg, const char* audio_dir) {
    for (int i = clock->pomodoro_count - 1; i >= 0; --i) {
        const clk_clock_pomodoro* po = &clock->pomodoros[i];
        bool found = false;
        for (int j = 0; j < cfg->clock.pomodoros.count; ++j) {
            if (strcmp(po->name, cfg->clock.pomodoros.items[j].name) == 0) {
                found = true;
                break;
            }
        }
        if (!found) {
            for (int j = 0; j < clock->pomodoros[i].segment_count; ++j)
                clk_audio_destroy(clock->pomodoros[i].segments[j].sound);
            clk_clock_remove_pomodoro(clock, i);
        }
    }

    for (int j = 0; j < cfg->clock.pomodoros.count; ++j) {
        const clk_cfg_pomodoro* src = &cfg->clock.pomodoros.items[j];
        clk_clock_pomodoro* po = clk_clock_find_pomodoro_by_name(clock, src->name);

        if (po) {
            int po_idx = clk_clock_find_pomodoro_index_by_id(clock, po->id);

            strncpy(po->name, src->name, CLK_CLOCK_NAME_MAX - 1);

            bool segs_same = (po->segment_count == src->segment_count);
            if (segs_same) {
                for (int k = 0; k < src->segment_count; ++k) {
                    if (strcmp(po->segments[k].name, src->segments[k].name) != 0 ||
                        po->segments[k].duration_seconds != src->segments[k].duration_seconds) {
                        segs_same = false;
                        break;
                    }
                }
            }

            if (segs_same) {
                for (int k = 0; k < po->segment_count; ++k) {
                    clk_clock_pomodoro_segment* seg = &po->segments[k];
                    const clk_cfg_pomodoro_segment* src_seg = &src->segments[k];

                    seg->repeat_count = src_seg->sound_repeat;
                    seg->volume = (float)src_seg->volume / 100.0f;
                    seg->loop = src_seg->loop;

                    char new_path[CLK_SOUND_PATH_MAX];
                    new_path[0] = '\0';
                    if (audio_dir && src_seg->sound_file[0] != '\0')
                        snprintf(new_path, sizeof(new_path), "%s/%s.mp3", audio_dir,
                                 src_seg->sound_file);

                    const char* old_path = clk_audio_sound_get_path(seg->sound);
                    if (!old_path)
                        old_path = "";

                    if (strcmp(old_path, new_path) != 0) {
                        clk_audio_destroy(seg->sound);
                        seg->sound = new_path[0] ? clk_audio_load(engine, new_path) : NULL;
                    }
                }
            } else {
                clk_clock_pomodoro_clear_segments(clock, po_idx);
                for (int k = 0; k < src->segment_count; ++k) {
                    const clk_cfg_pomodoro_segment* src_seg = &src->segments[k];
                    clk_clock_pomodoro_segment seg;
                    memset(&seg, 0, sizeof(seg));

                    seg.id = k;
                    strncpy(seg.name, src_seg->name, CLK_CLOCK_NAME_MAX - 1);
                    seg.duration_seconds = src_seg->duration_seconds;
                    seg.repeat_count = src_seg->sound_repeat;
                    seg.volume = (float)src_seg->volume / 100.0f;
                    seg.loop = src_seg->loop;

                    if (audio_dir && src_seg->sound_file[0] != '\0') {
                        char full_path[CLK_SOUND_PATH_MAX];
                        snprintf(full_path, sizeof(full_path), "%s/%s.mp3", audio_dir,
                                 src_seg->sound_file);
                        seg.sound = clk_audio_load(engine, full_path);
                    }

                    clk_clock_pomodoro_add_segment(clock, po_idx, &seg);
                }
            }
        } else {
            clk_clock_pomodoro pomodoro;
            memset(&pomodoro, 0, sizeof(pomodoro));

            pomodoro.id = clk_clock_next_pomodoro_id(clock);
            pomodoro.current_segment = -1;
            strncpy(pomodoro.name, src->name, CLK_CLOCK_NAME_MAX - 1);

            int pomo_idx = clock->pomodoro_count;
            clk_clock_add_pomodoro(clock, &pomodoro);

            for (int k = 0; k < src->segment_count; ++k) {
                const clk_cfg_pomodoro_segment* src_seg = &src->segments[k];
                clk_clock_pomodoro_segment seg;
                memset(&seg, 0, sizeof(seg));

                seg.id = k;
                strncpy(seg.name, src_seg->name, CLK_CLOCK_NAME_MAX - 1);
                seg.duration_seconds = src_seg->duration_seconds;
                seg.repeat_count = src_seg->sound_repeat;
                seg.volume = (float)src_seg->volume / 100.0f;
                seg.loop = src_seg->loop;

                if (audio_dir && src_seg->sound_file[0] != '\0') {
                    char full_path[CLK_SOUND_PATH_MAX];
                    snprintf(full_path, sizeof(full_path), "%s/%s.mp3", audio_dir,
                             src_seg->sound_file);
                    seg.sound = clk_audio_load(engine, full_path);
                }

                clk_clock_pomodoro_add_segment(clock, pomo_idx, &seg);
            }
        }
    }
}

void clk_app_clock_diff_update(clk_clock* clock, clk_audio_engine* engine,
                               const clk_app_config* cfg) {
    if (!clock || !engine || !cfg)
        return;

    clk_clock_stop_all_bells(clock);

    const char* audio_dir = NULL;
    clk_json_value* audio_dir_val = clk_json_object_get(cfg->json, "audio_dir");
    if (audio_dir_val && clk_json_is_string(audio_dir_val))
        clk_json_get_string(audio_dir_val, &audio_dir);

    diff_update_alarms(clock, engine, cfg, audio_dir);
    diff_update_pomodoros(clock, engine, cfg, audio_dir);
}

/* ================================================================
 *  Internal helpers — sound path utilities
 * ================================================================ */

static void scan_sound_options(const clk_app_config* cfg, char*** out_paths, int* out_count,
                               char*** out_names, const char*** out_opts) {
    *out_paths = NULL;
    *out_names = NULL;
    *out_opts = NULL;
    *out_count = 0;

    const char* audio_dir = NULL;
    clk_json_value* v = clk_json_object_get(cfg->json, "audio_dir");
    if (v && clk_json_is_string(v))
        clk_json_get_string(v, &audio_dir);
    if (!audio_dir)
        return;

    char** paths = clk_fs_scan_dir(audio_dir, ".mp3", out_count);
    if (!paths || *out_count == 0) {
        clk_fs_free_list(paths, *out_count);
        *out_count = 0;
        return;
    }

    *out_paths = paths;
    *out_names = clk_menu_build_names(paths, *out_count);
    *out_opts = clk_menu_wrap_strings(*out_names, *out_count);
}

/* ================================================================
 *  Alarm items
 * ================================================================ */

static void register_alarm_tab(clk_menu* menu, clk_clock* clock, const char** sound_opts,
                               int sound_count) {
    clk_menu_tab* tab = clk_tab_list_find(menu->tab_list, CLK_TAB_ALARM);
    if (!tab)
        clk_menu_add_tab(menu, CLK_TAB_ALARM, "alarm");

    clk_item_list* list = clk_item_list_create();

    for (int i = 0; i < clock->alarm_count; ++i) {
        clk_clock_alarm* a = &clock->alarms[i];
        int base = a->id * CLK_ALARM_ITEM_STRIDE;

        int sound_idx = 0;
        if (a->sound && sound_count > 0) {
            const char* full_path = clk_audio_sound_get_path(a->sound);
            char basename[CLK_CONFIG_ALARM_SOUND_MAX];
            clk_app_config_sound_basename(full_path, basename, sizeof(basename));
            sound_idx = clk_menu_find_index(basename, sound_opts, sound_count, 0);
        }

        clk_item_list_add_action(list, CLK_TAB_ALARM, base + CLK_ALARM_HEADER_OFFSET, a->name);
        clk_item_list_add_bool(list, CLK_TAB_ALARM, base + CLK_ALARM_ENABLED_OFFSET, "enabled",
                               a->alarm.enabled);
        clk_item_list_add_int(list, CLK_TAB_ALARM, base + CLK_ALARM_HOUR_OFFSET, "hour",
                              a->alarm.hour, 0, 23, 1);
        clk_item_list_add_int(list, CLK_TAB_ALARM, base + CLK_ALARM_MINUTE_OFFSET, "minute",
                              a->alarm.minute, 0, 59, 1);
        clk_item_list_add_str(list, CLK_TAB_ALARM, base + CLK_ALARM_REPEAT_OFFSET, "repeat",
                              (int)a->repeat_days, clk_repeat_day_options,
                              CLK_REPEAT_DAY_OPTION_COUNT);
        clk_item_list_add_bool(list, CLK_TAB_ALARM, base + CLK_ALARM_LOOP_OFFSET, "loop", a->loop);
        clk_item_list_add_int(list, CLK_TAB_ALARM, base + CLK_ALARM_REPEAT_COUNT, "repeat count",
                              a->repeat_count, 1, 99, 1);
        clk_item_list_add_int(list, CLK_TAB_ALARM, base + CLK_ALARM_VOLUME_OFFSET, "volume",
                              (int)lroundf(a->volume * 100.0f), 0, 100, 5);
        clk_item_list_add_str(list, CLK_TAB_ALARM, base + CLK_ALARM_SOUND_OFFSET, "sound",
                              sound_idx, sound_opts, sound_count);
        clk_item_list_add_action(list, CLK_TAB_ALARM, base + CLK_ALARM_ADD_OFFSET, "add alarm");
        clk_item_list_add_action(list, CLK_TAB_ALARM, base + CLK_ALARM_DELETE_OFFSET,
                                 "delete alarm");
    }

    if (clock->alarm_count == 0) {
        int base = 0 * CLK_ALARM_ITEM_STRIDE;
        clk_item_list_add_action(list, CLK_TAB_ALARM, base + CLK_ALARM_ADD_OFFSET, "add alarm");
    }

    clk_tab_list_set_item_list(menu->tab_list, CLK_TAB_ALARM, list);
}

/* ================================================================
 *  Pomodoro items
 * ================================================================ */

static void register_pomodoro_tab(clk_menu* menu, clk_clock* clock, const char** sound_opts,
                                  int sound_count) {
    clk_menu_tab* tab = clk_tab_list_find(menu->tab_list, CLK_TAB_POMODORO);
    if (!tab)
        clk_menu_add_tab(menu, CLK_TAB_POMODORO, "pomodoro");

    clk_item_list* list = clk_item_list_create();

    for (int i = 0; i < clock->pomodoro_count; ++i) {
        clk_clock_pomodoro* po = &clock->pomodoros[i];
        int base = po->id * CLK_POMO_STRIDE;

        clk_item_list_add_action(list, CLK_TAB_POMODORO, base + CLK_POMO_HEADER_OFFSET, po->name);
        clk_item_list_add_bool(list, CLK_TAB_POMODORO, base + CLK_POMO_ENABLED_OFFSET, "enabled",
                               po->enabled);

        for (int j = 0; j < po->segment_count; ++j) {
            clk_clock_pomodoro_segment* seg = &po->segments[j];
            int seg_base = base + CLK_POMO_SEGMENT_BASE + seg->id * CLK_POMO_SEG_STRIDE;

            int sound_idx = 0;
            if (seg->sound && sound_count > 0) {
                const char* full_path = clk_audio_sound_get_path(seg->sound);
                char basename[CLK_CONFIG_ALARM_SOUND_MAX];
                clk_app_config_sound_basename(full_path, basename, sizeof(basename));
                sound_idx = clk_menu_find_index(basename, sound_opts, sound_count, 0);
            }

            clk_item_list_add_action(list, CLK_TAB_POMODORO, seg_base + CLK_POMO_SEG_HEADER_OFFSET,
                                     seg->name);
            clk_item_list_add_int(list, CLK_TAB_POMODORO, seg_base + CLK_POMO_SEG_DURATION_OFFSET,
                                  "duration (m)", seg->duration_seconds / 60.0, 1, 999, 1);
            clk_item_list_add_int(list, CLK_TAB_POMODORO, seg_base + CLK_POMO_SEG_REPEAT_OFFSET,
                                  "repeat", seg->repeat_count, 1, 99, 1);
            clk_item_list_add_int(list, CLK_TAB_POMODORO, seg_base + CLK_POMO_SEG_VOLUME_OFFSET,
                                  "volume", (int)lroundf(seg->volume * 100.0f), 0, 100, 5);
            clk_item_list_add_str(list, CLK_TAB_POMODORO, seg_base + CLK_POMO_SEG_SOUND_OFFSET,
                                  "sound", sound_idx, sound_opts, sound_count);
            clk_item_list_add_action(list, CLK_TAB_POMODORO, seg_base + CLK_POMO_SEG_ADD_OFFSET,
                                     "add segment");
            clk_item_list_add_action(list, CLK_TAB_POMODORO, seg_base + CLK_POMO_SEG_DELETE_OFFSET,
                                     "delete segment");
        }

        if (po->segment_count == 0) {
            int seg_base = base + CLK_POMO_SEGMENT_BASE + 0 * CLK_POMO_SEG_STRIDE;
            clk_item_list_add_action(list, CLK_TAB_POMODORO, seg_base + CLK_POMO_SEG_ADD_OFFSET,
                                     "add segment");
        }

        clk_item_list_add_action(list, CLK_TAB_POMODORO, base + CLK_POMO_ADD_OFFSET,
                                 "add pomodoro");
        clk_item_list_add_action(list, CLK_TAB_POMODORO, base + CLK_POMO_DELETE_OFFSET,
                                 "delete pomodoro");
    }

    if (clock->pomodoro_count == 0) {
        int base = 0 * CLK_POMO_STRIDE;
        clk_item_list_add_action(list, CLK_TAB_POMODORO, base + CLK_POMO_ADD_OFFSET,
                                 "add pomodoro");
    }

    clk_tab_list_set_item_list(menu->tab_list, CLK_TAB_POMODORO, list);
}
