#include "clk_app_config.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "clk_file_util.h"
#include "clk_fs_watch.h"
#include "clk_json.h"
#include "clk_menu.h"

/* ── internal helpers ── */

static const char* json_get_str(clk_json_value* obj, const char* key) {
    clk_json_value* v = clk_json_object_get(obj, key);
    const char* str = NULL;
    if (v && clk_json_is_string(v) && clk_json_get_string(v, &str) == 0)
        return str;
    return NULL;
}

static char** parse_time_formats_arr(clk_json_value* time_obj, int* out_count) {
    *out_count = 0;
    clk_json_value* array = clk_json_object_get(time_obj, "time_formats");
    if (!array || !clk_json_is_array(array))
        return NULL;
    int count = (int)clk_json_array_count(array);
    if (count <= 0)
        return NULL;
    char** formats = calloc(count, sizeof(char*));
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

void clk_cfg_fonts_init(clk_cfg_fonts* f, clk_json_value* fonts_obj) {
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

void clk_cfg_fonts_reload(clk_cfg_fonts* f, clk_json_value* fonts_obj, clk_menu* menu, int tab_id,
                          int item_id) {
    f->dir = json_get_str(fonts_obj, "fonts_dir");
    const char* saved = json_get_str(fonts_obj, "font");
    if (saved)
        f->idx = clk_menu_find_index(saved, (const char**)f->names, f->count, f->idx);
    clk_menu_rebuild_item(menu, tab_id, item_id, (const char**)f->names, f->count, f->idx);
}

void clk_cfg_fonts_sync(clk_cfg_fonts* f, clk_menu* menu, int tab_id, int item_id) {
    clk_fs_sync_dir(f->dir, &f->paths, &f->count, &f->names, &f->idx, menu, tab_id, item_id);
}

void clk_cfg_fonts_deinit(clk_cfg_fonts* f) {
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

void clk_cfg_time_formats_init(clk_cfg_time_formats* tf, clk_json_value* time_obj) {
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

void clk_cfg_time_formats_reload(clk_cfg_time_formats* tf, clk_json_value* time_obj, clk_menu* menu,
                                 int tab_id, int item_id) {
    free(tf->strings);
    free(tf->options);

    tf->strings = parse_time_formats_arr(time_obj, &tf->count);
    if (!tf->strings) {
        tf->strings = calloc(1, sizeof(char*));
        tf->count = 0;
    }
    tf->options = clk_menu_wrap_strings(tf->strings, tf->count);

    const char* saved = json_get_str(time_obj, "selected_time_format");
    if (saved)
        tf->idx = clk_menu_find_index(saved, tf->options, tf->count, 0);

    clk_menu_rebuild_item(menu, tab_id, item_id, tf->options, tf->count, tf->idx);
}

void clk_cfg_time_formats_switch(clk_cfg_time_formats* tf) {
    copy_time_format(tf->current, tf->strings[tf->idx]);
}

void clk_cfg_time_formats_deinit(clk_cfg_time_formats* tf) {
    free(tf->strings);
    free(tf->options);
    memset(tf, 0, sizeof(*tf));
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

    clk_json_value* fonts_obj = clk_json_object_get(cfg->json, "fonts");
    clk_json_value* time_obj = clk_json_object_get(cfg->json, "time_format");
    clk_json_value* menu_obj = clk_json_object_get(cfg->json, "menu");
    if (!fonts_obj || !time_obj || !menu_obj) {
        clk_json_free(cfg->json);
        cfg->json = NULL;
        return false;
    }

    clk_cfg_fonts_init(&cfg->fonts, fonts_obj);
    clk_cfg_time_formats_init(&cfg->time_formats, time_obj);
    clk_cfg_themes_init(&cfg->themes, menu_obj);

    if (!cfg->fonts.count || !cfg->time_formats.strings || !cfg->themes.count) {
        clk_json_free(cfg->json);
        cfg->json = NULL;
        return false;
    }

    return true;
}

void clk_app_config_reload(clk_app_config* cfg, clk_menu* menu, int tab_id, int tfmt_id,
                           int font_id, int theme_id) {
    clk_json_value* fonts_obj = clk_json_object_get(cfg->json, "fonts");
    clk_json_value* time_obj = clk_json_object_get(cfg->json, "time_format");
    clk_json_value* menu_obj = clk_json_object_get(cfg->json, "menu");

    if (fonts_obj)
        clk_cfg_fonts_reload(&cfg->fonts, fonts_obj, menu, tab_id, font_id);
    if (time_obj)
        clk_cfg_time_formats_reload(&cfg->time_formats, time_obj, menu, tab_id, tfmt_id);
    if (menu_obj)
        clk_cfg_themes_reload(&cfg->themes, menu_obj, menu, tab_id, theme_id);
}

void clk_app_config_deinit(clk_app_config* cfg) {
    clk_cfg_fonts_deinit(&cfg->fonts);
    clk_cfg_themes_deinit(&cfg->themes);
    clk_cfg_time_formats_deinit(&cfg->time_formats);
    clk_json_free(cfg->json);
    cfg->json = NULL;
}
