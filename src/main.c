#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#else
#include <dirent.h>
#endif

#include "clk_clock.h"
#include "clk_json.h"
#include "clk_key_io.h"
#include "clk_menu.h"
#include "clk_menu_instance.h"
#include "clk_menu_theme.h"
#include "clk_term.h"

#define APP_CONFIG "assets/config/app_config.json"

/* ── cross-platform directory scanner ── */

#if defined(_WIN32) || defined(_WIN64)
static char** scan_json_dir(const char* dir_path, int* out_cnt) {
    *out_cnt = 0;
    char pattern[1024];
    snprintf(pattern, sizeof(pattern), "%s\\*.json", dir_path);

    WIN32_FIND_DATA fd;
    HANDLE h = FindFirstFile(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE)
        return NULL;

    char** list = NULL;
    int cnt = 0, cap = 0;
    do {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            continue;
        size_t len = strlen(fd.cFileName);
        if (len < 6 || strcmp(fd.cFileName + len - 5, ".json") != 0)
            continue;
        if (cnt >= cap) {
            cap = cap ? cap * 2 : 8;
            char** tmp = realloc(list, cap * sizeof(char*));
            if (!tmp) {
                free(list);
                FindClose(h);
                return NULL;
            }
            list = tmp;
        }
        size_t flen = strlen(dir_path) + 1 + len + 1;
        list[cnt] = malloc(flen);
        snprintf(list[cnt], flen, "%s\\%s", dir_path, fd.cFileName);
        cnt++;
    } while (FindNextFile(h, &fd));
    FindClose(h);

    *out_cnt = cnt;
    return list;
}
#else
static char** scan_json_dir(const char* dir_path, int* out_cnt) {
    *out_cnt = 0;
    DIR* d = opendir(dir_path);
    if (!d)
        return NULL;

    char** list = NULL;
    int cnt = 0, cap = 0;
    struct dirent* de;
    while ((de = readdir(d)) != NULL) {
        size_t len = strlen(de->d_name);
        if (len <= 5 || strcmp(de->d_name + len - 5, ".json") != 0)
            continue;
        if (cnt >= cap) {
            cap = cap ? cap * 2 : 8;
            char** tmp = realloc(list, cap * sizeof(char*));
            if (!tmp) {
                free(list);
                closedir(d);
                return NULL;
            }
            list = tmp;
        }
        size_t flen = strlen(dir_path) + 1 + len + 1;
        list[cnt] = malloc(flen);
        snprintf(list[cnt], flen, "%s/%s", dir_path, de->d_name);
        cnt++;
    }
    closedir(d);

    *out_cnt = cnt;
    return list;
}
#endif

static const char** str_array_to_const(char** arr, int cnt) {
    const char** v = calloc(cnt, sizeof(const char*));
    for (int i = 0; i < cnt; ++i)
        v[i] = arr[i];
    return v;
}

/* ── recenter ── */

static void recenter_clock(clk_clock* clk, int tw, int th) {
    int cw, ch;
    if (!clk_clock_get_clock_size(clk, &cw, &ch))
        return;
    clk_clock_set_sprite_pos(clk, (tw - cw) / 2, (th - ch) / 2);
}

static void recenter_menu(clk_menu_instance* inst, int tw, int th) {
    if (!inst)
        return;
    clk_menu_instance_set_position(inst, (tw - inst->tex.tex_w) / 2, (th - inst->tex.tex_h) / 2);
}

/* ── focus ── */

typedef enum { FOCUS_CLOCK, FOCUS_MENU } focus_t;

