#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#else
#include <dirent.h>
#endif

#include "clk_ascii_render.h"
#include "clk_clock.h"
#include "clk_file_util.h"
#include "clk_json.h"
#include "clk_key_io.h"
#include "clk_menu.h"
#include "clk_menu_instance.h"
#include "clk_menu_theme.h"
#include "clk_term.h"

#define APP_CONFIG "assets/config/app_config.json"
#define CLK_MENU_DEFAULT_WIDTH  65
#define CLK_MENU_DEFAULT_HEIGHT 35
#define CLK_FRAME_MS            16
#define CLK_HOTRELOAD_TICKS     30   /* 30 × 16ms ≈ 0.5s */

/* ── cross-platform directory scanner ── */

#if defined(_WIN32) || defined(_WIN64)
/**
 * Scan a directory for *.json files and return their full paths.
 * Writes the number of matches to *out_count.
 * Returns NULL on error or when nothing matches.
 * Ownership: caller must free each returned string and the array itself.
 */
static char** scan_json_dir(const char* dir_path, int* out_count) {
    *out_count = 0;
    char pattern[1024];
    snprintf(pattern, sizeof(pattern), "%s\\*.json", dir_path);

    WIN32_FIND_DATA find_data;
    HANDLE find_handle = FindFirstFile(pattern, &find_data);
    if (find_handle == INVALID_HANDLE_VALUE)
        return NULL;

    char** list = NULL;
    int count = 0, capacity = 0;
    do {
        if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            continue;
        size_t name_len = strlen(find_data.cFileName);
        if (name_len < 6 || strcmp(find_data.cFileName + name_len - 5, ".json") != 0)
            continue;
        if (count >= capacity) {
            capacity = capacity ? capacity * 2 : 8;
            char** tmp = realloc(list, capacity * sizeof(char*));
            if (!tmp) {
                free(list);
                FindClose(find_handle);
                return NULL;
            }
            list = tmp;
        }
        size_t path_len = strlen(dir_path) + 1 + name_len + 1;
        list[count] = malloc(path_len);
        snprintf(list[count], path_len, "%s\\%s", dir_path, find_data.cFileName);
        count++;
    } while (FindNextFile(find_handle, &find_data));
    FindClose(find_handle);

    *out_count = count;
    return list;
}
#else
/**
 * Scan a directory for *.json files and return their full paths.
 * Writes the number of matches to *out_count.
 * Returns NULL on error or when nothing matches.
 * Ownership: caller must free each returned string and the array itself.
 */
static char** scan_json_dir(const char* dir_path, int* out_count) {
    *out_count = 0;
    DIR* dir = opendir(dir_path);
    if (!dir)
        return NULL;

    char** list = NULL;
    int count = 0, capacity = 0;
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        size_t name_len = strlen(entry->d_name);
        if (name_len <= 5 || strcmp(entry->d_name + name_len - 5, ".json") != 0)
            continue;
        if (count >= capacity) {
            capacity = capacity ? capacity * 2 : 8;
            char** tmp = realloc(list, capacity * sizeof(char*));
            if (!tmp) {
                free(list);
                closedir(dir);
                return NULL;
            }
            list = tmp;
        }
        size_t path_len = strlen(dir_path) + 1 + name_len + 1;
        list[count] = malloc(path_len);
        snprintf(list[count], path_len, "%s/%s", dir_path, entry->d_name);
        count++;
    }
    closedir(dir);

    *out_count = count;
    return list;
}
#endif

/**
 * Build a human-readable label from a file path.
 * Strips the leading directory and the trailing ".json" extension.
 * Returns a malloc'd string the caller must free.
 */
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

/**
 * Wrap a char** array as a const char** view of the same pointers.
 * Returns a malloc'd array (the caller must free); the strings are not copied.
 */
static const char** str_array_to_const(char** array, int count) {
    const char** result = calloc(count, sizeof(const char*));
    for (int i = 0; i < count; ++i)
        result[i] = array[i];
    return result;
}

/* ── hot-reload dir scan ── */

/**
 * Re-scan a directory and, if its set of *.json files changed, rebuild the
 * matching menu options in place.
 *
 * On a real change the old paths and display names are freed, the in/out
 * pointers are repointed at the freshly scanned data, new display names are
 * generated, the selection index is clamped, and the menu item's options are
 * cleared and repopulated.
 */
