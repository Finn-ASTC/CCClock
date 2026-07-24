#include "clk_app_config.h"

#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "clk_file_util.h"
#include "clk_fs_watch.h"
#include "clk_json.h"
#include "clk_menu.h"

/* ------------------------------------------------------------------
 *  Internal helpers
 * ------------------------------------------------------------------ */

static const char* json_get_string(clk_json_value* obj, const char* key) {
    clk_json_value* v = clk_json_object_get(obj, key);
    const char* str = NULL;
    if (v && clk_json_is_string(v) && clk_json_get_string(v, &str) == 0)
        return str;
    return NULL;
}

static double json_get_number_or_default(clk_json_value* obj, const char* key, double fallback) {
    clk_json_value* v = clk_json_object_get(obj, key);
    double num = fallback;
    if (v && clk_json_is_number(v) && clk_json_get_number(v, &num) == 0)
        return num;
    return fallback;
}

/** Parse a JSON string array into a char**.
 *  Returned strings borrow pointers from the JSON tree — do NOT free
 *  them individually; the JSON must outlive the returned array. */
static char** parse_time_formats_array(clk_json_value* time_obj, int* out_count) {
    *out_count = 0;
    clk_json_value* array = clk_json_object_get(time_obj, "time_formats");
    if (!array || !clk_json_is_array(array))
        return NULL;
    int count = (int)clk_json_array_count(array);
    if (count <= 0)
        return NULL;
    char** formats = calloc((size_t)count, sizeof(char*));
    if (!formats)
        return NULL;
    for (int i = 0; i < count; ++i) {
        clk_json_value* element = clk_json_array_get(array, (size_t)i);
        const char* str = NULL;
        if (element && clk_json_is_string(element) && clk_json_get_string(element, &str) == 0)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-qual"
            formats[i] = (char*)str;
#pragma GCC diagnostic pop
    }
    *out_count = count;
    return formats;
}

static void copy_time_format(char* dst, const char* src) {
    if (!src)
        return;
    size_t len = strlen(src);
    if (len >= CLK_CLOCK_FORMAT_MAX_LENGTH)
        len = CLK_CLOCK_FORMAT_MAX_LENGTH - 1;
    memcpy(dst, src, len);
    dst[len] = '\0';
}

/* ================================================================
 *  Fonts
 * ================================================================ */

static void clk_cfg_fonts_init(clk_cfg_fonts* fonts, clk_json_value* fonts_obj) {
    memset(fonts, 0, sizeof(*fonts));

    fonts->dir = json_get_string(fonts_obj, "fonts_dir");
    if (!fonts->dir)
        return;

    fonts->paths = clk_fs_scan_dir(fonts->dir, ".json", &fonts->count);
    if (!fonts->paths)
        return;

    fonts->names = clk_menu_build_names(fonts->paths, fonts->count);
    if (!fonts->names)
        return;

    const char* saved = json_get_string(fonts_obj, "font");
    if (saved)
        fonts->index =
            clk_menu_find_index(saved, (const char* const*)fonts->names, fonts->count, 0);
}

static void clk_cfg_fonts_reload(clk_cfg_fonts* fonts, clk_json_value* fonts_obj, clk_menu* menu,
                                 int tab_id, int item_id) {
    fonts->dir = json_get_string(fonts_obj, "fonts_dir");
    const char* saved = json_get_string(fonts_obj, "font");
    if (saved)
        fonts->index = clk_menu_find_index(saved, (const char* const*)fonts->names, fonts->count,
                                           fonts->index);
    clk_menu_rebuild_item(menu, tab_id, item_id, (const char* const*)fonts->names, fonts->count,
                          fonts->index);
}

static void clk_cfg_fonts_sync(clk_cfg_fonts* fonts, clk_menu* menu, int tab_id, int item_id) {
    clk_fs_sync_dir(fonts->dir, &fonts->paths, &fonts->count, &fonts->names, &fonts->index, menu,
                    tab_id, item_id);
}

