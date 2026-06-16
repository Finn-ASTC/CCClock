#include "clk_menu_theme.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "clk_json.h"
#include "clk_term.h"

/* ================================================================
 *  Internal helpers — file I/O
 * ================================================================ */

static char* read_file(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f)
        return NULL;

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    if (sz < 0) {
        fclose(f);
        return NULL;
    }
    rewind(f);

    char* buf = malloc(sz + 1);
    if (!buf) {
        fclose(f);
        return NULL;
    }

    size_t n = fread(buf, 1, sz, f);
    fclose(f);

    if (n != (size_t)sz) {
        free(buf);
        return NULL;
    }

    buf[sz] = '\0';
    return buf;
}

/* ================================================================
 *  Internal helpers — style registration
 * ================================================================ */

static int register_inline_style(const clk_json_value* obj) {
    if (!clk_term_is_init() || !obj)
        return -1;

    const clk_json_value* fg_json = clk_json_object_get(obj, "fg");
    const clk_json_value* bg_json = clk_json_object_get(obj, "bg");
    if (!fg_json || !bg_json)
        return -1;

    const char* fg = NULL;
    const char* bg = NULL;
    const char* attr = NULL;

    if (clk_json_get_string(fg_json, &fg) || clk_json_get_string(bg_json, &bg))
        return -1;

    const clk_json_value* attr_json = clk_json_object_get(obj, "attr");
    if (attr_json)
        clk_json_get_string(attr_json, &attr);

    int style_id = clk_term_register_style_hex(fg, bg, attr);
    return style_id == 0 ? -1 : style_id;
}

/* ================================================================
 *  Internal helpers — def resolution
 * ================================================================ */

static clk_menu_def* resolve_def(clk_menu_theme* theme, const clk_json_value* json_defs,
                                 const char* name);

static bool resolve_composite_members(clk_menu_theme* theme, const clk_json_value* json_defs,
                                      const clk_json_value* members_json, clk_menu_def* def) {
    int cnt = clk_json_array_count(members_json);
    if (cnt == 0) {
        def->members = NULL;
        def->member_cnt = 0;
        return true;
    }

    clk_menu_def** arr = malloc(cnt * sizeof(clk_menu_def*));
    if (!arr)
        return false;

    for (size_t i = 0; i < cnt; ++i) {
        const clk_json_value* elem = clk_json_array_get(members_json, i);
        const char* name = NULL;
        if (!elem || !clk_json_is_string(elem) || clk_json_get_string(elem, &name) != 0) {
            free(arr);
            return false;
        }

        const clk_menu_def* found = clk_menu_theme_find_def(theme, name);
        arr[i] = found ? (clk_menu_def*)found : resolve_def(theme, json_defs, name);
        if (!arr[i]) {
            free(arr);
            return false;
        }
    }

    def->members = arr;
    def->member_cnt = (int)cnt;
    return true;
}

static bool resolve_special_members(clk_menu_theme* theme, const clk_json_value* json_defs,
                                    const clk_json_value* layout_json, clk_menu_def*** out_members,
                                    int* out_cnt) {
    int cnt = clk_json_array_count(layout_json);
    if (cnt == 0) {
        *out_members = NULL;
        *out_cnt = 0;
        return true;
    }

    clk_menu_def** arr = malloc(cnt * sizeof(clk_menu_def*));
    if (!arr)
        return false;

    for (size_t i = 0; i < cnt; ++i) {
        const clk_json_value* elem = clk_json_array_get(layout_json, i);
        const char* name = NULL;
        if (!elem || !clk_json_is_string(elem) || clk_json_get_string(elem, &name) != 0) {
            free(arr);
            return false;
        }

        const clk_menu_def* found = clk_menu_theme_find_def(theme, name);
        arr[i] = found ? (clk_menu_def*)found : resolve_def(theme, json_defs, name);
        if (!arr[i]) {
            free(arr);
            return false;
        }
    }

    *out_members = arr;
    *out_cnt = (int)cnt;
    return true;
}

