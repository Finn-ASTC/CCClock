#include "clk_app_config.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "clk_file_util.h"
#include "clk_fs_watch.h"
#include "clk_json.h"
#include "clk_menu.h"

/* ------------------------------------------------------------------
 *  Internal helpers
 * ------------------------------------------------------------------ */

static const char* json_get_str(clk_json_value* obj, const char* key) {
    clk_json_value* v = clk_json_object_get(obj, key);
    const char* str = NULL;
    if (v && clk_json_is_string(v) && clk_json_get_string(v, &str) == 0)
        return str;
    return NULL;
}

static double json_get_number_default(clk_json_value* obj, const char* key, double fallback) {
    clk_json_value* v = clk_json_object_get(obj, key);
    double num = fallback;
    if (v && clk_json_is_number(v) && clk_json_get_number(v, &num) == 0)
        return num;
    return fallback;
}

/** Parse a JSON string array into a char**.
 *  Returned strings borrow pointers from the JSON tree — do NOT free
 *  them individually; the JSON must outlive the returned array. */
static char** parse_time_formats_arr(clk_json_value* time_obj, int* out_count) {
    *out_count = 0;
    clk_json_value* array = clk_json_object_get(time_obj, "time_formats");
    if (!array || !clk_json_is_array(array))
        return NULL;
    int count = (int)clk_json_array_count(array);
    if (count <= 0)
        return NULL;
    char** formats = calloc(count, sizeof(char*));
    if (!formats)
        return NULL;
    for (int i = 0; i < count; ++i) {
        clk_json_value* element = clk_json_array_get(array, i);
        const char* str = NULL;
        if (element && clk_json_is_string(element) && clk_json_get_string(element, &str) == 0)
            formats[i] = (char*)str;
    }
    *out_count = count;
    return formats;
}

static void copy_time_format(char* dst, const char* src) {
    size_t len = strlen(src);
    if (len >= CLK_CLOCK_FORMAT_MAX_LENGTH)
        len = CLK_CLOCK_FORMAT_MAX_LENGTH - 1;
    memcpy(dst, src, len);
    dst[len] = '\0';
}

/* ================================================================
 *  Fonts
 * ================================================================ */

static void clk_cfg_fonts_init(clk_cfg_fonts* f, clk_json_value* fonts_obj) {
    memset(f, 0, sizeof(*f));

    f->dir = json_get_str(fonts_obj, "fonts_dir");
    if (!f->dir)
        return;

    f->paths = clk_fs_scan_dir(f->dir, ".json", &f->count);
    if (!f->paths)
        return;

    f->names = clk_menu_build_names(f->paths, f->count);
    if (!f->names)
        return;

    const char* saved = json_get_str(fonts_obj, "font");
    if (saved)
        f->idx = clk_menu_find_index(saved, (const char**)f->names, f->count, 0);
}

static void clk_cfg_fonts_reload(clk_cfg_fonts* f, clk_json_value* fonts_obj, clk_menu* menu,
                                 int tab_id, int item_id) {
    f->dir = json_get_str(fonts_obj, "fonts_dir");
    const char* saved = json_get_str(fonts_obj, "font");
    if (saved)
        f->idx = clk_menu_find_index(saved, (const char**)f->names, f->count, f->idx);
    clk_menu_rebuild_item(menu, tab_id, item_id, (const char**)f->names, f->count, f->idx);
}

static void clk_cfg_fonts_sync(clk_cfg_fonts* f, clk_menu* menu, int tab_id, int item_id) {
    clk_fs_sync_dir(f->dir, &f->paths, &f->count, &f->names, &f->idx, menu, tab_id, item_id);
}

static void clk_cfg_fonts_deinit(clk_cfg_fonts* f) {
    for (int i = 0; i < f->count; ++i) {
        free(f->paths[i]);
        free(f->names[i]);
    }
    free(f->paths);
    free(f->names);
    memset(f, 0, sizeof(*f));
}

/* ================================================================
 *  Themes
 * ================================================================ */

void clk_cfg_themes_init(clk_cfg_themes* t, clk_json_value* menu_obj) {
    memset(t, 0, sizeof(*t));

    t->dir = json_get_str(menu_obj, "themes_dir");
    if (!t->dir)
        return;

    t->paths = clk_fs_scan_dir(t->dir, ".json", &t->count);
    if (!t->paths)
        return;

    t->names = clk_menu_build_names(t->paths, t->count);
    if (!t->names)
        return;

    const char* saved = json_get_str(menu_obj, "theme");
    if (saved)
        t->idx = clk_menu_find_index(saved, (const char**)t->names, t->count, 0);
}