static void clk_cfg_fonts_deinit(clk_cfg_fonts* fonts) {
    for (int i = 0; i < fonts->count; ++i) {
        free(fonts->paths[i]);
        free(fonts->names[i]);
    }
    free(fonts->paths);
    free(fonts->names);
    memset(fonts, 0, sizeof(*fonts));
}

/* ================================================================
 *  Themes
 * ================================================================ */

void clk_cfg_themes_init(clk_cfg_themes* themes, clk_json_value* menu_obj) {
    memset(themes, 0, sizeof(*themes));

    themes->dir = json_get_string(menu_obj, "themes_dir");
    if (!themes->dir)
        return;

    themes->paths = clk_fs_scan_dir(themes->dir, ".json", &themes->count);
    if (!themes->paths)
        return;

    themes->names = clk_menu_build_names(themes->paths, themes->count);
    if (!themes->names)
        return;

    const char* saved = json_get_string(menu_obj, "theme");
    if (saved)
        themes->index =
            clk_menu_find_index(saved, (const char* const*)themes->names, (int)themes->count, 0);
}

void clk_cfg_themes_reload(clk_cfg_themes* themes, clk_json_value* menu_obj, clk_menu* menu,
                           int tab_id, int item_id) {
    themes->dir = json_get_string(menu_obj, "themes_dir");
    const char* saved = json_get_string(menu_obj, "theme");
    if (saved)
        themes->index = clk_menu_find_index(saved, (const char* const*)themes->names,
                                            (int)themes->count, themes->index);
    clk_menu_rebuild_item(menu, tab_id, item_id, (const char* const*)themes->names,
                          (int)themes->count, themes->index);
}

void clk_cfg_themes_sync(clk_cfg_themes* themes, clk_menu* menu, int tab_id, int item_id) {
    clk_fs_sync_dir(themes->dir, &themes->paths, &themes->count, &themes->names, &themes->index,
                    menu, tab_id, item_id);
}

void clk_cfg_themes_deinit(clk_cfg_themes* themes) {
    for (int i = 0; i < themes->count; ++i) {
        free(themes->paths[i]);
        free(themes->names[i]);
    }
    free(themes->paths);
    free(themes->names);
    memset(themes, 0, sizeof(*themes));
}

/* ================================================================
 *  Time formats
 * ================================================================ */

static void clk_cfg_time_formats_init(clk_cfg_time_formats* time_fmts, clk_json_value* time_obj) {
    memset(time_fmts, 0, sizeof(*time_fmts));

    time_fmts->strings = parse_time_formats_array(time_obj, &time_fmts->count);
    if (!time_fmts->strings)
        return;

    time_fmts->options = clk_menu_wrap_strings(time_fmts->strings, time_fmts->count);

    const char* saved = json_get_string(time_obj, "selected_time_format");
    if (saved)
        time_fmts->index = clk_menu_find_index(saved, time_fmts->options, time_fmts->count, 0);

    copy_time_format(time_fmts->current, time_fmts->strings[time_fmts->index]);
}

static void clk_cfg_time_formats_reload(clk_cfg_time_formats* time_fmts, clk_json_value* time_obj,
                                        clk_menu* menu, int tab_id, int item_id) {
    free(time_fmts->strings);
    free(time_fmts->options);

    time_fmts->strings = parse_time_formats_array(time_obj, &time_fmts->count);
    time_fmts->options = clk_menu_wrap_strings(time_fmts->strings, time_fmts->count);

    const char* saved = json_get_string(time_obj, "selected_time_format");
    if (saved)
        time_fmts->index = clk_menu_find_index(saved, time_fmts->options, time_fmts->count, 0);

    clk_menu_rebuild_item(menu, tab_id, item_id, time_fmts->options, time_fmts->count,
                          time_fmts->index);
}

static void clk_cfg_time_formats_switch(clk_cfg_time_formats* time_fmts) {
    copy_time_format(time_fmts->current, time_fmts->strings[time_fmts->index]);
}

