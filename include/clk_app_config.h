#ifndef CLK_APP_CONFIG_H
#define CLK_APP_CONFIG_H

#include <stdbool.h>
#include <stddef.h>
#include <time.h>

#include "clk_clock.h"
#include "clk_json.h"
#include "clk_menu.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------
 *  Per-module data containers (no shared types — each is independent)
 * ------------------------------------------------------------------ */

#define CLK_CONFIG_ALARM_SOUND_MAX 256

#define CLK_CONFIG_VOLUME_DEFAULT 50
#define CLK_CONFIG_SOUND_REPEAT_DEFAULT 1
#define CLK_CONFIG_POMODORO_MINUTES_DEFAULT 25
#define CLK_MINUTES_TO_SECONDS(m) ((m) * 60)

typedef struct {
    const char* dir;
    char** paths;
    char** names;
    int count;
    int index;
} clk_cfg_fonts;

typedef struct {
    const char* dir;
    char** paths;
    char** names;
    int count;
    int index;
} clk_cfg_themes;

typedef struct {
    char** strings;
    const char** options;
    int count;
    int index;
    char current[CLK_CLOCK_FORMAT_MAX_LENGTH];
} clk_cfg_time_formats;

typedef struct {
    char name[CLK_CLOCK_NAME_MAX];
    int hour;
    int minute;
    bool enabled;
    char sound_file[CLK_CONFIG_ALARM_SOUND_MAX];
    int sound_repeat;
    int volume;
    bool loop;
    clk_repeat_days repeat_days;
    time_t today_date; /* midnight of target date, only when CLK_REPEAT_TODAY */
} clk_cfg_alarm;

typedef struct {
    clk_cfg_alarm* items;
    int count;
} clk_cfg_alarms;

typedef struct {
    char name[CLK_CLOCK_NAME_MAX];
    int duration_seconds;
    int sound_repeat;
    int volume;
    bool loop;
    char sound_file[CLK_CONFIG_ALARM_SOUND_MAX];
} clk_cfg_pomodoro_segment;

typedef struct {
    char name[CLK_CLOCK_NAME_MAX];
    clk_cfg_pomodoro_segment* segments;
    int segment_count;
} clk_cfg_pomodoro;

typedef struct {
    clk_cfg_pomodoro* items;
    int count;
} clk_cfg_pomodoros;

typedef struct {
    char sound_file[CLK_CONFIG_ALARM_SOUND_MAX];
    int volume;
    bool enabled;
} clk_cfg_bgm;

typedef struct {
    clk_cfg_bgm* items;
    int count;
} clk_cfg_bgms;

/* ------------------------------------------------------------------
 *  Ascii Clock Theme (fonts + time formats wrapped)
 * ------------------------------------------------------------------ */

typedef struct {
    clk_cfg_fonts fonts;
    clk_cfg_time_formats time_formats;
} clk_cfg_ascii_clock_theme;

/** Parse the "ascii_clock_theme" JSON object — delegates to fonts/time_format init. */
void clk_cfg_ascii_clock_theme_init(clk_cfg_ascii_clock_theme* ascii_clock,
                                    clk_json_value* theme_obj);

/** Rebuild fonts/time_format menu items after a config reload. */
void clk_cfg_ascii_clock_theme_reload(clk_cfg_ascii_clock_theme* ascii_clock,
                                      clk_json_value* theme_obj, clk_menu* menu, int tab_id,
                                      int font_id, int tfmt_id);

/** Rescan the fonts directory and update the menu item if files changed. */
void clk_cfg_ascii_clock_theme_sync_fonts(clk_cfg_ascii_clock_theme* ascii_clock, clk_menu* menu,
                                          int tab_id, int item_id);

/** Copy the currently selected time format into time_formats.current. */
void clk_cfg_ascii_clock_theme_switch_time(clk_cfg_ascii_clock_theme* ascii_clock);

void clk_cfg_ascii_clock_theme_deinit(clk_cfg_ascii_clock_theme* ascii_clock);

/* ------------------------------------------------------------------
 *  Themes
 * ------------------------------------------------------------------ */

/** Load from the "menu" JSON object (reads "themes_dir" + "theme"). */
void clk_cfg_themes_init(clk_cfg_themes* themes, clk_json_value* menu_obj);

void clk_cfg_themes_reload(clk_cfg_themes* themes, clk_json_value* menu_obj, clk_menu* menu,
                           int tab_id, int item_id);

void clk_cfg_themes_sync(clk_cfg_themes* themes, clk_menu* menu, int tab_id, int item_id);

void clk_cfg_themes_deinit(clk_cfg_themes* themes);

/* ------------------------------------------------------------------
 *  Clock (alarms + pomodoros wrapped)
 * ------------------------------------------------------------------ */

typedef struct {
    clk_cfg_alarms alarms;
    clk_cfg_pomodoros pomodoros;
} clk_cfg_clock;

void clk_cfg_clock_init(clk_cfg_clock* config, clk_json_value* clock_obj);
void clk_cfg_clock_deinit(clk_cfg_clock* config);

/* ------------------------------------------------------------------
 *  BGM
 * ------------------------------------------------------------------ */

/** Parse the "BGM" JSON array into clk_cfg_bgm entries. */
void clk_cfg_bgms_init(clk_cfg_bgms* bgm_list, clk_json_value* json_array);

void clk_cfg_bgms_deinit(clk_cfg_bgms* bgm_list);

/* ------------------------------------------------------------------
 *  Repeat-days conversion
 * ------------------------------------------------------------------ */

/** Convert a repeat-day string to its enum value.
 *  Unknown strings map to CLK_REPEAT_TODAY. */
clk_repeat_days clk_repeat_days_from_string(const char* str);

/** Convert a repeat-day enum value back to its canonical string. */
const char* clk_repeat_days_to_string(clk_repeat_days d);

void clk_app_config_sound_basename(const char* full_path, char* out, size_t size);

/* ------------------------------------------------------------------
 *  Aggregate
 * ------------------------------------------------------------------ */

typedef struct {
    clk_cfg_ascii_clock_theme ascii_clock;
    clk_cfg_themes themes;
    clk_cfg_clock clock;
    clk_cfg_bgms bgm;
    clk_json_value* json;
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

void clk_app_config_save(const clk_app_config* cfg, const char* path);

void clk_app_config_sync_basic(const clk_app_config* cfg);

void clk_app_config_sync_clock(const clk_app_config* cfg, const clk_clock* clock);

#ifdef __cplusplus
}
#endif

#endif