void clk_cfg_themes_reload(clk_cfg_themes* t, clk_json_value* menu_obj, clk_menu* menu, int tab_id,
                           int item_id) {
    t->dir = json_get_str(menu_obj, "themes_dir");
    const char* saved = json_get_str(menu_obj, "theme");
    if (saved)
        t->idx = clk_menu_find_index(saved, (const char**)t->names, t->count, t->idx);
    clk_menu_rebuild_item(menu, tab_id, item_id, (const char**)t->names, t->count, t->idx);
}

void clk_cfg_themes_sync(clk_cfg_themes* t, clk_menu* menu, int tab_id, int item_id) {
    clk_fs_sync_dir(t->dir, &t->paths, &t->count, &t->names, &t->idx, menu, tab_id, item_id);
}

void clk_cfg_themes_deinit(clk_cfg_themes* t) {
    for (int i = 0; i < t->count; ++i) {
        free(t->paths[i]);
        free(t->names[i]);
    }
    free(t->paths);
    free(t->names);
    memset(t, 0, sizeof(*t));
}

/* ================================================================
 *  Time formats
 * ================================================================ */

static void clk_cfg_time_formats_init(clk_cfg_time_formats* tf, clk_json_value* time_obj) {
    memset(tf, 0, sizeof(*tf));

    tf->strings = parse_time_formats_arr(time_obj, &tf->count);
    if (!tf->strings)
        return;

    tf->options = clk_menu_wrap_strings(tf->strings, tf->count);

    const char* saved = json_get_str(time_obj, "selected_time_format");
    if (saved)
        tf->idx = clk_menu_find_index(saved, tf->options, tf->count, 0);

    copy_time_format(tf->current, tf->strings[tf->idx]);
}

static void clk_cfg_time_formats_reload(clk_cfg_time_formats* tf, clk_json_value* time_obj,
                                        clk_menu* menu, int tab_id, int item_id) {
    free(tf->strings);
    free(tf->options);

    tf->strings = parse_time_formats_arr(time_obj, &tf->count);
    if (!tf->strings) {
        tf->strings = calloc(1, sizeof(char*));
        if (!tf->strings)
            tf->strings = NULL;
        tf->count = 0;
    }
    tf->options = clk_menu_wrap_strings(tf->strings, tf->count);

    const char* saved = json_get_str(time_obj, "selected_time_format");
    if (saved)
        tf->idx = clk_menu_find_index(saved, tf->options, tf->count, 0);

    clk_menu_rebuild_item(menu, tab_id, item_id, tf->options, tf->count, tf->idx);
}

static void clk_cfg_time_formats_switch(clk_cfg_time_formats* tf) {
    copy_time_format(tf->current, tf->strings[tf->idx]);
}

static void clk_cfg_time_formats_deinit(clk_cfg_time_formats* tf) {
    free(tf->strings);
    free(tf->options);
    memset(tf, 0, sizeof(*tf));
}

/* ================================================================
 *  Ascii Clock Theme (fonts + time formats wrapper)
 * ================================================================ */

void clk_cfg_ascii_clock_theme_init(clk_cfg_ascii_clock_theme* t, clk_json_value* theme_obj) {
    memset(t, 0, sizeof(*t));

    clk_json_value* fonts_obj = clk_json_object_get(theme_obj, "fonts");
    clk_json_value* time_obj = clk_json_object_get(theme_obj, "time_format");
    if (fonts_obj)
        clk_cfg_fonts_init(&t->fonts, fonts_obj);
    if (time_obj)
        clk_cfg_time_formats_init(&t->time_formats, time_obj);
}

void clk_cfg_ascii_clock_theme_reload(clk_cfg_ascii_clock_theme* t, clk_json_value* theme_obj,
                                      clk_menu* menu, int tab_id, int font_id, int tfmt_id) {
    clk_json_value* fonts_obj = clk_json_object_get(theme_obj, "fonts");
    clk_json_value* time_obj = clk_json_object_get(theme_obj, "time_format");
    if (fonts_obj)
        clk_cfg_fonts_reload(&t->fonts, fonts_obj, menu, tab_id, font_id);
    if (time_obj)
        clk_cfg_time_formats_reload(&t->time_formats, time_obj, menu, tab_id, tfmt_id);
}

void clk_cfg_ascii_clock_theme_sync_fonts(clk_cfg_ascii_clock_theme* t, clk_menu* menu, int tab_id,
                                          int item_id) {
    clk_cfg_fonts_sync(&t->fonts, menu, tab_id, item_id);
}