static void clk_cfg_time_formats_deinit(clk_cfg_time_formats* time_fmts) {
    free(time_fmts->strings);
    free(time_fmts->options);
    memset(time_fmts, 0, sizeof(*time_fmts));
}

/* ================================================================
 *  Ascii Clock Theme (fonts + time formats wrapper)
 * ================================================================ */

void clk_cfg_ascii_clock_theme_init(clk_cfg_ascii_clock_theme* ascii_clock,
                                    clk_json_value* theme_obj) {
    memset(ascii_clock, 0, sizeof(*ascii_clock));

    clk_json_value* fonts_obj = clk_json_object_get(theme_obj, "fonts");
    clk_json_value* time_obj = clk_json_object_get(theme_obj, "time_format");
    if (fonts_obj)
        clk_cfg_fonts_init(&ascii_clock->fonts, fonts_obj);
    if (time_obj)
        clk_cfg_time_formats_init(&ascii_clock->time_formats, time_obj);
}

void clk_cfg_ascii_clock_theme_reload(clk_cfg_ascii_clock_theme* ascii_clock,
                                      clk_json_value* theme_obj, clk_menu* menu, int tab_id,
                                      int font_id, int tfmt_id) {
    clk_json_value* fonts_obj = clk_json_object_get(theme_obj, "fonts");
    clk_json_value* time_obj = clk_json_object_get(theme_obj, "time_format");
    if (fonts_obj)
        clk_cfg_fonts_reload(&ascii_clock->fonts, fonts_obj, menu, tab_id, font_id);
    if (time_obj)
        clk_cfg_time_formats_reload(&ascii_clock->time_formats, time_obj, menu, tab_id, tfmt_id);
}

void clk_cfg_ascii_clock_theme_sync_fonts(clk_cfg_ascii_clock_theme* ascii_clock, clk_menu* menu,
                                          int tab_id, int item_id) {
    clk_cfg_fonts_sync(&ascii_clock->fonts, menu, tab_id, item_id);
}

void clk_cfg_ascii_clock_theme_switch_time(clk_cfg_ascii_clock_theme* ascii_clock) {
    clk_cfg_time_formats_switch(&ascii_clock->time_formats);
}

void clk_cfg_ascii_clock_theme_deinit(clk_cfg_ascii_clock_theme* ascii_clock) {
    clk_cfg_fonts_deinit(&ascii_clock->fonts);
    clk_cfg_time_formats_deinit(&ascii_clock->time_formats);
    memset(ascii_clock, 0, sizeof(*ascii_clock));
}

/* ================================================================
 *  Alarms
 * ================================================================ */

clk_repeat_days clk_repeat_days_from_string(const char* str) {
    if (!str)
        return CLK_REPEAT_TODAY;
    if (strcmp(str, "Everyday") == 0)
        return CLK_REPEAT_EVERYDAY;
    if (strcmp(str, "Today") == 0)
        return CLK_REPEAT_TODAY;
    if (strcmp(str, "Monday") == 0)
        return CLK_REPEAT_MONDAY;
    if (strcmp(str, "Tuesday") == 0)
        return CLK_REPEAT_TUESDAY;
    if (strcmp(str, "Wednesday") == 0)
        return CLK_REPEAT_WEDNESDAY;
    if (strcmp(str, "Thursday") == 0)
        return CLK_REPEAT_THURSDAY;
    if (strcmp(str, "Friday") == 0)
        return CLK_REPEAT_FRIDAY;
    if (strcmp(str, "Saturday") == 0)
        return CLK_REPEAT_SATURDAY;
    if (strcmp(str, "Sunday") == 0)
        return CLK_REPEAT_SUNDAY;
    return CLK_REPEAT_TODAY;
}

const char* clk_repeat_days_to_string(clk_repeat_days d) {
    static const char* names[] = {"Today",  "Monday",   "Tuesday", "Wednesday", "Thursday",
                                  "Friday", "Saturday", "Sunday",  "Everyday"};
    return names[(int)d];
}