static void check_dir_changed(const char* dir_path, char*** paths, int* count,
                              const char*** display_names, int* index, clk_menu* menu,
                              int item_id) {
    if (!dir_path)
        return;
    int new_count;
    char** new_paths = scan_json_dir(dir_path, &new_count);
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
    *display_names = calloc(new_count, sizeof(const char*));
    for (int i = 0; i < new_count; ++i)
        (*display_names)[i] = make_display_name(new_paths[i]);

    if (*index >= new_count)
        *index = 0;
    clk_menu_clear_options(menu, 0, item_id);
    for (int i = 0; i < new_count; ++i)
        clk_menu_add_option(menu, 0, item_id, (*display_names)[i]);
    clk_menu_set_value_str(menu, 0, item_id, (*display_names)[*index]);
}

/* ── recenter ── */

/** Center the clock renderer within the given terminal dimensions. */
static void recenter_clock(clk_ascii_render* render, const char* time_format, int term_width,
                           int term_height) {
    int clock_width, clock_height;
    if (!clk_ascii_render_get_size(render, time_format, &clock_width, &clock_height))
        return;
    clk_ascii_render_set_pos(render, (term_width - clock_width) / 2,
                             (term_height - clock_height) / 2);
}

/** Center the menu instance within the given terminal dimensions. */
static void recenter_menu(clk_menu_instance* instance, int term_width, int term_height) {
    if (!instance)
        return;
    clk_menu_instance_set_position(instance, (term_width - instance->tex.tex_w) / 2,
                                   (term_height - instance->tex.tex_h) / 2);
}

/* ── focus ── */

typedef enum { FOCUS_CLOCK, FOCUS_MENU } focus_t;

