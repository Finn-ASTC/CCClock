#ifndef CLK_MENU_THEME_H
#define CLK_MENU_THEME_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ================================================================
 *  Def — unified recursive definition
 * ================================================================ */

typedef enum {
    CLK_MENU_DEF_STRING,
    CLK_MENU_DEF_TAB_STR,
    CLK_MENU_DEF_ITEM_LABEL_STR,
    CLK_MENU_DEF_ITEM_VALUE_STR,
    CLK_MENU_DEF_COMPOSITE,
    CLK_MENU_DEF_TAB,
    CLK_MENU_DEF_ITEM_LABEL,
    CLK_MENU_DEF_ITEM_VALUE,
} clk_menu_def_type;

typedef struct clk_menu_def {
    char* name;
    clk_menu_def_type type;

    /* STRING leaf */
    char* string_val;
    int style_id;

    /* dynamic leaf */
    int active_style_id;
    int inactive_style_id;

    /* COMPOSITE */
    struct clk_menu_def** members;
    int member_cnt;

    /* special composite */
    struct clk_menu_def** active_members;
    int active_cnt;
    struct clk_menu_def** inactive_members;
    int inactive_cnt;
} clk_menu_def;

/* ================================================================
 *  Row — elements inside a single line
 * ================================================================ */

typedef struct {
    struct clk_menu_def* def;
    double fill; /* -1.0 = none, 0.0~1.0 = anchor */
} clk_menu_row_elem;

typedef struct {
    clk_menu_row_elem* elems;
    int count;
} clk_menu_row;

/* ================================================================
 *  Section — a block of rows
 * ================================================================ */

typedef enum {
    CLK_MENU_SEC_NORMAL,
    CLK_MENU_SEC_TAB_BAR,
    CLK_MENU_SEC_ITEM_LIST,
} clk_menu_section_type;

typedef struct {
    char* name;
    clk_menu_section_type type;
    clk_menu_row* rows;
    int row_count;
} clk_menu_section;

/* ================================================================
 *  Theme — top-level container
 * ================================================================ */

typedef struct clk_menu_theme {
    clk_menu_def** defs;
    int def_count;
    clk_menu_section* sections;
    int section_count;
    char** layout;
    int layout_count;
} clk_menu_theme;

/* ================================================================
 *  API
 * ================================================================ */

/** Load and parse a menu theme JSON file.
 *  Must have called clk_term_init() first (styles are registered). */
bool clk_menu_theme_load(const char* json_path, clk_menu_theme* theme);

/** Destroy + memset + re-load from the same or a different path. */
bool clk_menu_theme_reload(const char* json_path, clk_menu_theme* theme);

/** Free all memory owned by the theme. */
void clk_menu_theme_destroy(clk_menu_theme* theme);

/** Look up a def by name. Returns NULL if not found. */
const clk_menu_def* clk_menu_theme_find_def(const clk_menu_theme* theme, const char* name);

/** Look up a section by name. Returns NULL if not found. */
const clk_menu_section* clk_menu_theme_find_section(const clk_menu_theme* theme, const char* name);

#ifdef __cplusplus
}
#endif

#endif /* CLK_MENU_THEME_H */