static void parse_one_alarm(clk_cfg_alarm* alarm, clk_json_value* alarm_json) {
    memset(alarm, 0, sizeof(*alarm));

    const char* str = json_get_string(alarm_json, "name");
    if (str) {
        strncpy(alarm->name, str, sizeof(alarm->name) - 1);
        alarm->name[sizeof(alarm->name) - 1] = '\0';
    }

    alarm->hour = (int)json_get_number_or_default(alarm_json, "hour", 0);
    alarm->minute = (int)json_get_number_or_default(alarm_json, "minute", 0);
    alarm->volume =
        (int)json_get_number_or_default(alarm_json, "volume", CLK_CONFIG_VOLUME_DEFAULT);
    alarm->sound_repeat = (int)json_get_number_or_default(alarm_json, "sound_repeat",
                                                          CLK_CONFIG_SOUND_REPEAT_DEFAULT);

    {
        clk_json_value* json_val = clk_json_object_get(alarm_json, "enable");
        alarm->enabled = json_val ? clk_json_is_true(json_val) : false;
    }
    {
        clk_json_value* json_val = clk_json_object_get(alarm_json, "loop");
        alarm->loop = json_val ? clk_json_is_true(json_val) : false;
    }

    str = json_get_string(alarm_json, "sound");
    if (str) {
        strncpy(alarm->sound_file, str, sizeof(alarm->sound_file) - 1);
        alarm->sound_file[sizeof(alarm->sound_file) - 1] = '\0';
    }

    alarm->repeat_days = clk_repeat_days_from_string(json_get_string(alarm_json, "repeat"));

    alarm->today_date = (time_t)0;
    str = json_get_string(alarm_json, "today_date");
    if (str) {
        struct tm date_tm = {0};
        if (sscanf(str, "%d-%d-%d", &date_tm.tm_year, &date_tm.tm_mon, &date_tm.tm_mday) == 3) {
            date_tm.tm_year -= 1900;
            date_tm.tm_mon -= 1;
            alarm->today_date = mktime(&date_tm);
        }
    }
}

static void clk_cfg_alarms_init(clk_cfg_alarms* alarms, clk_json_value* json_array) {
    memset(alarms, 0, sizeof(*alarms));

    int json_count = (int)clk_json_array_count(json_array);
    if (json_count <= 0)
        return;

    alarms->count = json_count < CLK_ALARM_MAX ? json_count : CLK_ALARM_MAX;
    alarms->items = calloc((size_t)alarms->count, sizeof(clk_cfg_alarm));
    if (!alarms->items) {
        alarms->count = 0;
        return;
    }

    for (int i = 0; i < alarms->count; ++i) {
        clk_json_value* obj = clk_json_array_get(json_array, (size_t)i);
        if (!obj || !clk_json_is_object(obj))
            continue;
        parse_one_alarm(&alarms->items[i], obj);
    }
}

static void clk_cfg_alarms_deinit(clk_cfg_alarms* alarms) {
    free(alarms->items);
    memset(alarms, 0, sizeof(*alarms));
}

/* ================================================================
 *  Pomodoros
 * ================================================================ */

static void parse_one_segment(clk_cfg_pomodoro_segment* segment, clk_json_value* seg_obj) {
    memset(segment, 0, sizeof(*segment));

    const char* str = json_get_string(seg_obj, "name");
    if (str) {
        strncpy(segment->name, str, sizeof(segment->name) - 1);
        segment->name[sizeof(segment->name) - 1] = '\0';
    }

    segment->duration_seconds = CLK_MINUTES_TO_SECONDS(
        (int)json_get_number_or_default(seg_obj, "minutes", CLK_CONFIG_POMODORO_MINUTES_DEFAULT));
    segment->sound_repeat =
        (int)json_get_number_or_default(seg_obj, "sound_repeat", CLK_CONFIG_SOUND_REPEAT_DEFAULT);
    segment->volume = (int)json_get_number_or_default(seg_obj, "volume", CLK_CONFIG_VOLUME_DEFAULT);

    {
        clk_json_value* json_val = clk_json_object_get(seg_obj, "loop");
        segment->loop = json_val ? clk_json_is_true(json_val) : false;
    }

    str = json_get_string(seg_obj, "sound");
    if (str) {
        strncpy(segment->sound_file, str, sizeof(segment->sound_file) - 1);
        segment->sound_file[sizeof(segment->sound_file) - 1] = '\0';
    }
}