/** Translate a raw key event into the corresponding menu input action. */
static clk_menu_input map_menu_input(clk_key_event key_event) {
    switch (key_event.key) {
        case CLK_KEY_UP:
            return CLK_MENU_INPUT_PREV_ITEM;
        case CLK_KEY_DOWN:
            return CLK_MENU_INPUT_NEXT_ITEM;
        case CLK_KEY_LEFT:
            return CLK_MENU_INPUT_DEC_VALUE;
        case CLK_KEY_RIGHT:
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

/* ── helpers ── */

/** Copy a time format string into @p dst (size CLK_CLOCK_FORMAT_MAX_LENGTH),
 *  clamping to fit.  Safe for any valid source. */
static void copy_time_format(char* dst, const char* src) {
    size_t len = strlen(src);
    if (len >= CLK_CLOCK_FORMAT_MAX_LENGTH)
        len = CLK_CLOCK_FORMAT_MAX_LENGTH - 1;
    memcpy(dst, src, len);
    dst[len] = '\0';
}

/** Find the index of @p needle in @p haystack (count entries).
 *  Returns the index, or @p fallback if not found. */
static int find_index_by_str(const char* needle, const char** haystack, int count,
                              int fallback) {
    for (int i = 0; i < count; ++i) {
        if (strcmp(needle, haystack[i]) == 0)
            return i;
    }
    return fallback;
}

int main(void) {
    if (!clk_term_init()) {
        printf("term init fail\n");
        return 1;
    }

    /* ── load app config ─────────────────────────────────── */
    size_t _config_size = 0;
    char* raw_json = clk_file_read_all(APP_CONFIG, &_config_size);
    if (!raw_json) {
        clk_term_close();
        printf("no app config\n");
        return 1;
    }

    clk_json_value* config = clk_json_parse(raw_json);
    free(raw_json);
    if (!config) {
        clk_term_close();
        return 1;
    }

    clk_json_value* clock_section = clk_json_object_get(config, "clock");
    clk_json_value* menu_section = clk_json_object_get(config, "menu");
    if (!clock_section || !menu_section) {
        clk_json_free(config);
        clk_term_close();
        return 1;
    }

    const char* fonts_dir = NULL;
    clk_json_value* dir_field = clk_json_object_get(clock_section, "fonts_dir");
    if (dir_field && clk_json_is_string(dir_field))
        clk_json_get_string(dir_field, &fonts_dir);

    char** font_paths;
    int font_count;
    font_paths = fonts_dir ? scan_json_dir(fonts_dir, &font_count) : NULL;

    char** time_formats;
    int time_format_count = 0;
    {
        clk_json_value* array = clk_json_object_get(clock_section, "time_formats");
        if (array && clk_json_is_array(array)) {
            time_format_count = (int)clk_json_array_count(array);
            time_formats = calloc(time_format_count, sizeof(char*));
            for (int i = 0; i < time_format_count; ++i) {
                clk_json_value* element = clk_json_array_get(array, i);
                const char* str = NULL;
                if (element && clk_json_is_string(element) &&
                    clk_json_get_string(element, &str) == 0)
                    time_formats[i] = (char*)str;
            }
        } else
            time_formats = NULL;
    }

    const char* themes_dir = NULL;
    dir_field = clk_json_object_get(menu_section, "themes_dir");
    if (dir_field && clk_json_is_string(dir_field))
        clk_json_get_string(dir_field, &themes_dir);

    char** theme_paths;
    int theme_count;
    theme_paths = themes_dir ? scan_json_dir(themes_dir, &theme_count) : NULL;

    if (!font_count || !time_format_count || !theme_count) {
        clk_json_free(config);
        clk_term_close();
        return 1;
    }

    int time_format_index = 0, font_index = 0, theme_index = 0;
    {
        clk_json_value* json_val;
        double num;
        json_val = clk_json_object_get(config, "selected_fmt");
        if (json_val && clk_json_is_number(json_val) && clk_json_get_number(json_val, &num) == 0)
            time_format_index = (int)num % time_format_count;
        json_val = clk_json_object_get(config, "selected_font");
        if (json_val && clk_json_is_number(json_val) && clk_json_get_number(json_val, &num) == 0)
            font_index = (int)num % font_count;
        json_val = clk_json_object_get(config, "selected_theme");
        if (json_val && clk_json_is_number(json_val) && clk_json_get_number(json_val, &num) == 0)
            theme_index = (int)num % theme_count;
    }

    /* ── clock (time + render split) ─────────────────────── */
    char time_format[CLK_CLOCK_FORMAT_MAX_LENGTH];
    copy_time_format(time_format, time_formats[time_format_index]);

    clk_ascii_render render;
    if (!clk_ascii_render_create(&render, font_paths[font_index])) {
        clk_json_free(config);
        clk_term_close();
        printf("render create fail\n");
        return 1;
    }
    clk_ascii_render_set_z_order(&render, 0);
    clk_ascii_render_add_to_term(&render);

    /* ── menu ────────────────────────────────────────────── */
    enum { ITEM_TFMT = 1, ITEM_FONT = 2, ITEM_THEME = 3, ITEM_QUIT = 4 };

    const char** font_names = calloc(font_count, sizeof(const char*));
    const char** theme_names = calloc(theme_count, sizeof(const char*));
    for (int i = 0; i < font_count; ++i)
        font_names[i] = make_display_name(font_paths[i]);
    for (int i = 0; i < theme_count; ++i)
        theme_names[i] = make_display_name(theme_paths[i]);

    clk_menu* menu = clk_menu_create();
    clk_menu_add_tab(menu, 0, "menu");
    const char** time_format_options = str_array_to_const(time_formats, time_format_count);
    clk_menu_add_item_str(menu, 0, ITEM_TFMT, "time format", time_format_index, time_format_options,
                          time_format_count);
    clk_menu_add_item_str(menu, 0, ITEM_FONT, "font", font_index, font_names, font_count);
    clk_menu_add_item_str(menu, 0, ITEM_THEME, "menu theme", theme_index, theme_names, theme_count);
    clk_menu_add_item_action(menu, 0, ITEM_QUIT, "quit");

    clk_menu_theme theme;
    memset(&theme, 0, sizeof(theme));
    clk_menu_theme_load(theme_paths[theme_index], &theme);

    clk_menu_instance* menu_inst = clk_menu_instance_create(menu, &theme);
    clk_menu_instance_set_size(menu_inst, CLK_MENU_DEFAULT_WIDTH, CLK_MENU_DEFAULT_HEIGHT);
    clk_menu_instance_set_visible(menu_inst, false);
    clk_menu_instance_add_to_term(menu_inst);
    clk_sprite_set_z(menu_inst->sprite, 5);

    /* ── initial layout ──────────────────────────────────── */
    int term_width, term_height;
    clk_term_get_size(&term_width, &term_height);
    recenter_clock(&render, time_format, term_width, term_height);
    recenter_menu(menu_inst, term_width, term_height);

    /* ── hot-reload state ─────────────────────────────────── */
    time_t last_app = 0, last_font = 0, last_theme = 0;
    {
        struct stat stat_buf;
        if (stat(APP_CONFIG, &stat_buf) == 0)
            last_app = stat_buf.st_mtime;
    }
    int hot_tick = 0;

    /* ── main loop ───────────────────────────────────────── */
    focus_t focus = FOCUS_CLOCK;
    bool running = true;

    while (running) {
        clk_key_event key_event = clk_get_key_event();

        switch (focus) {
            case FOCUS_CLOCK:
                if (key_event.key == 's' || key_event.key == 'S') {
                    clk_menu_instance_set_visible(menu_inst, true);
                    focus = FOCUS_MENU;
                    continue;
                }
                if (key_event.key == 'f' || key_event.key == 'F') {
                    time_format_index = (time_format_index + 1) % time_format_count;
                    copy_time_format(time_format, time_formats[time_format_index]);
                    clk_menu_set_value_str(menu, 0, ITEM_TFMT, time_formats[time_format_index]);
                }
                if (key_event.key == 'r' || key_event.key == 'R') {
                    font_index = (font_index + 1) % font_count;
                    clk_ascii_render_change_font(&render, font_paths[font_index]);
                    clk_menu_set_value_str(menu, 0, ITEM_FONT, font_names[font_index]);
                }
                if (key_event.key == 'q' || key_event.key == 'Q')
                    running = false;

                break;

            case FOCUS_MENU:
                if (key_event.key == 'q' || key_event.key == 'Q') {
                    clk_menu_instance_set_visible(menu_inst, false);
                    focus = FOCUS_CLOCK;
                    continue;
                }
                {
                    clk_menu_event menu_event =
                        clk_menu_instance_handle_input(menu_inst, map_menu_input(key_event));
                    if (menu_event.type == CLK_MENU_EVENT_SUBMIT && menu_event.item_id == ITEM_QUIT)
                        running = false;
                    if (menu_event.type == CLK_MENU_EVENT_VALUE_CHANGED) {
                        switch (menu_event.item_id) {
                            case ITEM_TFMT:
                                time_format_index = find_index_by_str(menu_event.value.str,
                                                                      (const char**)time_formats,
                                                                      time_format_count,
                                                                      time_format_index);
                                copy_time_format(time_format, time_formats[time_format_index]);
                                break;
                            case ITEM_FONT:
                                font_index = find_index_by_str(menu_event.value.str, font_names,
                                                                font_count, font_index);
                                clk_ascii_render_change_font(&render, font_paths[font_index]);
                                break;
                            case ITEM_THEME:
                                theme_index = find_index_by_str(menu_event.value.str, theme_names,
                                                                 theme_count, theme_index);
                                clk_menu_instance_change_theme(menu_inst, theme_paths[theme_index]);
                                break;
                        }
                    }
                }
                break;
        }

        /* resize */
        if (clk_term_size_changed()) {
            clk_term_resize();
            clk_term_compact();
            clk_term_get_size(&term_width, &term_height);
            recenter_clock(&render, time_format, term_width, term_height);
            recenter_menu(menu_inst, term_width, term_height);
        }

        recenter_clock(&render, time_format, term_width, term_height);
        recenter_menu(menu_inst, term_width, term_height);

        /* time → render pipeline */
        {
            char translated[128];
            char time_str[CLK_CLOCK_FORMAT_MAX_LENGTH];
            if (clk_clock_translate_format(time_format, translated, sizeof(translated)) &&
                clk_clock_format_now(translated, time_str, sizeof(time_str)))
                clk_ascii_render_update(&render, time_str);
        }

        clk_menu_instance_render(menu_inst);
        clk_term_draw();
        clk_term_sleep_ms(CLK_FRAME_MS);

        /* ── hot reload ────────────────────────────────── */
        if (++hot_tick > CLK_HOTRELOAD_TICKS) {
            hot_tick = 0;
            struct stat stat_buf;

            if (stat(APP_CONFIG, &stat_buf) == 0 && stat_buf.st_mtime != last_app) {
                last_app = stat_buf.st_mtime;
                char* raw_json2 = clk_file_read_all(APP_CONFIG, NULL);
                if (raw_json2) {
                    clk_json_value* new_config = clk_json_parse(raw_json2);
                    free(raw_json2);
                    if (new_config) {
                        clk_json_free(config);
                        config = new_config;
                        clock_section = clk_json_object_get(config, "clock");
                        menu_section = clk_json_object_get(config, "menu");
                        {
                            const char* dir_str = NULL;
                            clk_json_value* json_val;
                            json_val = clock_section
                                           ? clk_json_object_get(clock_section, "fonts_dir")
                                           : NULL;
                            if (json_val && clk_json_is_string(json_val))
                                clk_json_get_string(json_val, &dir_str);
                            fonts_dir = dir_str;
                            dir_str = NULL;
                            json_val = menu_section
                                           ? clk_json_object_get(menu_section, "themes_dir")
                                           : NULL;
                            if (json_val && clk_json_is_string(json_val))
                                clk_json_get_string(json_val, &dir_str);
                            themes_dir = dir_str;
                        }
                        free(time_formats);
                        free(time_format_options);
                        {
                            clk_json_value* array =
                                clk_json_object_get(clock_section, "time_formats");
                            if (array && clk_json_is_array(array)) {
                                time_format_count = (int)clk_json_array_count(array);
                                time_formats = calloc(time_format_count, sizeof(char*));
                                for (int i = 0; i < time_format_count; ++i) {
                                    clk_json_value* element = clk_json_array_get(array, i);
                                    const char* str = NULL;
                                    if (element && clk_json_is_string(element) &&
                                        clk_json_get_string(element, &str) == 0)
                                        time_formats[i] = (char*)str;
                                }
                            } else {
                                time_formats = NULL;
                                time_format_count = 0;
                            }
                        }
                        time_format_options = str_array_to_const(time_formats, time_format_count);
                        if (time_format_index >= time_format_count)
                            time_format_index = 0;
                        clk_menu_clear_options(menu, 0, ITEM_TFMT);
                        for (int i = 0; i < time_format_count; ++i)
                            clk_menu_add_option(menu, 0, ITEM_TFMT, time_format_options[i]);
                        clk_menu_set_value_str(menu, 0, ITEM_TFMT, time_formats[time_format_index]);
                    }
                }
            }
            if (font_index >= 0 && font_index < font_count &&
                stat(font_paths[font_index], &stat_buf) == 0 && stat_buf.st_mtime != last_font) {
                last_font = stat_buf.st_mtime;
                clk_ascii_render_change_font(&render, font_paths[font_index]);
            }
            if (theme_index >= 0 && theme_index < theme_count &&
                stat(theme_paths[theme_index], &stat_buf) == 0 && stat_buf.st_mtime != last_theme) {
                last_theme = stat_buf.st_mtime;
                clk_menu_instance_change_theme(menu_inst, theme_paths[theme_index]);
            }

            check_dir_changed(fonts_dir, &font_paths, &font_count, &font_names, &font_index, menu,
                              ITEM_FONT);
            check_dir_changed(themes_dir, &theme_paths, &theme_count, &theme_names, &theme_index,
                              menu, ITEM_THEME);
        }
    }

    /* ── save state ──────────────────────────────────────── */
    clk_json_object_set_number(config, "selected_fmt", time_format_index);
    clk_json_object_set_number(config, "selected_font", font_index);
    clk_json_object_set_number(config, "selected_theme", theme_index);
    {
        char* out = clk_json_stringify_pretty(config);
        if (out) {
            FILE* file = fopen(APP_CONFIG, "w");
            if (file) {
                fputs(out, file);
                fclose(file);
            }
            free(out);
        }
    }

    /* ── cleanup ─────────────────────────────────────────── */
    clk_menu_instance_destroy(menu_inst);
    clk_menu_destroy(menu);
    clk_menu_theme_destroy(&theme);
    clk_ascii_render_destroy(&render);
    clk_json_free(config);
    for (int i = 0; i < font_count; ++i)
        free(font_paths[i]);
    free(font_paths);
    free(time_formats);
    for (int i = 0; i < theme_count; ++i)
        free(theme_paths[i]);
    free(theme_paths);
    for (int i = 0; i < font_count; ++i)
        free((void*)font_names[i]);
    free(font_names);
    for (int i = 0; i < theme_count; ++i)
        free((void*)theme_names[i]);
    free(theme_names);
    free(time_format_options);
    clk_term_close();
    return 0;
}
