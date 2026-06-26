#ifndef CLK_CLOCK_MENU_H
#define CLK_CLOCK_MENU_H

#include <stdbool.h>

#include "clk_json.h"
#include "clk_key_io.h"
#include "clk_menu.h"

#ifdef __cplusplus
extern "C" {
#endif

/* === item_id constants === */
enum {
    CLK_MENU_CLOCK_TFMT = 1,
    CLK_MENU_CLOCK_FONT = 2,
    CLK_MENU_CLOCK_THEME = 3,
    CLK_MENU_CLOCK_QUIT = 4,
    CLK_MENU_ALARM_BASE = 100,
    CLK_MENU_POMO_BASE = 200,
    CLK_MENU_BGM_BASE = 300,
};

/* === build === */

/** Build the Clock tab.  Appends one tab (id=0) and its four items. */
void clk_clock_menu_build(clk_menu* menu, const char** time_format_options, int time_format_count,
                          int time_format_index, const char** font_names, int font_count,
                          int font_index, const char** theme_names, int theme_count,
                          int theme_index);

/** Restore persisted selections from the config JSON.
 *  Falls back to index 0 on miss or absence. */
void clk_clock_menu_restore_selections(clk_json_value* clock_section, clk_json_value* menu_section,
                                       char** time_formats, int time_format_count,
                                       const char** font_names, int font_count,
                                       const char** theme_names, int theme_count,
                                       int* time_format_index, int* font_index, int* theme_index);

/* === hot-reload === */

/** Rebuild options for a single item. */
void clk_clock_menu_rebuild_item(clk_menu* menu, int tab_id, int item_id, const char** options,
                                 int count, int new_index);

/** Re-scan a directory and update the corresponding menu item if
 *  the file list changed.  Takes ownership on change — old data is
 *  freed and the in/out pointers are repointed. */
void clk_clock_menu_sync_dir(clk_menu* menu, int tab_id, int item_id, const char* dir_path,
                             char*** paths, int* count, const char*** display_names, int* index);

/* === event === */

/** Handle a VALUE_CHANGED or SUBMIT event for the Clock tab.
 *  Returns true when CLK_MENU_CLOCK_QUIT was submitted. */
bool clk_clock_menu_handle_event(clk_menu_event* event, int* time_format_index, int* font_index,
                                 int* theme_index);

/* === helpers (re-exported for main.c) === */

/** Build display names from file paths by stripping directory + extension. */
const char** clk_clock_menu_build_names(char** paths, int count);

/** Wrap a char** as a const char** view (no copies of strings). */
const char** clk_clock_menu_wrap_strings(char** strings, int count);

/** Translate a raw key event into a menu input action. */
clk_menu_input clk_clock_menu_translate_key(clk_key_event key_event);

#ifdef __cplusplus
}
#endif

#endif