static void parse_one_pomodoro(clk_cfg_pomodoro* pomodoro, clk_json_value* obj) {
    memset(pomodoro, 0, sizeof(*pomodoro));

    const char* str = json_get_string(obj, "name");
    if (str) {
        strncpy(pomodoro->name, str, sizeof(pomodoro->name) - 1);
        pomodoro->name[sizeof(pomodoro->name) - 1] = '\0';
    }

    clk_json_value* segs_json = clk_json_object_get(obj, "segments");
    if (!segs_json || !clk_json_is_array(segs_json))
        return;

    int seg_count = (int)clk_json_array_count(segs_json);
    if (seg_count <= 0)
        return;

    pomodoro->segment_count =
        seg_count < CLK_POMODORO_MAX_SEGMENTS ? seg_count : CLK_POMODORO_MAX_SEGMENTS;
    pomodoro->segments = calloc((size_t)pomodoro->segment_count, sizeof(clk_cfg_pomodoro_segment));
    if (!pomodoro->segments) {
        pomodoro->segment_count = 0;
        return;
    }

    for (int j = 0; j < pomodoro->segment_count; ++j) {
        clk_json_value* seg_obj = clk_json_array_get(segs_json, (size_t)j);
        if (!seg_obj || !clk_json_is_object(seg_obj))
            continue;
        parse_one_segment(&pomodoro->segments[j], seg_obj);
    }
}

static void clk_cfg_pomodoros_init(clk_cfg_pomodoros* pomodoros, clk_json_value* json_array) {
    memset(pomodoros, 0, sizeof(*pomodoros));

    int json_count = (int)clk_json_array_count(json_array);
    if (json_count <= 0)
        return;

    pomodoros->count = json_count < CLK_POMODORO_MAX ? json_count : CLK_POMODORO_MAX;
    pomodoros->items = calloc((size_t)pomodoros->count, sizeof(clk_cfg_pomodoro));
    if (!pomodoros->items) {
        pomodoros->count = 0;
        return;
    }

    for (int i = 0; i < pomodoros->count; ++i) {
        clk_json_value* obj = clk_json_array_get(json_array, (size_t)i);
        if (!obj || !clk_json_is_object(obj))
            continue;
        parse_one_pomodoro(&pomodoros->items[i], obj);
    }
}

static void clk_cfg_pomodoros_deinit(clk_cfg_pomodoros* pomodoros) {
    for (int i = 0; i < pomodoros->count; ++i)
        free(pomodoros->items[i].segments);
    free(pomodoros->items);
    memset(pomodoros, 0, sizeof(*pomodoros));
}

/* ================================================================
 *  Clock (alarms + pomodoros wrapper)
 * ================================================================ */

void clk_cfg_clock_init(clk_cfg_clock* config, clk_json_value* clock_obj) {
    memset(config, 0, sizeof(*config));

    clk_json_value* alarms_json = clk_json_object_get(clock_obj, "alarms");
    if (alarms_json && clk_json_is_array(alarms_json))
        clk_cfg_alarms_init(&config->alarms, alarms_json);

    clk_json_value* pomo_json = clk_json_object_get(clock_obj, "pomodoro");
    if (pomo_json && clk_json_is_array(pomo_json))
        clk_cfg_pomodoros_init(&config->pomodoros, pomo_json);
}

void clk_cfg_clock_deinit(clk_cfg_clock* config) {
    clk_cfg_alarms_deinit(&config->alarms);
    clk_cfg_pomodoros_deinit(&config->pomodoros);
    memset(config, 0, sizeof(*config));
}