void clk_cfg_ascii_clock_theme_switch_time(clk_cfg_ascii_clock_theme* t) {
    clk_cfg_time_formats_switch(&t->time_formats);
}

void clk_cfg_ascii_clock_theme_deinit(clk_cfg_ascii_clock_theme* t) {
    clk_cfg_fonts_deinit(&t->fonts);
    clk_cfg_time_formats_deinit(&t->time_formats);
    memset(t, 0, sizeof(*t));
}

/* ================================================================
 *  Alarms
 * ================================================================ */

static clk_repeat_days parse_repeat_days(const char* str) {
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

static void parse_one_alarm(clk_cfg_alarm* alarm, clk_json_value* obj) {
    memset(alarm, 0, sizeof(*alarm));

    const char* str = json_get_str(obj, "name");
    if (str) {
        strncpy(alarm->name, str, sizeof(alarm->name) - 1);
        alarm->name[sizeof(alarm->name) - 1] = '\0';
    }

    alarm->hour = (int)json_get_number_default(obj, "hour", 0);
    alarm->minute = (int)json_get_number_default(obj, "minute", 0);
    alarm->volume = (int)json_get_number_default(obj, "volume", CLK_CFG_VOLUME_DEFAULT);
    alarm->sound_repeat =
        (int)json_get_number_default(obj, "sound_repeat", CLK_CFG_SOUND_REPEAT_DEFAULT);

    {
        clk_json_value* v = clk_json_object_get(obj, "enable");
        alarm->enabled = v ? clk_json_is_true(v) : false;
    }
    {
        clk_json_value* v = clk_json_object_get(obj, "loop");
        alarm->loop = v ? clk_json_is_true(v) : false;
    }

    str = json_get_str(obj, "sound");
    if (str) {
        strncpy(alarm->sound_file, str, sizeof(alarm->sound_file) - 1);
        alarm->sound_file[sizeof(alarm->sound_file) - 1] = '\0';
    }

    alarm->repeat_days = parse_repeat_days(json_get_str(obj, "repeat"));
}

static void clk_cfg_alarms_init(clk_cfg_alarms* a, clk_json_value* json_array) {
    memset(a, 0, sizeof(*a));

    int json_count = (int)clk_json_array_count(json_array);
    if (json_count <= 0)
        return;

    a->count = json_count < CLK_ALARM_MAX ? json_count : CLK_ALARM_MAX;
    a->items = calloc(a->count, sizeof(clk_cfg_alarm));
    if (!a->items) {
        a->count = 0;
        return;
    }

    for (int i = 0; i < a->count; ++i) {
        clk_json_value* obj = clk_json_array_get(json_array, i);
        if (!obj || !clk_json_is_object(obj))
            continue;
        parse_one_alarm(&a->items[i], obj);
    }
}

static void clk_cfg_alarms_deinit(clk_cfg_alarms* a) {
    free(a->items);
    memset(a, 0, sizeof(*a));
}

/* ================================================================
 *  Pomodoros
 * ================================================================ */

static void parse_one_segment(clk_cfg_pomodoro_segment* seg, clk_json_value* seg_obj) {
    memset(seg, 0, sizeof(*seg));

    const char* str = json_get_str(seg_obj, "name");
    if (str) {
        strncpy(seg->name, str, sizeof(seg->name) - 1);
        seg->name[sizeof(seg->name) - 1] = '\0';
    }

    seg->duration_seconds = CLK_MINUTES_TO_SECONDS(
        (int)json_get_number_default(seg_obj, "minutes", CLK_CFG_POMO_MINUTES_DEFAULT));
    seg->sound_repeat =
        (int)json_get_number_default(seg_obj, "sound_repeat", CLK_CFG_SOUND_REPEAT_DEFAULT);
    seg->volume = (int)json_get_number_default(seg_obj, "volume", CLK_CFG_VOLUME_DEFAULT);

    {
        clk_json_value* v = clk_json_object_get(seg_obj, "loop");
        seg->loop = v ? clk_json_is_true(v) : false;
    }

    str = json_get_str(seg_obj, "sound");
    if (str) {
        strncpy(seg->sound_file, str, sizeof(seg->sound_file) - 1);
        seg->sound_file[sizeof(seg->sound_file) - 1] = '\0';
    }
}

static void parse_one_pomodoro(clk_cfg_pomodoro* pomo, clk_json_value* obj) {
    memset(pomo, 0, sizeof(*pomo));

    const char* str = json_get_str(obj, "name");
    if (str) {
        strncpy(pomo->name, str, sizeof(pomo->name) - 1);
        pomo->name[sizeof(pomo->name) - 1] = '\0';
    }

    clk_json_value* segs_json = clk_json_object_get(obj, "segments");
    if (!segs_json || !clk_json_is_array(segs_json))
        return;

    int seg_count = (int)clk_json_array_count(segs_json);
    if (seg_count <= 0)
        return;

    pomo->segment_count =
        seg_count < CLK_POMODORO_MAX_SEGMENTS ? seg_count : CLK_POMODORO_MAX_SEGMENTS;
    pomo->segments = calloc(pomo->segment_count, sizeof(clk_cfg_pomodoro_segment));
    if (!pomo->segments) {
        pomo->segment_count = 0;
        return;
    }

    for (int j = 0; j < pomo->segment_count; ++j) {
        clk_json_value* seg_obj = clk_json_array_get(segs_json, j);
        if (!seg_obj || !clk_json_is_object(seg_obj))
            continue;
        parse_one_segment(&pomo->segments[j], seg_obj);
    }
}

static void clk_cfg_pomodoros_init(clk_cfg_pomodoros* p, clk_json_value* json_array) {
    memset(p, 0, sizeof(*p));

    int json_count = (int)clk_json_array_count(json_array);
    if (json_count <= 0)
        return;

    p->count = json_count < CLK_POMODORO_MAX ? json_count : CLK_POMODORO_MAX;
    p->items = calloc(p->count, sizeof(clk_cfg_pomodoro));
    if (!p->items) {
        p->count = 0;
        return;
    }

    for (int i = 0; i < p->count; ++i) {
        clk_json_value* obj = clk_json_array_get(json_array, i);
        if (!obj || !clk_json_is_object(obj))
            continue;
        parse_one_pomodoro(&p->items[i], obj);
    }
}

static void clk_cfg_pomodoros_deinit(clk_cfg_pomodoros* p) {
    for (int i = 0; i < p->count; ++i)
        free(p->items[i].segments);
    free(p->items);
    memset(p, 0, sizeof(*p));
}

/* ================================================================
 *  Clock (alarms + pomodoros wrapper)
 * ================================================================ */

void clk_cfg_clock_init(clk_cfg_clock* c, clk_json_value* clock_obj) {
    memset(c, 0, sizeof(*c));

    clk_json_value* alarms_json = clk_json_object_get(clock_obj, "alarms");
    if (alarms_json && clk_json_is_array(alarms_json))
        clk_cfg_alarms_init(&c->alarms, alarms_json);

    clk_json_value* pomo_json = clk_json_object_get(clock_obj, "pomodoro");
    if (pomo_json && clk_json_is_array(pomo_json))
        clk_cfg_pomodoros_init(&c->pomodoros, pomo_json);
}

void clk_cfg_clock_deinit(clk_cfg_clock* c) {
    clk_cfg_alarms_deinit(&c->alarms);
    clk_cfg_pomodoros_deinit(&c->pomodoros);
    memset(c, 0, sizeof(*c));
}

/* ================================================================
 *  BGM
 * ================================================================ */

static void parse_one_bgm(clk_cfg_bgm* bgm, clk_json_value* obj) {
    memset(bgm, 0, sizeof(*bgm));

    const char* str = json_get_str(obj, "sound");
    if (str) {
        strncpy(bgm->sound_file, str, sizeof(bgm->sound_file) - 1);
        bgm->sound_file[sizeof(bgm->sound_file) - 1] = '\0';
    }

    bgm->volume = (int)json_get_number_default(obj, "volume", CLK_CFG_VOLUME_DEFAULT);

    clk_json_value* v = clk_json_object_get(obj, "enable");
    bgm->enabled = v ? clk_json_is_true(v) : false;
}

void clk_cfg_bgms_init(clk_cfg_bgms* b, clk_json_value* json_array) {
    memset(b, 0, sizeof(*b));

    int json_count = (int)clk_json_array_count(json_array);
    if (json_count <= 0)
        return;

    b->items = calloc(json_count, sizeof(clk_cfg_bgm));
    if (!b->items)
        return;

    b->count = json_count;
    for (int i = 0; i < b->count; ++i) {
        clk_json_value* obj = clk_json_array_get(json_array, i);
        if (!obj || !clk_json_is_object(obj))
            continue;
        parse_one_bgm(&b->items[i], obj);
    }
}

void clk_cfg_bgms_deinit(clk_cfg_bgms* b) {
    free(b->items);
    memset(b, 0, sizeof(*b));
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
        clk_json_free(cfg->json);
        cfg->json = NULL;
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
