#include "clk_clock_menu.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "clk_fs_watch.h"

/* ================================================================
 *  Internal helpers
 * ================================================================ */

static char* make_display_name(const char* path) {
    const char* last_slash = NULL;
    for (const char* p = path; *p; ++p)
        if (*p == '/' || *p == '\\')
            last_slash = p;
    const char* name_start = last_slash ? last_slash + 1 : path;
    char* display_name = strdup(name_start);
    char* dot_pos = strrchr(display_name, '.');
    if (dot_pos)
        *dot_pos = '\0';
    return display_name;
}

static const char** build_display_names(char** paths, int count) {
    const char** names = calloc(count, sizeof(const char*));
    for (int i = 0; i < count; ++i)
        names[i] = make_display_name(paths[i]);
    return names;
}

static const char** str_array_to_const(char** array, int count) {
    const char** result = calloc(count, sizeof(const char*));
    for (int i = 0; i < count; ++i)
        result[i] = array[i];
    return result;
}

static int find_index_by_str(const char* needle, const char** haystack, int count, int fallback) {
    for (int i = 0; i < count; ++i) {
        if (strcmp(needle, haystack[i]) == 0)
            return i;
    }
    return fallback;
}

/* ================================================================
 *  Build
 * ================================================================ */

void clk_clock_menu_build(clk_menu* menu, const char** time_format_options, int time_format_count,
                          int time_format_index, const char** font_names, int font_count,
                          int font_index, const char** theme_names, int theme_count,
                          int theme_index) {
    clk_menu_add_tab(menu, 0, "clock");
    clk_menu_add_item_str(menu, 0, CLK_MENU_CLOCK_TFMT, "time format", time_format_index,
                          time_format_options, time_format_count);
    clk_menu_add_item_str(menu, 0, CLK_MENU_CLOCK_FONT, "font", font_index, font_names, font_count);
    clk_menu_add_item_str(menu, 0, CLK_MENU_CLOCK_THEME, "menu theme", theme_index, theme_names,
                          theme_count);
    clk_menu_add_item_action(menu, 0, CLK_MENU_CLOCK_QUIT, "quit");
}

void clk_clock_menu_restore_selections(clk_json_value* clock_section, clk_json_value* menu_section,
                                       char** time_formats, int time_format_count,
                                       const char** font_names, int font_count,
                                       const char** theme_names, int theme_count,
                                       int* time_format_index, int* font_index, int* theme_index) {
    const char* str;
    clk_json_value* jv;

    jv = clk_json_object_get(clock_section, "selected_time_format");
    if (jv && clk_json_is_string(jv) && clk_json_get_string(jv, &str) == 0)
        *time_format_index =
            find_index_by_str(str, (const char**)time_formats, time_format_count, 0);
    jv = clk_json_object_get(clock_section, "font");
    if (jv && clk_json_is_string(jv) && clk_json_get_string(jv, &str) == 0)
        *font_index = find_index_by_str(str, font_names, font_count, 0);

    jv = clk_json_object_get(menu_section, "theme");
    if (jv && clk_json_is_string(jv) && clk_json_get_string(jv, &str) == 0)
        *theme_index = find_index_by_str(str, theme_names, theme_count, 0);
}

/* ================================================================
 *  Hot-reload
 * ================================================================ */

void clk_clock_menu_rebuild_item(clk_menu* menu, int tab_id, int item_id, const char** options,
                                 int count, int new_index) {
    clk_menu_clear_options(menu, tab_id, item_id);
    for (int i = 0; i < count; ++i)
        clk_menu_add_option(menu, tab_id, item_id, options[i]);
    clk_menu_set_value_str(menu, tab_id, item_id, options[new_index]);
}

void clk_clock_menu_sync_dir(clk_menu* menu, int tab_id, int item_id, const char* dir_path,
                             char*** paths, int* count, const char*** display_names, int* index) {
    if (!dir_path)
        return;
    int new_count;
    char** new_paths = clk_fs_scan_dir(dir_path, ".json", &new_count);
    if (!new_paths)
        return;

    bool changed = (new_count != *count);
    for (int i = 0; i < *count && i < new_count && !changed; ++i)
        if (strcmp(new_paths[i], (*paths)[i]) != 0)
            changed = true;

    if (!changed || new_count <= 0) {
        for (int i = 0; i < new_count; ++i)
            free(new_paths[i]);
        free(new_paths);
        return;
    }

    for (int i = 0; i < *count; ++i) {
        free((*paths)[i]);
        free((void*)(*display_names)[i]);
    }
    free(*paths);
    free(*display_names);

    *paths = new_paths;
    *count = new_count;
    *display_names = build_display_names(new_paths, new_count);
    if (*index >= new_count)
        *index = 0;
    clk_clock_menu_rebuild_item(menu, tab_id, item_id, *display_names, new_count, *index);
}

/* ================================================================
 *  Events
 * ================================================================ */

clk_menu_input clk_clock_menu_translate_key(clk_key_event key_event) {
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

bool clk_clock_menu_handle_event(clk_menu_event* event, int* time_format_index, int* font_index,
                                 int* theme_index) {
    if (!event)
        return false;

    if (event->type == CLK_MENU_EVENT_SUBMIT && event->item_id == CLK_MENU_CLOCK_QUIT)
        return true;

    if (event->type != CLK_MENU_EVENT_VALUE_CHANGED)
        return false;

    /* caller is responsible for applying the resolved index
     * (changing the renderer, copying time_format, etc.) */
    return false;
}

/* ================================================================
 *  Re-exported helpers
 * ================================================================ */

const char** clk_clock_menu_build_names(char** paths, int count) {
    return build_display_names(paths, count);
}

const char** clk_clock_menu_wrap_strings(char** strings, int count) {
    return str_array_to_const(strings, count);
}