/* ================================================================
 *  BGM
 * ================================================================ */

static void parse_one_bgm(clk_cfg_bgm* bgm_entry, clk_json_value* obj) {
    memset(bgm_entry, 0, sizeof(*bgm_entry));

    const char* str = json_get_string(obj, "sound");
    if (str) {
        strncpy(bgm_entry->sound_file, str, sizeof(bgm_entry->sound_file) - 1);
        bgm_entry->sound_file[sizeof(bgm_entry->sound_file) - 1] = '\0';
    }

    bgm_entry->volume = (int)json_get_number_or_default(obj, "volume", CLK_CONFIG_VOLUME_DEFAULT);

    clk_json_value* json_val = clk_json_object_get(obj, "enable");
    bgm_entry->enabled = json_val ? clk_json_is_true(json_val) : false;
}

void clk_cfg_bgms_init(clk_cfg_bgms* bgm_list, clk_json_value* json_array) {
    memset(bgm_list, 0, sizeof(*bgm_list));

    int json_count = (int)clk_json_array_count(json_array);
    if (json_count <= 0)
        return;

    bgm_list->count = json_count < 99 ? json_count : 99;
    bgm_list->items = calloc((size_t)bgm_list->count, sizeof(clk_cfg_bgm));
    if (!bgm_list->items) {
        bgm_list->count = 0;
        return;
    }
    for (int i = 0; i < bgm_list->count; ++i) {
        clk_json_value* obj = clk_json_array_get(json_array, (size_t)i);
        if (!obj || !clk_json_is_object(obj))
            continue;
        parse_one_bgm(&bgm_list->items[i], obj);
    }
}

void clk_cfg_bgms_deinit(clk_cfg_bgms* bgm_list) {
    free(bgm_list->items);
    memset(bgm_list, 0, sizeof(*bgm_list));
}

/* ================================================================
 *  Aggregate
 * ================================================================ */

bool clk_app_config_load(clk_app_config* cfg, const char* path) {
    memset(cfg, 0, sizeof(*cfg));

    char* raw = clk_file_read_all(path, NULL);
    if (!raw)
        return false;

    cfg->json = clk_json_parse(raw);
    free(raw);
    if (!cfg->json)
        return false;

    clk_json_value* ascii_clock_obj = clk_json_object_get(cfg->json, "ascii_clock_theme");
    clk_json_value* menu_obj = clk_json_object_get(cfg->json, "menu");
    if (!ascii_clock_obj || !menu_obj) {
        clk_json_free(cfg->json);
        cfg->json = NULL;
        return false;
    }

    clk_cfg_ascii_clock_theme_init(&cfg->ascii_clock, ascii_clock_obj);
    clk_cfg_themes_init(&cfg->themes, menu_obj);

    {
        clk_json_value* clock_obj = clk_json_object_get(cfg->json, "clock");
        if (clock_obj && clk_json_is_object(clock_obj))
            clk_cfg_clock_init(&cfg->clock, clock_obj);
    }

    {
        clk_json_value* bgm_json = clk_json_object_get(cfg->json, "BGM");
        if (bgm_json && clk_json_is_array(bgm_json))
            clk_cfg_bgms_init(&cfg->bgm, bgm_json);
    }

    if (!cfg->ascii_clock.fonts.count || !cfg->ascii_clock.time_formats.strings ||
        !cfg->themes.count) {
        clk_app_config_deinit(cfg);
        return false;
    }

    return true;
}

