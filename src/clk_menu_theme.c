#include "clk_menu_theme.h"

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

static int register_style_obj(const clk_json_value* style) {
    /* TODO: read fg, bg, attr from JSON style object,
     *       call clk_term_register_style_hex */
    (void)style;
    return 0;
}

/* ================================================================
 *  Internal helpers — def resolution
 * ================================================================ */

static clk_menu_def* resolve_def(clk_menu_theme* theme, const clk_json_value* json_defs,
                                 const char* name);

static bool resolve_composite_members(clk_menu_theme* theme, const clk_json_value* json_defs,
                                      const clk_json_value* members_json, clk_menu_def* def) {
    /* TODO: iterate members_json array, resolve each name via
     *       resolve_def(), store in def->members[] */
    (void)theme;
    (void)json_defs;
    (void)members_json;
    (void)def;
    return true;
}

static bool resolve_special_members(clk_menu_theme* theme, const clk_json_value* json_defs,
                                    const clk_json_value* layout_json, clk_menu_def*** out_members,
                                    int* out_cnt) {
    /* TODO: iterate layout_json array, resolve each name → def*,
     *       allocate array, store in *out_members / *out_cnt */
    (void)theme;
    (void)json_defs;
    (void)layout_json;
    (void)out_members;
    (void)out_cnt;
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

    def->active_style_id = register_style_obj(act);
    def->inactive_style_id = register_style_obj(inact);
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

    def->style_id = register_style_obj(json_def);
    return def;
}

/* ---------------------------------------------------------------- */

static clk_menu_def* resolve_def(clk_menu_theme* theme, const clk_json_value* json_defs,
                                 const char* name) {
    /* already cached? */
    const clk_menu_def* cached = clk_menu_theme_find_def(theme, name);
    if (cached)
        return (clk_menu_def*)cached;

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

static bool parse_sections(clk_menu_theme* theme, const clk_json_value* json_defs,
                           const clk_json_value* json_sections) {
    /* TODO: iterate json_sections, parse each section's rows,
     *       for each row_elem resolve name → def pointer */
    (void)theme;
    (void)json_defs;
    (void)json_sections;
    return true;
}

/* ================================================================
 *  Framework parsing
 * ================================================================ */

static bool parse_framework(clk_menu_theme* theme, const clk_json_value* json_framework) {
    /* TODO: read json_framework["layout"], store section names in theme->layout[] */
    (void)theme;
    (void)json_framework;
    return true;
}

/* ================================================================
 *  Validation
 * ================================================================ */

static bool validate_theme(const clk_menu_theme* theme) {
    /* TODO:
     *   tab / item_label / item_value unique in defs
     *   tab_bar contains tab, no item_label/item_value
     *   item_list contains item_label + item_value, no tab
     *   normal contains no special composites
     *   fill values increment */
    (void)theme;
    return true;
}

/* ================================================================
 *  API
 * ================================================================ */

bool clk_menu_theme_load(const char* json_path, clk_menu_theme* theme) {
    if (!json_path || !theme)
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

    /* resolve all defs referenced from sections and framework */
    /* TODO: parse sections and framework first, triggering resolve_def,
     *       then validate */
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
    /* TODO: free all defs (name, string_val, members/active_members arrays),
     *       free all sections (name, rows.elems), free layout strings */
    (void)theme;
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
