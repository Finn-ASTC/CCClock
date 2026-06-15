#include "clk_menu_theme.h"

#include <stdlib.h>
#include <string.h>

#include "clk_json.h"
#include "clk_term.h"

/* ================================================================
 *  Internal helpers
 * ================================================================ */

static int register_style_obj(const clk_json_value* style_obj) {
    /* TODO: read fg, bg, attr from JSON, call clk_term_register_style_hex */
    return 0;
}

static void resolve_refs(struct clk_menu_def** defs, int def_count) {
    /* TODO: for each def, replace char** members → def**, free intermediate strings */
}

static bool validate_theme(const clk_menu_theme* theme) {
    /* TODO: tab / item_label / item_value unique;
     *       tab_bar contains tab, no item_label/item_value;
     *       item_list contains item_label + item_value, no tab;
     *       normal contains no special composites;
     *       fill values increment */
    return true;
}

/* ================================================================
 *  API
 * ================================================================ */

bool clk_menu_theme_load(const char* json_path, clk_menu_theme* theme) {
    /* TODO:
     *   1. Read file + clk_json_parse
     *   2. Pass 1: parse defs (leaf / composite / special composite)
     *              parse sections (rows → row_elem ref + fill)
     *              parse framework.layout
     *   3. Pass 2: resolve ref names → def pointers
     *   4. validate_theme()
     *   5. return true / false
     */
    return false;
}

bool clk_menu_theme_reload(const char* json_path, clk_menu_theme* theme) {
    /* TODO: clk_menu_theme_destroy + memset + clk_menu_theme_load */
    return false;
}

void clk_menu_theme_destroy(clk_menu_theme* theme) {
    /* TODO: free all defs + sections + layout strings */
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