static clk_menu_def* resolve_leaf_dyn(clk_menu_def* def, const clk_json_value* json_def,
                                      const char* type_str) {
    const clk_json_value* act = clk_json_object_get(json_def, "active");
    const clk_json_value* inact = clk_json_object_get(json_def, "inactive");
    if (!act || !inact)
        return NULL;

    if (strcmp(type_str, "tab_str") == 0)
        def->type = CLK_MENU_DEF_TAB_STR;
    else if (strcmp(type_str, "item_label_str") == 0)
        def->type = CLK_MENU_DEF_ITEM_LABEL_STR;
    else
        def->type = CLK_MENU_DEF_ITEM_VALUE_STR;

    def->active_style_id = register_inline_style(act);
    def->inactive_style_id = register_inline_style(inact);
    return def;
}

static clk_menu_def* resolve_special(clk_menu_theme* theme, const clk_json_value* json_defs,
                                     clk_menu_def* def, const clk_json_value* json_def,
                                     const char* type_str) {
    const clk_json_value* act = clk_json_object_get(json_def, "active");
    const clk_json_value* inact = clk_json_object_get(json_def, "inactive");
    if (!act || !inact)
        return NULL;

    if (strcmp(type_str, "tab") == 0)
        def->type = CLK_MENU_DEF_TAB;
    else if (strcmp(type_str, "item_label") == 0)
        def->type = CLK_MENU_DEF_ITEM_LABEL;
    else
        def->type = CLK_MENU_DEF_ITEM_VALUE;

    if (!resolve_special_members(theme, json_defs, act, &def->active_members, &def->active_cnt))
        return NULL;
    if (!resolve_special_members(theme, json_defs, inact, &def->inactive_members,
                                 &def->inactive_cnt))
        return NULL;
    return def;
}

static clk_menu_def* resolve_leaf_string(clk_menu_def* def, const clk_json_value* json_def) {
    def->type = CLK_MENU_DEF_STRING;

    const char* str = NULL;
    const clk_json_value* jstr = clk_json_object_get(json_def, "string");
    if (jstr && clk_json_is_string(jstr))
        clk_json_get_string(jstr, &str);
    def->string_val = str ? strdup(str) : NULL;

    def->style_id = register_inline_style(json_def);
    return def;
}

/* ---------------------------------------------------------------- */

static clk_menu_def* resolve_def(clk_menu_theme* theme, const clk_json_value* json_defs,
                                 const char* name) {
    const clk_json_value* json_def = clk_json_object_get(json_defs, name);
    if (!json_def)
        return NULL;

    clk_menu_def* def = malloc(sizeof(clk_menu_def));
    if (!def)
        return NULL;
    memset(def, 0, sizeof(clk_menu_def));
    def->name = strdup(name);

    /* insert into theme defs[] */
    clk_menu_def** tmp = realloc(theme->defs, (theme->def_count + 1) * sizeof(clk_menu_def*));
    if (!tmp) {
        free(def->name);
        free(def);
        return NULL;
    }
    tmp[theme->def_count] = def;
    theme->defs = tmp;
    theme->def_count++;

    /* composite? */
    if (clk_json_is_array(json_def)) {
        def->type = CLK_MENU_DEF_COMPOSITE;
        if (!resolve_composite_members(theme, json_defs, json_def, def))
            return NULL;
        return def;
    }

    /* must be an object from here on */
    if (!clk_json_is_object(json_def))
        return NULL;

    const char* type_str = NULL;
    const clk_json_value* jtype = clk_json_object_get(json_def, "type");
    if (!jtype || !clk_json_is_string(jtype))
        return NULL;
    clk_json_get_string(jtype, &type_str);
    if (!type_str)
        return NULL;

    if (strcmp(type_str, "tab_str") == 0 || strcmp(type_str, "item_label_str") == 0 ||
        strcmp(type_str, "item_value_str") == 0) {
        return resolve_leaf_dyn(def, json_def, type_str);
    }
    if (strcmp(type_str, "tab") == 0 || strcmp(type_str, "item_label") == 0 ||
        strcmp(type_str, "item_value") == 0) {
        return resolve_special(theme, json_defs, def, json_def, type_str);
    }
    return resolve_leaf_string(def, json_def);
}

/* ================================================================
 *  Section parsing
 * ================================================================ */

