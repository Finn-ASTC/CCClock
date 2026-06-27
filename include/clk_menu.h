#ifndef CLK_MENU_H
#define CLK_MENU_H

#include <stdbool.h>
#include <stddef.h>

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
        const char* str;
        double num;
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
} clk_menu;

/* ------------------------------------------------------------------
 *  Input
 * ------------------------------------------------------------------ */

typedef enum {
    CLK_MENU_INPUT_NONE,
    CLK_MENU_INPUT_PREV_ITEM,
    CLK_MENU_INPUT_NEXT_ITEM,
    CLK_MENU_INPUT_DEC_VALUE,
    CLK_MENU_INPUT_INC_VALUE,
    CLK_MENU_INPUT_NEXT_TAB,
    CLK_MENU_INPUT_CONFIRM,
} clk_menu_input;

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
        const char* str;
        double num;
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
int clk_menu_add_tab(clk_menu* menu, int tab_id, const char* name);

/* ------------------------------------------------------------------
 *  Items
 * ------------------------------------------------------------------ */

/** Append a string item. */
void clk_menu_add_item_str(clk_menu* menu, int tab_id, int item_id, const char* label,
                           int default_idx, const char** options, int option_count);

/** Insert a string item at @p position (-1 = append). */
void clk_menu_add_item_str_at(clk_menu* menu, int tab_id, int item_id, const char* label,
                               int default_idx, const char** options, int option_count,
                               int position);

/** Append an int item. */
void clk_menu_add_item_int(clk_menu* menu, int tab_id, int item_id, const char* label,
                           double default_val, double min_val, double max_val, double step_val);

/** Insert an int item at @p position (-1 = append). */
void clk_menu_add_item_int_at(clk_menu* menu, int tab_id, int item_id, const char* label,
                               double default_val, double min_val, double max_val, double step_val,
                               int position);

/** Append a bool item. */
void clk_menu_add_item_bool(clk_menu* menu, int tab_id, int item_id, const char* label,
                            bool default_val);

/** Insert a bool item at @p position (-1 = append). */
void clk_menu_add_item_bool_at(clk_menu* menu, int tab_id, int item_id, const char* label,
                                bool default_val, int position);

/** Append an action item. */
void clk_menu_add_item_action(clk_menu* menu, int tab_id, int item_id, const char* label);

/** Insert an action item at @p position (-1 = append). */
void clk_menu_add_item_action_at(clk_menu* menu, int tab_id, int item_id, const char* label,
                                  int position);

void clk_menu_remove_item(clk_menu* menu, int tab_id, int item_id);

/* ------------------------------------------------------------------
 *  Interaction
 * ------------------------------------------------------------------ */

clk_menu_event clk_menu_handle_input(clk_menu* menu, clk_menu_input input);

/* ------------------------------------------------------------------
 *  External sync
 * ------------------------------------------------------------------ */

bool clk_menu_set_value_str(clk_menu* menu, int tab_id, int item_id, const char* val);
bool clk_menu_set_value_int(clk_menu* menu, int tab_id, int item_id, double val);
bool clk_menu_set_value_bool(clk_menu* menu, int tab_id, int item_id, bool val);

/* ------------------------------------------------------------------
 *  Dynamic options
 * ------------------------------------------------------------------ */

void clk_menu_add_option(clk_menu* menu, int tab_id, int item_id, const char* opt);
void clk_menu_remove_option(clk_menu* menu, int tab_id, int item_id, int idx);
void clk_menu_clear_options(clk_menu* menu, int tab_id, int item_id);

/* ------------------------------------------------------------------
 *  INT range
 * ------------------------------------------------------------------ */

void clk_menu_set_item_range(clk_menu* menu, int tab_id, int item_id, double min_val,
                             double max_val, double step_val);

/* ------------------------------------------------------------------
 *  Path-list helpers
 * ------------------------------------------------------------------ */

/** Build display names from file paths (strip directory + extension).
 *  Each returned string is malloc'd; caller frees via clk_path_list_free. */
char** clk_menu_build_names(char** paths, int count);

/** Wrap a char** as a const char** view (strings are not copied). */
const char** clk_menu_wrap_strings(char** strings, int count);

/** Find the index of a string in an array.  Returns @p fallback when not found. */
int clk_menu_find_index(const char* needle, const char** haystack, int count, int fallback);

/** Rebuild the options of a string-type item: clear → add → set current value. */
void clk_menu_rebuild_item(clk_menu* menu, int tab_id, int item_id, const char** options,
                           int count, int new_index);

#ifdef __cplusplus
}
#endif

#endif /* CLK_MENU_H */
