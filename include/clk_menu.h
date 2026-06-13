#ifndef CLK_MENU_H
#define CLK_MENU_H

#include <stdbool.h>
#include <stddef.h>

#include "clk_key_io.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------
 *  Types
 * ------------------------------------------------------------------ */

typedef enum {
    CLK_MENU_TYPE_STR,
    CLK_MENU_TYPE_INT,
    CLK_MENU_TYPE_BOOL,
    CLK_MENU_TYPE_ACTION,
} clk_menu_value_type;

typedef struct {
    int id;
    int tab_id;
    clk_menu_value_type type;
    char* label;
    union {
        const char* s;
        double d;
        bool b;
    } value;
    double min_val, max_val, step_val;
    char** options;
    int option_count;
    int option_idx;
} clk_menu_item;

typedef struct {
    int id;
    char* name;
    clk_menu_item** items;
    size_t item_count;
    size_t item_capacity;
    int active_item;
} clk_menu_tab;

typedef struct {
    clk_menu_tab** tabs;
    size_t tab_count;
    size_t tab_capacity;
    int active_tab;
    int scroll_offset;
    int posx, posy;
    int width, height;
    bool visible;
} clk_menu;

/* ------------------------------------------------------------------
 *  Events
 * ------------------------------------------------------------------ */

typedef enum {
    CLK_MENU_EVENT_NONE,
    CLK_MENU_EVENT_VALUE_CHANGED,
    CLK_MENU_EVENT_SUBMIT,
} clk_menu_event_type;

typedef struct {
    clk_menu_event_type type;
    int tab_id;
    int item_id;
    union {
        const char* s;
        double d;
        bool b;
    } value;
} clk_menu_event;

/* ------------------------------------------------------------------
 *  Lifecycle
 * ------------------------------------------------------------------ */

clk_menu* clk_menu_create(void);
void clk_menu_destroy(clk_menu* menu);

/* ------------------------------------------------------------------
 *  Tabs
 * ------------------------------------------------------------------ */

/** Add a tab with the given @p tab_id (caller-defined, e.g. enum value).
 *  Returns the tab_id on success, or -1 on failure. */
int clk_menu_add_tab(clk_menu* m, int tab_id, const char* name);

/* ------------------------------------------------------------------
 *  Items
 * ------------------------------------------------------------------ */

void clk_menu_add_item_str(clk_menu* m, int tab_id, int item_id, const char* label, int default_idx,
                           const char** options, int option_count);

void clk_menu_add_item_int(clk_menu* m, int tab_id, int item_id, const char* label,
                           double default_val, double min_val, double max_val, double step_val);

void clk_menu_add_item_bool(clk_menu* m, int tab_id, int item_id, const char* label,
                            bool default_val);

void clk_menu_add_item_action(clk_menu* m, int tab_id, int item_id, const char* label);

void clk_menu_remove_item(clk_menu* m, int tab_id, int item_id);

/* ------------------------------------------------------------------
 *  Interaction
 * ------------------------------------------------------------------ */

clk_menu_event clk_menu_handle_input(clk_menu* m, clk_key_event input);

/* ------------------------------------------------------------------
 *  Layout
 * ------------------------------------------------------------------ */

void clk_menu_set_position(clk_menu* m, int x, int y);
void clk_menu_set_size(clk_menu* m, int w, int h);
void clk_menu_set_visible(clk_menu* m, bool v);

/* ------------------------------------------------------------------
 *  External sync
 * ------------------------------------------------------------------ */

void clk_menu_set_value_str(clk_menu* m, int tab_id, int item_id, const char* val);
void clk_menu_set_value_int(clk_menu* m, int tab_id, int item_id, double val);
void clk_menu_set_value_bool(clk_menu* m, int tab_id, int item_id, bool val);

/* ------------------------------------------------------------------
 *  Dynamic options
 * ------------------------------------------------------------------ */

void clk_menu_add_option(clk_menu* m, int tab_id, int item_id, const char* opt);
void clk_menu_remove_option(clk_menu* m, int tab_id, int item_id, int idx);
void clk_menu_clear_options(clk_menu* m, int tab_id, int item_id);

/* ------------------------------------------------------------------
 *  INT range
 * ------------------------------------------------------------------ */

void clk_menu_set_item_range(clk_menu* m, int tab_id, int item_id, double min_val, double max_val,
                             double step_val);

#ifdef __cplusplus
}
#endif

#endif /* CLK_MENU_H */
