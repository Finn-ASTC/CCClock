#ifndef CLK_APP_CONFIG_H
#define CLK_APP_CONFIG_H

#include <stdbool.h>
#include <stddef.h>

#include "clk_clock.h"
#include "clk_json.h"
#include "clk_menu.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------
 *  Per-module data containers (no shared types — each is independent)
 * ------------------------------------------------------------------ */

typedef struct {
    const char* dir;
    char**      paths;
    char**      names;
    int         count;
    int         idx;
} clk_cfg_fonts;

typedef struct {
    const char* dir;
    char**      paths;
    char**      names;
    int         count;
    int         idx;
} clk_cfg_themes;

typedef struct {
    char**       strings;
    const char** options;
    int          count;
    int          idx;
    char         current[CLK_CLOCK_FORMAT_MAX_LENGTH];
} clk_cfg_time_formats;

/* ------------------------------------------------------------------
 *  Fonts
 * ------------------------------------------------------------------ */

/** Load from the "fonts" JSON object.  Scans the directory, builds
 *  display names, resolves the saved font index. */
void clk_cfg_fonts_init(clk_cfg_fonts* f, clk_json_value* fonts_obj);

/** Rebuild the font menu item after a config reload. */
void clk_cfg_fonts_reload(clk_cfg_fonts* f, clk_json_value* fonts_obj, clk_menu* menu, int tab_id,
                          int item_id);

/** Rescan the directory and update the menu item if files changed. */
void clk_cfg_fonts_sync(clk_cfg_fonts* f, clk_menu* menu, int tab_id, int item_id);

void clk_cfg_fonts_deinit(clk_cfg_fonts* f);

/* ------------------------------------------------------------------
 *  Themes
 * ------------------------------------------------------------------ */

/** Load from the "menu" JSON object (reads "themes_dir" + "theme"). */
void clk_cfg_themes_init(clk_cfg_themes* t, clk_json_value* menu_obj);

void clk_cfg_themes_reload(clk_cfg_themes* t, clk_json_value* menu_obj, clk_menu* menu, int tab_id,
                           int item_id);

void clk_cfg_themes_sync(clk_cfg_themes* t, clk_menu* menu, int tab_id, int item_id);

void clk_cfg_themes_deinit(clk_cfg_themes* t);

/* ------------------------------------------------------------------
 *  Time formats
 * ------------------------------------------------------------------ */

/** Load from the "time_format" JSON object. */
void clk_cfg_time_formats_init(clk_cfg_time_formats* tf, clk_json_value* time_obj);

/** Rebuild the time format menu item after a config reload. */
void clk_cfg_time_formats_reload(clk_cfg_time_formats* tf, clk_json_value* time_obj, clk_menu* menu,
                                 int tab_id, int item_id);

/** Copy the currently selected format string into tf->current.
 *  Call after tf->idx has been changed. */
void clk_cfg_time_formats_switch(clk_cfg_time_formats* tf);

void clk_cfg_time_formats_deinit(clk_cfg_time_formats* tf);

/* ------------------------------------------------------------------
 *  Aggregate
 * ------------------------------------------------------------------ */

typedef struct {
    clk_cfg_fonts        fonts;
    clk_cfg_themes       themes;
    clk_cfg_time_formats  time_formats;
    clk_json_value*      json;       /* raw parse tree — used for save */
} clk_app_config;

/** Load all modules from @p path.  Returns false on any failure. */
bool clk_app_config_load(clk_app_config* cfg, const char* path);

/** Reload after the underlying JSON has been replaced by the caller.
 *  Frees stale data and rebuilds the time format menu item.
 *  @p tfmt_id / font_id / theme_id  are the caller's item_id constants
 *  for the three menu items on @p tab_id. */
void clk_app_config_reload(clk_app_config* cfg, clk_menu* menu, int tab_id, int tfmt_id,
                           int font_id, int theme_id);

/** Release all resources held by all modules. */
void clk_app_config_deinit(clk_app_config* cfg);

#ifdef __cplusplus
}
#endif

#endif