void clk_app_config_reload(clk_app_config* cfg, clk_menu* menu, int tab_id, int tfmt_id,
                           int font_id, int theme_id) {
    clk_json_value* ascii_clock_obj = clk_json_object_get(cfg->json, "ascii_clock_theme");
    clk_json_value* menu_obj = clk_json_object_get(cfg->json, "menu");

    if (ascii_clock_obj)
        clk_cfg_ascii_clock_theme_reload(&cfg->ascii_clock, ascii_clock_obj, menu, tab_id, font_id,
                                         tfmt_id);
    if (menu_obj)
        clk_cfg_themes_reload(&cfg->themes, menu_obj, menu, tab_id, theme_id);

    {
        clk_cfg_clock_deinit(&cfg->clock);
        clk_json_value* clock_obj = clk_json_object_get(cfg->json, "clock");
        if (clock_obj && clk_json_is_object(clock_obj))
            clk_cfg_clock_init(&cfg->clock, clock_obj);
    }

    {
        clk_cfg_bgms_deinit(&cfg->bgm);
        clk_json_value* bgm_json = clk_json_object_get(cfg->json, "BGM");
        if (bgm_json && clk_json_is_array(bgm_json))
            clk_cfg_bgms_init(&cfg->bgm, bgm_json);
    }
}

void clk_app_config_deinit(clk_app_config* cfg) {
    clk_cfg_ascii_clock_theme_deinit(&cfg->ascii_clock);
    clk_cfg_themes_deinit(&cfg->themes);
    clk_cfg_clock_deinit(&cfg->clock);
    clk_cfg_bgms_deinit(&cfg->bgm);
    clk_json_free(cfg->json);
    cfg->json = NULL;
}

void clk_app_config_save(const clk_app_config* cfg, const char* path) {
    if (!cfg || !path)
        return;
    char* out = clk_json_stringify_pretty(cfg->json);
    if (!out)
        return;
    FILE* file = fopen(path, "w");
    if (file) {
        fputs(out, file);
        fclose(file);
    }
    free(out);
}

void clk_app_config_sync_basic(const clk_app_config* cfg) {
    if (!cfg)
        return;
    clk_json_value* theme_obj = clk_json_object_get(cfg->json, "ascii_clock_theme");
    clk_json_value* menu_obj = clk_json_object_get(cfg->json, "menu");
    if (theme_obj) {
        clk_json_value* font_obj = clk_json_object_get(theme_obj, "fonts");
        clk_json_value* time_obj = clk_json_object_get(theme_obj, "time_format");
        if (font_obj && cfg->ascii_clock.fonts.index >= 0 &&
            cfg->ascii_clock.fonts.index < cfg->ascii_clock.fonts.count)
            clk_json_object_set_string(font_obj, "font",
                                       cfg->ascii_clock.fonts.names[cfg->ascii_clock.fonts.index]);
        if (time_obj && cfg->ascii_clock.time_formats.index >= 0 &&
            cfg->ascii_clock.time_formats.index < cfg->ascii_clock.time_formats.count)
            clk_json_object_set_string(
                time_obj, "selected_time_format",
                cfg->ascii_clock.time_formats.strings[cfg->ascii_clock.time_formats.index]);
    }
    if (menu_obj && cfg->themes.index >= 0 && cfg->themes.index < cfg->themes.count)
        clk_json_object_set_string(menu_obj, "theme", cfg->themes.names[cfg->themes.index]);
}

void clk_app_config_sound_basename(const char* full_path, char* out, size_t size) {
    const char* last_slash = NULL;
    for (const char* p = full_path; p && *p; ++p)
        if (*p == '/' || *p == '\\')
            last_slash = p;
    const char* start = last_slash ? last_slash + 1 : full_path;

    size_t len = 0;
    while (start[len] && start[len] != '.' && len < size - 1)
        out[len] = start[len], ++len;
    out[len] = '\0';
}

