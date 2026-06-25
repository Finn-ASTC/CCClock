#include "clk_menu_theme.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "clk_file_util.h"
#include "clk_json.h"
#include "clk_term.h"

#define CLK_MENU_DYNAMIC_FILL_EST_WIDTH 15

/* ================================================================
 *  Internal helpers — file I/O
 * ================================================================ */

/** Reads an entire file into a newly allocated, NUL-terminated buffer.
 *  Returns the buffer (caller frees), or NULL on open/read/alloc failure. */
static char* read_file(const char* path) {
    return clk_file_read_all(path, NULL);
}

/* ================================================================
 *  Internal helpers — style registration
 * ================================================================ */

/** Registers a terminal style from an inline JSON object's fg/bg/attr fields.
 *  Returns the new style id, or -1 if the terminal is uninitialised or fg/bg are missing. */
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

/** Resolves the member defs of a composite, reusing existing defs or resolving them on demand.
 *  Stores the member array on def. Returns true on success, false on alloc/resolution failure. */
static bool resolve_composite_members(clk_menu_theme* theme, const clk_json_value* json_defs,
                                      const clk_json_value* members_json, clk_menu_def* def) {
    int cnt = (int)clk_json_array_count(members_json);
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

/** Resolves the active/inactive layout members of a special composite from a JSON array.
 *  Outputs the member array and count. Returns true on success, false on failure. */
static bool resolve_special_members(clk_menu_theme* theme, const clk_json_value* json_defs,
                                    const clk_json_value* layout_json, clk_menu_def*** out_members,
                                    int* out_cnt) {
    int cnt = (int)clk_json_array_count(layout_json);
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

/** Resolves a dynamic string leaf (tab_str / item_label_str / item_value_str) with active/inactive
 * styles. Returns def on success, or NULL when the active/inactive fields are missing. */
static clk_menu_def* resolve_leaf_dyn(clk_menu_def* def, const clk_json_value* json_def,
                                      const char* type_str) {
    const clk_json_value* active = clk_json_object_get(json_def, "active");
    const clk_json_value* inactive = clk_json_object_get(json_def, "inactive");
    if (!active || !inactive)
        return NULL;

    if (strcmp(type_str, "tab_str") == 0)
        def->type = CLK_MENU_DEF_TAB_STR;
    else if (strcmp(type_str, "item_label_str") == 0)
        def->type = CLK_MENU_DEF_ITEM_LABEL_STR;
    else
        def->type = CLK_MENU_DEF_ITEM_VALUE_STR;

    def->active_style_id = register_inline_style(active);
    def->inactive_style_id = register_inline_style(inactive);
    return def;
}

/** Resolves a special composite (tab / item_label / item_value) and its active/inactive member
 * layouts. Returns def on success, or NULL on missing fields or member-resolution failure. */
static clk_menu_def* resolve_special(clk_menu_theme* theme, const clk_json_value* json_defs,
                                     clk_menu_def* def, const clk_json_value* json_def,
                                     const char* type_str) {
    const clk_json_value* active = clk_json_object_get(json_def, "active");
    const clk_json_value* inactive = clk_json_object_get(json_def, "inactive");
    if (!active || !inactive)
        return NULL;

    if (strcmp(type_str, "tab") == 0)
        def->type = CLK_MENU_DEF_TAB;
    else if (strcmp(type_str, "item_label") == 0)
        def->type = CLK_MENU_DEF_ITEM_LABEL;
    else
        def->type = CLK_MENU_DEF_ITEM_VALUE;

    if (!resolve_special_members(theme, json_defs, active, &def->active_members, &def->active_cnt))
        return NULL;
    if (!resolve_special_members(theme, json_defs, inactive, &def->inactive_members,
                                 &def->inactive_cnt))
        return NULL;
    return def;
}

/** Resolves a plain string leaf, copying its literal text and registering its inline style. Returns
 * def. */
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

/* ================================================================
 *  Internal helpers — def dispatch
 * ================================================================ */

/** Resolves a def by name, allocating and registering it in the theme before recursing so that
 *  members may reference defs still under construction. Returns the def, or NULL on failure. */
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

/** Parses one layout row (an array of def names or {ref,fill} objects) into a row of elements.
 *  Returns true on success, false on allocation or unresolved-reference failure. */
static bool parse_single_row(clk_menu_theme* theme, const clk_json_value* json_defs,
                             const clk_json_value* json_row, clk_menu_row* row_out) {
    int cnt = (int)clk_json_array_count(json_row);
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
            /* { "ref": "name", "fill": 0.30 } — default fill=1.0 when no key */
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

/** Parses every section listed in the framework layout order into the theme's section array.
 *  Returns true on success; on failure frees any partially built sections and returns false. */
static bool parse_sections(clk_menu_theme* theme, const clk_json_value* json_defs,
                           const clk_json_value* json_sections, const clk_json_value* json_layout) {
    int sec_cnt = (int)clk_json_array_count(json_layout);
    if (sec_cnt <= 0)
        return false;

    clk_menu_section* secs = calloc(sec_cnt, sizeof(clk_menu_section));
    if (!secs)
        return false;

    int si;
    for (si = 0; si < sec_cnt; ++si) {
        const clk_json_value* name_elem = clk_json_array_get(json_layout, si);
        const char* name = NULL;
        if (!name_elem || !clk_json_is_string(name_elem) ||
            clk_json_get_string(name_elem, &name) != 0)
            goto fail_sections;

        const clk_json_value* json_sec = clk_json_object_get(json_sections, name);
        if (!json_sec || !clk_json_is_object(json_sec))
            goto fail_sections;

        secs[si].name = strdup(name);
        if (!secs[si].name)
            goto fail_sections;

        const char* type_str = NULL;
        const clk_json_value* jt = clk_json_object_get(json_sec, "type");
        if (!jt || !clk_json_is_string(jt) || clk_json_get_string(jt, &type_str) != 0)
            goto fail_sections;

        if (strcmp(type_str, "tab_bar") == 0)
            secs[si].type = CLK_MENU_SEC_TAB_BAR;
        else if (strcmp(type_str, "item_list") == 0)
            secs[si].type = CLK_MENU_SEC_ITEM_LIST;
        else
            secs[si].type = CLK_MENU_SEC_NORMAL;

        const clk_json_value* rows = clk_json_object_get(json_sec, "rows");
        if (!rows || !clk_json_is_array(rows))
            goto fail_sections;

        int row_cnt = (int)clk_json_array_count(rows);
        clk_menu_row* row_arr = calloc(row_cnt, sizeof(clk_menu_row));
        if (!row_arr)
            goto fail_sections;

        bool ok = true;
        for (int ri = 0; ri < row_cnt; ++ri) {
            const clk_json_value* json_row = clk_json_array_get(rows, ri);

            if (clk_json_is_string(json_row)) {
                /* shorthand: single def name → 1-element row */
                const char* n = NULL;
                clk_json_get_string(json_row, &n);
                row_arr[ri].count = 1;
                row_arr[ri].elems = calloc(1, sizeof(clk_menu_row_elem));
                row_arr[ri].elems[0].fill = -1.0;
                const clk_menu_def* d = clk_menu_theme_find_def(theme, n);
                row_arr[ri].elems[0].def = d ? (clk_menu_def*)d : resolve_def(theme, json_defs, n);
                if (!row_arr[ri].elems[0].def) {
                    ok = false;
                    break;
                }
            } else if (clk_json_is_array(json_row)) {
                if (!parse_single_row(theme, json_defs, json_row, &row_arr[ri])) {
                    ok = false;
                    break;
                }
            } else {
                ok = false;
                break;
            }
        }
        if (!ok) {
            for (int ri = 0; ri < row_cnt; ++ri)
                free(row_arr[ri].elems);
            free(row_arr);
            goto fail_sections;
        }
        secs[si].rows = row_arr;
        secs[si].row_count = row_cnt;
    }

    theme->sections = secs;
    theme->section_count = sec_cnt;
    return true;

fail_sections:
    for (int i = 0; i < si; ++i) {
        free(secs[i].name);
        for (int j = 0; j < secs[i].row_count; ++j)
            free(secs[i].rows[j].elems);
        free(secs[i].rows);
    }
    free(secs);
    return false;
}

/* ================================================================
 *  Min-size computation
 * ================================================================ */

/** Computes the rendered width of a leaf def, summing members for composites.
 *  Returns the width in columns; dynamic special defs use a fixed estimate. */
static int leaf_width(const clk_menu_def* def) {
    if (!def)
        return 0;
    switch (def->type) {
        case CLK_MENU_DEF_STRING:
            return def->string_val ? (int)strlen(def->string_val) : 0;
        case CLK_MENU_DEF_COMPOSITE: {
            int w = 0;
            for (int i = 0; i < def->member_cnt; ++i)
                w += leaf_width(def->members[i]);
            return w;
        }
        case CLK_MENU_DEF_TAB:
        case CLK_MENU_DEF_ITEM_LABEL:
        case CLK_MENU_DEF_ITEM_VALUE:
            /* dynamic fill — estimated */
            return CLK_MENU_DYNAMIC_FILL_EST_WIDTH;
        default:
            /* dynamic leaf without fill (rare) — tiny estimate */
            return 2;
    }
}

/** Computes the theme's minimum width and height from its sections and rows, storing both on the
 * theme. */
static void compute_min_size(clk_menu_theme* theme) {
    int min_w = 0, min_h = 0;

    for (int si = 0; si < theme->section_count; ++si) {
        const clk_menu_section* sec = &theme->sections[si];
        min_h += sec->row_count;

        for (int ri = 0; ri < sec->row_count; ++ri) {
            int row_w = 0;
            for (int ei = 0; ei < sec->rows[ri].count; ++ei) {
                const clk_menu_row_elem* elem = &sec->rows[ri].elems[ei];
                if (elem->fill >= 0.0) {
                    clk_menu_def_type t = elem->def->type;
                    if (t == CLK_MENU_DEF_TAB || t == CLK_MENU_DEF_ITEM_LABEL ||
                        t == CLK_MENU_DEF_ITEM_VALUE)
                        row_w += CLK_MENU_DYNAMIC_FILL_EST_WIDTH;
                    /* plain fill leaves contribute 0 */
                } else {
                    row_w += leaf_width(elem->def);
                }
            }
            if (row_w > min_w)
                min_w = row_w;
        }
    }

    theme->min_width = min_w;
    theme->min_height = min_h;
}

/* ================================================================
 *  Validation
 * ================================================================ */

/** Checks that the active and inactive member lists of a special composite each hold at least
 *  min_count members of the expected type. Returns true when both satisfy the count. */
static bool composite_contains(const clk_menu_def* def, clk_menu_def_type expected, int min_count) {
    int active = 0, inactive = 0;
    for (int i = 0; i < def->active_cnt; ++i)
        if (def->active_members[i]->type == expected)
            active++;
    for (int i = 0; i < def->inactive_cnt; ++i)
        if (def->inactive_members[i]->type == expected)
            inactive++;
    return active >= min_count && inactive >= min_count;
}

/** Validates structural invariants: exactly one of each special composite, per-section type
 *  constraints, required fills, and strictly increasing fill anchors. Returns true if valid. */
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

    const clk_json_value* layout_order = clk_json_object_get(json_framework, "layout");
    if (!layout_order || !clk_json_is_array(layout_order)) {
        clk_json_free(json);
        return false;
    }

    bool ok =
        parse_sections(theme, json_defs, json_sections, layout_order) && validate_theme(theme);

    if (ok)
        compute_min_size(theme);

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