static bool parse_single_row(clk_menu_theme* theme, const clk_json_value* json_defs,
                             const clk_json_value* json_row, clk_menu_row* row_out) {
    int cnt = clk_json_array_count(json_row);
    if (cnt == 0) {
        row_out->elems = NULL;
        row_out->count = 0;
        return true;
    }

    clk_menu_row_elem* elems = malloc(cnt * sizeof(clk_menu_row_elem));
    if (!elems)
        return false;

    for (int i = 0; i < cnt; ++i) {
        const clk_json_value* elem = clk_json_array_get(json_row, i);
        memset(&elems[i], 0, sizeof(elems[i]));
        elems[i].fill = -1.0;

        if (clk_json_is_string(elem)) {
            /* shorthand: "name" */
            const char* name = NULL;
            if (clk_json_get_string(elem, &name) != 0) {
                free(elems);
                return false;
            }
            const clk_menu_def* d = clk_menu_theme_find_def(theme, name);
            elems[i].def = d ? (clk_menu_def*)d : resolve_def(theme, json_defs, name);

        } else if (clk_json_is_object(elem)) {
            /* { "ref": "name", "fill": 0.30 } */
            const clk_json_value* ref = clk_json_object_get(elem, "ref");
            if (!ref || !clk_json_is_string(ref)) {
                free(elems);
                return false;
            }
            const char* name = NULL;
            if (clk_json_get_string(ref, &name) != 0) {
                free(elems);
                return false;
            }
            const clk_menu_def* d = clk_menu_theme_find_def(theme, name);
            elems[i].def = d ? (clk_menu_def*)d : resolve_def(theme, json_defs, name);

            const clk_json_value* fill = clk_json_object_get(elem, "fill");
            if (fill && clk_json_is_number(fill))
                clk_json_get_number(fill, &elems[i].fill);
        } else {
            free(elems);
            return false;
        }

        if (!elems[i].def) {
            free(elems);
            return false;
        }
    }

    row_out->elems = elems;
    row_out->count = cnt;
    return true;
}

static bool parse_sections(clk_menu_theme* theme, const clk_json_value* json_defs,
                           const clk_json_value* json_sections) {
    /* count sections */
    int sec_cnt = (int)clk_json_object_count(json_sections);

    clk_menu_section* secs = malloc(sec_cnt * sizeof(clk_menu_section));
    if (!secs)
        return false;
    memset(secs, 0, sec_cnt * sizeof(clk_menu_section));

    /* parse each section */
    clk_json_object_iterator iter;
    clk_json_key_value_pair pair;
    if (clk_json_object_iterator_init(json_sections, &iter) != 0) {
        free(secs);
        return false;
    }

    int si = 0;
    while (clk_json_object_iterator_next(&iter, &pair) && si < sec_cnt) {
        secs[si].name = strdup(pair.key);
        if (!secs[si].name) {
            si++;
            continue;
        }
        const clk_json_value* sec = pair.value;
        if (!clk_json_is_object(sec)) {
            free(secs[si].name);
            si++;
            continue;
        }

        const char* type_str = NULL;
        const clk_json_value* jt = clk_json_object_get(sec, "type");
        if (!jt || !clk_json_is_string(jt) || clk_json_get_string(jt, &type_str) != 0) {
            free(secs[si].name);
            si++;
            continue;
        }

        if (strcmp(type_str, "tab_bar") == 0)
            secs[si].type = CLK_MENU_SEC_TAB_BAR;
        else if (strcmp(type_str, "item_list") == 0)
            secs[si].type = CLK_MENU_SEC_ITEM_LIST;
        else
            secs[si].type = CLK_MENU_SEC_NORMAL;

        const clk_json_value* rows = clk_json_object_get(sec, "rows");

        if (rows && clk_json_is_array(rows)) {
            int row_cnt = clk_json_array_count(rows);
            clk_menu_row* row_arr = calloc(row_cnt, sizeof(clk_menu_row));
            if (!row_arr) {
                free(secs[si].name);
                si++;
                continue;
            }
            bool ok = true;
            for (int ri = 0; ri < row_cnt; ++ri) {
                const clk_json_value* json_row = clk_json_array_get(rows, ri);
                if (!json_row || !clk_json_is_array(json_row)) {
                    ok = false;
                    break;
                }
                if (!parse_single_row(theme, json_defs, json_row, &row_arr[ri])) {
                    ok = false;
                    break;
                }
            }
            if (!ok) {
                for (int ri = 0; ri < row_cnt; ++ri)
                    free(row_arr[ri].elems);
                free(row_arr);
                free(secs[si].name);
                si++;
                continue;
            }
            secs[si].rows = row_arr;
            secs[si].row_count = row_cnt;
        }

        si++;
    }

    theme->sections = secs;
    theme->section_count = si;
    return true;
}