void clk_app_config_sync_clock(const clk_app_config* cfg, const clk_clock* clock) {
    if (!cfg || !clock)
        return;

    clk_json_value* clock_obj = clk_json_object_get(cfg->json, "clock");
    if (!clock_obj) {
        clock_obj = clk_json_create_object();
        clk_json_object_set(cfg->json, "clock", clock_obj);
    }

    clk_json_value* alarms_arr = clk_json_create_array();
    for (int i = 0; i < clock->alarm_count; ++i) {
        const clk_clock_alarm* a = &clock->alarms[i];
        clk_json_value* obj = clk_json_create_object();

        clk_json_object_set_string(obj, "name", a->name);
        clk_json_object_set_number(obj, "hour", a->alarm.hour);
        clk_json_object_set_number(obj, "minute", a->alarm.minute);
        a->alarm.enabled ? clk_json_object_set_true(obj, "enable")
                         : clk_json_object_set_false(obj, "enable");
        a->loop ? clk_json_object_set_true(obj, "loop") : clk_json_object_set_false(obj, "loop");
        clk_json_object_set_number(obj, "sound_repeat", a->repeat_count);
        clk_json_object_set_number(obj, "volume", (int)lroundf(a->volume * 100.0f));
        clk_json_object_set_string(obj, "repeat", clk_repeat_days_to_string(a->repeat_days));

        if (a->sound) {
            const char* full = clk_audio_sound_get_path(a->sound);
            if (full && full[0]) {
                char basename[CLK_CONFIG_ALARM_SOUND_MAX];
                clk_app_config_sound_basename(full, basename, sizeof(basename));
                clk_json_object_set_string(obj, "sound", basename);
            }
        }

        if (a->repeat_days == CLK_REPEAT_TODAY && a->today_date != 0) {
            struct tm tm;
            if (clk_time_localtime_from(a->today_date, &tm)) {
                char date[32];
                snprintf(date, sizeof(date), "%04d-%02d-%02d", tm.tm_year + 1900, tm.tm_mon + 1,
                         tm.tm_mday);
                clk_json_object_set_string(obj, "today_date", date);
            }
        }

        clk_json_array_append(alarms_arr, obj);
    }
    clk_json_object_set(clock_obj, "alarms", alarms_arr);

    clk_json_value* pomo_arr = clk_json_create_array();
    for (int i = 0; i < clock->pomodoro_count; ++i) {
        const clk_clock_pomodoro* po = &clock->pomodoros[i];
        clk_json_value* po_obj = clk_json_create_object();

        clk_json_object_set_string(po_obj, "name", po->name);

        clk_json_value* segs_arr = clk_json_create_array();
        for (int j = 0; j < po->segment_count; ++j) {
            const clk_clock_pomodoro_segment* seg = &po->segments[j];
            clk_json_value* seg_obj = clk_json_create_object();

            clk_json_object_set_string(seg_obj, "name", seg->name);
            clk_json_object_set_number(seg_obj, "minutes", seg->duration_seconds / 60.0);
            clk_json_object_set_number(seg_obj, "sound_repeat", seg->repeat_count);
            clk_json_object_set_number(seg_obj, "volume", (int)lroundf(seg->volume * 100.0f));
            if (seg->loop)
                clk_json_object_set_true(seg_obj, "loop");

            if (seg->sound) {
                const char* full = clk_audio_sound_get_path(seg->sound);
                if (full && full[0]) {
                    char basename[CLK_CONFIG_ALARM_SOUND_MAX];
                    clk_app_config_sound_basename(full, basename, sizeof(basename));
                    clk_json_object_set_string(seg_obj, "sound", basename);
                }
            }

            clk_json_array_append(segs_arr, seg_obj);
        }
        clk_json_object_set(po_obj, "segments", segs_arr);
        clk_json_array_append(pomo_arr, po_obj);
    }
    clk_json_object_set(clock_obj, "pomodoro", pomo_arr);
}

void clk_app_config_sync_bgm(const clk_app_config* cfg, const clk_bgm* bgm) {
    if (!cfg || !bgm)
        return;
    clk_json_value* bgm_arr = clk_json_create_array();
    clk_json_value* obj = clk_json_create_object();
    clk_json_object_set_string(obj, "sound", bgm->sound_file);
    clk_json_object_set_number(obj, "volume", bgm->volume);
    bgm->enabled ? clk_json_object_set_true(obj, "enable")
                 : clk_json_object_set_false(obj, "enable");
    clk_json_array_append(bgm_arr, obj);
    clk_json_object_set(cfg->json, "BGM", bgm_arr);
}