static clk_menu_input map_menu_input(clk_key_event ev) {
    switch (ev.key) {
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

int main(void) {
    if (!clk_term_init()) {
        printf("term init fail\n");
        return 1;
    }

    /* ── load app config ─────────────────────────────────── */
    char* raw = NULL;
    {
        FILE* f = fopen(APP_CONFIG, "rb");
        if (!f) {
            clk_term_close();
            printf("no app config\n");
            return 1;
        }
        fseek(f, 0, SEEK_END);
        long sz = ftell(f);
        rewind(f);
        raw = malloc(sz + 1);
        fread(raw, 1, sz, f);
        raw[sz] = 0;
        fclose(f);
    }

    clk_json_value* cfg = clk_json_parse(raw);
    free(raw);
    if (!cfg) {
        clk_term_close();
        return 1;
    }

    clk_json_value* ck = clk_json_object_get(cfg, "clock");
    clk_json_value* mn = clk_json_object_get(cfg, "menu");
    if (!ck || !mn) {
        clk_json_free(cfg);
        clk_term_close();
        return 1;
    }

    const char* fonts_dir = NULL;
    clk_json_value* jd = clk_json_object_get(ck, "fonts_dir");
    if (jd && clk_json_is_string(jd))
        clk_json_get_string(jd, &fonts_dir);

    char** font_paths;
    int fcnt;
    font_paths = fonts_dir ? scan_json_dir(fonts_dir, &fcnt) : NULL;

    char** time_fmts;
    int tcnt = 0;
    { /* parse time_formats array manually */
        clk_json_value* arr = clk_json_object_get(ck, "time_formats");
        if (arr && clk_json_is_array(arr)) {
            tcnt = clk_json_array_count(arr);
            time_fmts = calloc(tcnt, sizeof(char*));
            for (int i = 0; i < tcnt; ++i) {
                clk_json_value* e = clk_json_array_get(arr, i);
                const char* s = NULL;
                if (e && clk_json_is_string(e) && clk_json_get_string(e, &s) == 0)
                    time_fmts[i] = (char*)s;
            }
        } else
            time_fmts = NULL;
    }

    const char* themes_dir = NULL;
    jd = clk_json_object_get(mn, "themes_dir");
    if (jd && clk_json_is_string(jd))
        clk_json_get_string(jd, &themes_dir);

    char** theme_paths;
    int thcnt;
    theme_paths = themes_dir ? scan_json_dir(themes_dir, &thcnt) : NULL;

    if (!fcnt || !tcnt || !thcnt) {
        clk_json_free(cfg);
        clk_term_close();
        return 1;
    }

    /* restore last session's selections */
    int fmt_idx = 0, font_idx = 0, theme_idx = 0;
    {
        clk_json_value* jv;
        double d;
        jv = clk_json_object_get(cfg, "selected_fmt");
        if (jv && clk_json_is_number(jv) && clk_json_get_number(jv, &d) == 0)
            fmt_idx = (int)d % tcnt;
        jv = clk_json_object_get(cfg, "selected_font");
        if (jv && clk_json_is_number(jv) && clk_json_get_number(jv, &d) == 0)
            font_idx = (int)d % fcnt;
        jv = clk_json_object_get(cfg, "selected_theme");
        if (jv && clk_json_is_number(jv) && clk_json_get_number(jv, &d) == 0)
            theme_idx = (int)d % thcnt;
    }

    /* ── clock ──────────────────────────────────────────── */
    clk_clock clock;
    if (!clk_clock_create(&clock, time_fmts[fmt_idx], font_paths[font_idx])) {
        clk_json_free(cfg);
        clk_term_close();
        printf("clock create fail\n");
        return 1;
    }
    clk_clock_set_z_order(&clock, 0);
    clk_clock_add_to_term(&clock);

    /* ── menu ────────────────────────────────────────────── */
    enum { ITEM_TFMT = 1, ITEM_FONT = 2, ITEM_THEME = 3, ITEM_QUIT = 4 };

    /* build display names: strip path + .json */
    const char** fnt_disp = calloc(fcnt, sizeof(const char*));
    const char** thm_disp = calloc(thcnt, sizeof(const char*));
    for (int i = 0; i < fcnt; ++i) {
        const char* slash = NULL;
        for (const char* p = font_paths[i]; *p; ++p)
            if (*p == '/' || *p == '\\')
                slash = p;
        const char* s = slash ? slash + 1 : font_paths[i];
        char* d = strdup(s);
        char* dot = strrchr(d, '.');
        if (dot)
            *dot = '\0';
        fnt_disp[i] = d;
    }
    for (int i = 0; i < thcnt; ++i) {
        const char* slash = NULL;
        for (const char* p = theme_paths[i]; *p; ++p)
            if (*p == '/' || *p == '\\')
                slash = p;
        const char* s = slash ? slash + 1 : theme_paths[i];
        char* d = strdup(s);
        char* dot = strrchr(d, '.');
        if (dot)
            *dot = '\0';
        thm_disp[i] = d;
    }

    clk_menu* menu = clk_menu_create();
    clk_menu_add_tab(menu, 0, "menu");
    const char** tfmt_opts = str_array_to_const(time_fmts, tcnt);
    clk_menu_add_item_str(menu, 0, ITEM_TFMT, "time format", fmt_idx, tfmt_opts, tcnt);
    clk_menu_add_item_str(menu, 0, ITEM_FONT, "font", font_idx, fnt_disp, fcnt);
    clk_menu_add_item_str(menu, 0, ITEM_THEME, "theme", theme_idx, thm_disp, thcnt);
    clk_menu_add_item_action(menu, 0, ITEM_QUIT, "quit");

    clk_menu_theme theme;
    memset(&theme, 0, sizeof(theme));
    clk_menu_theme_load(theme_paths[theme_idx], &theme);

    clk_menu_instance* minst = clk_menu_instance_create(menu, &theme);
    clk_menu_instance_set_size(minst, 65, 35);
    clk_menu_instance_set_visible(minst, false);
    clk_menu_instance_add_to_term(minst);
    /* menu above clock */
    clk_sprite_set_z(minst->sprite, 5);

    /* ── initial layout ──────────────────────────────────── */
    int tw, th;
    clk_term_get_size(&tw, &th);
    recenter_clock(&clock, tw, th);
    recenter_menu(minst, tw, th);

    /* ── main loop ───────────────────────────────────────── */
    focus_t focus = FOCUS_CLOCK;
    bool running = true;

    while (running) {
        clk_key_event ev = clk_get_key_event();

        /* global quit from CLOCK */
        if (focus == FOCUS_CLOCK && (ev.key == 'q' || ev.key == 'Q')) {
            running = false;
            continue;
        }

        switch (focus) {
            case FOCUS_CLOCK:
                if (ev.key == 's' || ev.key == 'S') {
                    clk_menu_instance_set_visible(minst, true);
                    focus = FOCUS_MENU;
                    continue;
                }
                if (ev.key == 'f' || ev.key == 'F') {
                    fmt_idx = (fmt_idx + 1) % tcnt;
                    clk_clock_change_time_format(&clock, time_fmts[fmt_idx]);
                }
                if (ev.key == 'r' || ev.key == 'R') {
                    font_idx = (font_idx + 1) % fcnt;
                    clk_clock_change_font_path(&clock, font_paths[font_idx]);
                }
                break;

            case FOCUS_MENU:
                if (ev.key == 'q' || ev.key == 'Q') {
                    clk_menu_instance_set_visible(minst, false);
                    focus = FOCUS_CLOCK;
                    continue;
                }
                {
                    clk_menu_event mev = clk_menu_instance_handle_input(minst, map_menu_input(ev));
                    if (mev.type == CLK_MENU_EVENT_SUBMIT && mev.item_id == ITEM_QUIT)
                        running = false;
                    if (mev.type == CLK_MENU_EVENT_VALUE_CHANGED) {
                        switch (mev.item_id) {
                            case ITEM_TFMT:
                                for (int i = 0; i < tcnt; ++i)
                                    if (strcmp(mev.value.s, time_fmts[i]) == 0) {
                                        fmt_idx = i;
                                        break;
                                    }
                                clk_clock_change_time_format(&clock, time_fmts[fmt_idx]);
                                break;
                            case ITEM_FONT:
                                for (int i = 0; i < fcnt; ++i)
                                    if (strcmp(mev.value.s, fnt_disp[i]) == 0) {
                                        font_idx = i;
                                        break;
                                    }
                                clk_clock_change_font_path(&clock, font_paths[font_idx]);
                                break;
                            case ITEM_THEME: {
                                for (int i = 0; i < thcnt; ++i)
                                    if (strcmp(mev.value.s, thm_disp[i]) == 0) {
                                        theme_idx = i;
                                        break;
                                    }
                                int px = minst->sprite->posx, py = minst->sprite->posy;
                                int mw = minst->tex.tex_w, mh = minst->tex.tex_h;
                                clk_menu_instance_remove_from_term(minst);
                                clk_menu_instance_destroy(minst);
                                clk_menu_theme_destroy(&theme);
                                memset(&theme, 0, sizeof(theme));
                                clk_menu_theme_load(theme_paths[theme_idx], &theme);
                                minst = clk_menu_instance_create(menu, &theme);
                                clk_menu_instance_set_size(minst, mw, mh);
                                clk_menu_instance_set_position(minst, px, py);
                                clk_menu_instance_set_visible(minst, true);
                                clk_menu_instance_add_to_term(minst);
                                clk_sprite_set_z(minst->sprite, 5);
                                break;
                            }
                        }
                    }
                }
                break;
        }

        /* resize */
        if (clk_term_size_changed()) {
            clk_term_resize();
            clk_term_compact();
            clk_term_get_size(&tw, &th);
            recenter_clock(&clock, tw, th);
            recenter_menu(minst, tw, th);
        }

        recenter_clock(&clock, tw, th);
        recenter_menu(minst, tw, th);

        clk_clock_update(&clock);
        clk_menu_instance_render(minst);
        clk_term_draw();
        clk_term_sleep_ms(100);
    }

    /* ── save state ──────────────────────────────────────── */
    clk_json_object_set_number(cfg, "selected_fmt", fmt_idx);
    clk_json_object_set_number(cfg, "selected_font", font_idx);
    clk_json_object_set_number(cfg, "selected_theme", theme_idx);
    {
        char* out = clk_json_stringify_pretty(cfg);
        if (out) {
            FILE* f = fopen(APP_CONFIG, "w");
            if (f) {
                fputs(out, f);
                fclose(f);
            }
            free(out);
        }
    }

    /* ── cleanup ─────────────────────────────────────────── */
    clk_menu_instance_destroy(minst);
    clk_menu_destroy(menu);
    clk_menu_theme_destroy(&theme);
    clk_clock_destroy(&clock);
    clk_json_free(cfg);
    for (int i = 0; i < fcnt; ++i)
        free(font_paths[i]);
    free(font_paths);
    free(time_fmts);
    for (int i = 0; i < thcnt; ++i)
        free(theme_paths[i]);
    free(theme_paths);
    for (int i = 0; i < fcnt; ++i)
        free((void*)fnt_disp[i]);
    free(fnt_disp);
    for (int i = 0; i < thcnt; ++i)
        free((void*)thm_disp[i]);
    free(thm_disp);
    free(tfmt_opts);
    clk_term_close();
    return 0;
}
