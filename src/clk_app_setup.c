#include "clk_app_setup.h"

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

/** Register the "basic" tab items (time format, font, theme, quit). */
static void register_basic_tab(const clk_app_config* cfg, clk_menu* menu) {
    clk_menu_add_tab(menu, CLK_TAB_BASIC, "basic");

    /* ---- time format ---- */
    clk_menu_add_item_str(menu, CLK_TAB_BASIC, CLK_BASIC_ITEM_TIME_FORMAT, "time format",
                          cfg->ascii_clock.time_formats.index,
                          cfg->ascii_clock.time_formats.options,
                          cfg->ascii_clock.time_formats.count);

    /* ---- font ---- */
    clk_menu_add_item_str(menu, CLK_TAB_BASIC, CLK_BASIC_ITEM_FONT, "font",
                          cfg->ascii_clock.fonts.index, (const char**)cfg->ascii_clock.fonts.names,
                          cfg->ascii_clock.fonts.count);

    /* ---- menu theme ---- */
    clk_menu_add_item_str(menu, CLK_TAB_BASIC, CLK_BASIC_ITEM_THEME, "menu theme",
                          cfg->themes.index, (const char**)cfg->themes.names, cfg->themes.count);

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

    return menu;
}

/* ================================================================
 *  Menu rebuild (after add/delete)
 * ================================================================ */

void clk_app_menu_rebuild(clk_menu* menu, clk_clock* clock, const clk_app_config* cfg) {
    if (!menu || !clock || !cfg)
        return;

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

#define CLK_SOUND_PATH_MAX (CLK_CONFIG_ALARM_SOUND_MAX + 512)

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
        alarm.volume = src->volume / 100.0f;
        alarm.loop = src->loop;

        if (audio_dir && src->sound_file[0] != '\0') {
            char sound_path[CLK_SOUND_PATH_MAX];
            snprintf(sound_path, sizeof(sound_path), "%s/%s.mp3", audio_dir, src->sound_file);
            alarm.sound = clk_audio_load(*out_engine, sound_path);
        }

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
            seg.volume = src_seg->volume / 100.0f;
            seg.loop = src_seg->loop;

            if (audio_dir && src_seg->sound_file[0] != '\0') {
                char sound_path[CLK_SOUND_PATH_MAX];
                snprintf(sound_path, sizeof(sound_path), "%s/%s.mp3", audio_dir,
                         src_seg->sound_file);
                seg.sound = clk_audio_load(*out_engine, sound_path);
            }

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
                              (int)(a->volume * 100.0f + 0.5f), 0, 100, 5);
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
                                  "duration (m)", seg->duration_seconds / 60.0, 1, 999, 5);
            clk_item_list_add_int(list, CLK_TAB_POMODORO, seg_base + CLK_POMO_SEG_REPEAT_OFFSET,
                                  "repeat", seg->repeat_count, 1, 99, 1);
            clk_item_list_add_int(list, CLK_TAB_POMODORO, seg_base + CLK_POMO_SEG_VOLUME_OFFSET,
                                  "volume", (int)(seg->volume * 100.0f + 0.5f), 0, 100, 5);
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