/* ================================================================
 *  Framework parsing
 * ================================================================ */
static bool parse_framework(clk_menu_theme* theme, const clk_json_value* json_framework) {
    const clk_json_value* layout = clk_json_object_get(json_framework, "layout");
    if (!layout || !clk_json_is_array(layout))
        return false;

    int cnt = clk_json_array_count(layout);
    char** names = calloc(cnt, sizeof(char*));
    if (!names)
        return false;

    for (int i = 0; i < cnt; ++i) {
        const clk_json_value* elem = clk_json_array_get(layout, i);
        const char* str = NULL;
        if (!elem || !clk_json_is_string(elem) || clk_json_get_string(elem, &str) != 0) {
            for (int j = 0; j < i; ++j)
                free(names[j]);
            free(names);
            return false;
        }
        names[i] = strdup(str);
        if (!names[i]) {
            for (int j = 0; j < i; ++j)
                free(names[j]);
            free(names);
            return false;
        }
    }

    theme->layout = names;
    theme->layout_count = cnt;
    return true;
}

/* ================================================================
 *  Validation
 * ================================================================ */

static bool composite_contains(const clk_menu_def* def, clk_menu_def_type expected, int min_count) {
    int act = 0, inact = 0;
    for (int i = 0; i < def->active_cnt; ++i)
        if (def->active_members[i]->type == expected)
            act++;
    for (int i = 0; i < def->inactive_cnt; ++i)
        if (def->inactive_members[i]->type == expected)
            inact++;
    return act >= min_count && inact >= min_count;
}

static bool validate_theme(const clk_menu_theme* theme) {
    int tab_cnt = 0, il_cnt = 0, iv_cnt = 0;

    /* pass 1: unique special composites + internal structure */
    for (int i = 0; i < theme->def_count; ++i) {
        const clk_menu_def* def = theme->defs[i];
        switch (def->type) {
            case CLK_MENU_DEF_TAB:
                if (!composite_contains(def, CLK_MENU_DEF_TAB_STR, 1))
                    return false;
                tab_cnt++;
                break;
            case CLK_MENU_DEF_ITEM_LABEL:
                if (!composite_contains(def, CLK_MENU_DEF_ITEM_LABEL_STR, 1))
                    return false;
                il_cnt++;
                break;
            case CLK_MENU_DEF_ITEM_VALUE:
                if (!composite_contains(def, CLK_MENU_DEF_ITEM_VALUE_STR, 1))
                    return false;
                iv_cnt++;
                break;
            default:
                break;
        }
    }
    if (tab_cnt != 1 || il_cnt != 1 || iv_cnt != 1)
        return false;

    /* pass 2: section constraints */
    for (int si = 0; si < theme->section_count; ++si) {
        const clk_menu_section* sec = &theme->sections[si];
        bool has_tab = false, has_il = false, has_iv = false;

        for (int ri = 0; ri < sec->row_count; ++ri) {
            for (int ei = 0; ei < sec->rows[ri].count; ++ei) {
                clk_menu_def_type t = sec->rows[ri].elems[ei].def->type;
                if (t == CLK_MENU_DEF_TAB)
                    has_tab = true;
                if (t == CLK_MENU_DEF_ITEM_LABEL)
                    has_il = true;
                if (t == CLK_MENU_DEF_ITEM_VALUE)
                    has_iv = true;
            }
        }

        if (sec->type == CLK_MENU_SEC_TAB_BAR) {
            if (!has_tab || has_il || has_iv)
                return false;
        } else if (sec->type == CLK_MENU_SEC_ITEM_LIST) {
            if (!has_il || !has_iv || has_tab)
                return false;
        } else {
            if (has_tab || has_il || has_iv)
                return false;
        }

        /* fill required on special composites */
        for (int ri = 0; ri < sec->row_count; ++ri) {
            for (int ei = 0; ei < sec->rows[ri].count; ++ei) {
                clk_menu_def_type t = sec->rows[ri].elems[ei].def->type;
                double f = sec->rows[ri].elems[ei].fill;
                if (sec->type == CLK_MENU_SEC_TAB_BAR) {
                    if (t == CLK_MENU_DEF_TAB && f < 0.0)
                        return false;
                }
                if (sec->type == CLK_MENU_SEC_ITEM_LIST) {
                    if ((t == CLK_MENU_DEF_ITEM_LABEL || t == CLK_MENU_DEF_ITEM_VALUE) && f < 0.0)
                        return false;
                }
            }
        }
    }

    /* pass 3: fill anchors increment */
    for (int si = 0; si < theme->section_count; ++si) {
        for (int ri = 0; ri < theme->sections[si].row_count; ++ri) {
            const clk_menu_row* row = &theme->sections[si].rows[ri];
            double prev = -1.0;
            for (int ei = 0; ei < row->count; ++ei) {
                double f = row->elems[ei].fill;
                if (f >= 0.0) {
                    if (f <= prev)
                        return false;
                    prev = f;
                }
            }
        }
    }

    /* pass 4: layout references must exist */
    for (int i = 0; i < theme->layout_count; ++i) {
        if (!clk_menu_theme_find_section(theme, theme->layout[i]))
            return false;
    }

    return true;
}

/* ================================================================
 *  API
 * ================================================================ */

bool clk_menu_theme_load(const char* json_path, clk_menu_theme* theme) {
    if (!json_path || !theme || !clk_term_is_init())
        return false;

    char* file_content = read_file(json_path);
    if (!file_content)
        return false;

    clk_json_value* json = clk_json_parse(file_content);
    free(file_content);
    if (!json)
        return false;

    const clk_json_value* json_defs = clk_json_object_get(json, "defs");
    const clk_json_value* json_sections = clk_json_object_get(json, "sections");
    const clk_json_value* json_framework = clk_json_object_get(json, "framework");

    if (!json_defs || !json_sections || !json_framework) {
        clk_json_free(json);
        return false;
    }

    bool ok = parse_sections(theme, json_defs, json_sections) &&
              parse_framework(theme, json_framework) && validate_theme(theme);

    clk_json_free(json);
    return ok;
}

bool clk_menu_theme_reload(const char* json_path, clk_menu_theme* theme) {
    if (!theme)
        return false;
    clk_menu_theme_destroy(theme);
    memset(theme, 0, sizeof(*theme));
    return clk_menu_theme_load(json_path, theme);
}

void clk_menu_theme_destroy(clk_menu_theme* theme) {
    if (!theme)
        return;

    for (int i = 0; i < theme->def_count; ++i) {
        clk_menu_def* def = theme->defs[i];
        free(def->name);
        free(def->string_val);
        free(def->members);
        free(def->active_members);
        free(def->inactive_members);
        free(def);
    }
    free(theme->defs);

    for (int i = 0; i < theme->section_count; ++i) {
        free(theme->sections[i].name);
        for (int j = 0; j < theme->sections[i].row_count; ++j)
            free(theme->sections[i].rows[j].elems);
        free(theme->sections[i].rows);
    }
    free(theme->sections);

    for (int i = 0; i < theme->layout_count; ++i)
        free(theme->layout[i]);
    free(theme->layout);

    memset(theme, 0, sizeof(*theme));
}

const clk_menu_def* clk_menu_theme_find_def(const clk_menu_theme* theme, const char* name) {
    if (!theme || !name)
        return NULL;
    for (int i = 0; i < theme->def_count; ++i) {
        if (strcmp(theme->defs[i]->name, name) == 0)
            return theme->defs[i];
    }
    return NULL;
}

const clk_menu_section* clk_menu_theme_find_section(const clk_menu_theme* theme, const char* name) {
    if (!theme || !name)
        return NULL;
    for (int i = 0; i < theme->section_count; ++i) {
        if (strcmp(theme->sections[i].name, name) == 0)
            return &theme->sections[i];
    }
    return NULL;
}
