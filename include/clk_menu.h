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
    clk_menu_item** items;
    size_t count;
    size_t capacity;
} clk_item_list;

typedef struct clk_menu_tab {
    int id;
    char* name;
    clk_item_list* item_list;
    int active_item;
} clk_menu_tab;

typedef struct {
    clk_menu_tab** tabs;
    size_t count;
    size_t capacity;
} clk_tab_list;

typedef struct {
    clk_tab_list* tab_list;
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
 *  clk_menu — Lifecycle
 * ------------------------------------------------------------------ */

clk_menu* clk_menu_create(void);
void clk_menu_destroy(clk_menu* menu);

/* ------------------------------------------------------------------
 *  clk_menu — Tabs
 * ------------------------------------------------------------------ */

int clk_menu_add_tab(clk_menu* menu, int tab_id, const char* name);

/* ------------------------------------------------------------------
 *  clk_menu — Items (wrappers around clk_item_list_*)
 * ------------------------------------------------------------------ */

void clk_menu_add_item_str(clk_menu* menu, int tab_id, int item_id, const char* label,
                           int default_idx, const char** options, int option_count);

void clk_menu_add_item_str_at(clk_menu* menu, int tab_id, int item_id, const char* label,
                              int default_idx, const char** options, int option_count,
                              int position);

void clk_menu_add_item_int(clk_menu* menu, int tab_id, int item_id, const char* label,
                           double default_val, double min_val, double max_val, double step_val);

void clk_menu_add_item_int_at(clk_menu* menu, int tab_id, int item_id, const char* label,
                              double default_val, double min_val, double max_val, double step_val,
                              int position);

void clk_menu_add_item_bool(clk_menu* menu, int tab_id, int item_id, const char* label,
                            bool default_val);

void clk_menu_add_item_bool_at(clk_menu* menu, int tab_id, int item_id, const char* label,
                               bool default_val, int position);

void clk_menu_add_item_action(clk_menu* menu, int tab_id, int item_id, const char* label);

void clk_menu_add_item_action_at(clk_menu* menu, int tab_id, int item_id, const char* label,
                                 int position);

void clk_menu_remove_item(clk_menu* menu, int tab_id, int item_id);

/* ------------------------------------------------------------------
 *  clk_menu — Interaction
 * ------------------------------------------------------------------ */

clk_menu_event clk_menu_handle_input(clk_menu* menu, clk_menu_input input);

/* ------------------------------------------------------------------
 *  clk_menu — External sync
 * ------------------------------------------------------------------ */

bool clk_menu_set_value_str(clk_menu* menu, int tab_id, int item_id, const char* val);
bool clk_menu_set_value_int(clk_menu* menu, int tab_id, int item_id, double val);
bool clk_menu_set_value_bool(clk_menu* menu, int tab_id, int item_id, bool val);

/* ------------------------------------------------------------------
 *  clk_menu — Dynamic options
 * ------------------------------------------------------------------ */

void clk_menu_add_option(clk_menu* menu, int tab_id, int item_id, const char* opt);
void clk_menu_remove_option(clk_menu* menu, int tab_id, int item_id, int idx);
void clk_menu_clear_options(clk_menu* menu, int tab_id, int item_id);

/* ------------------------------------------------------------------
 *  clk_menu — INT range
 * ------------------------------------------------------------------ */

void clk_menu_set_item_range(clk_menu* menu, int tab_id, int item_id, double min_val,
                             double max_val, double step_val);

/* ------------------------------------------------------------------
 *  clk_menu — Path-list helpers
 * ------------------------------------------------------------------ */

char** clk_menu_build_names(char** paths, int count);
const char** clk_menu_wrap_strings(char** strings, int count);
int clk_menu_find_index(const char* needle, const char** haystack, int count, int fallback);
void clk_menu_rebuild_item(clk_menu* menu, int tab_id, int item_id, const char** options, int count,
                           int new_index);

/* ================================================================
 *  clk_item_list — item list container
 * ================================================================ */

clk_item_list* clk_item_list_create(void);
void clk_item_list_destroy(clk_item_list* list);

clk_menu_item* clk_item_list_find(const clk_item_list* list, int item_id);
clk_menu_item* clk_item_list_get_at(const clk_item_list* list, size_t position);
size_t clk_item_list_count(const clk_item_list* list);

void clk_item_list_add_str(clk_item_list* list, int tab_id, int item_id, const char* label,
                           int default_idx, const char** options, int option_count);

void clk_item_list_add_str_at(clk_item_list* list, int tab_id, int item_id, const char* label,
                              int default_idx, const char** options, int option_count,
                              int position);

void clk_item_list_add_int(clk_item_list* list, int tab_id, int item_id, const char* label,
                           double default_val, double min_val, double max_val, double step_val);

void clk_item_list_add_int_at(clk_item_list* list, int tab_id, int item_id, const char* label,
                              double default_val, double min_val, double max_val, double step_val,
                              int position);

void clk_item_list_add_bool(clk_item_list* list, int tab_id, int item_id, const char* label,
                            bool default_val);

void clk_item_list_add_bool_at(clk_item_list* list, int tab_id, int item_id, const char* label,
                               bool default_val, int position);

void clk_item_list_add_action(clk_item_list* list, int tab_id, int item_id, const char* label);

void clk_item_list_add_action_at(clk_item_list* list, int tab_id, int item_id, const char* label,
                                 int position);

void clk_item_list_remove(clk_item_list* list, int item_id);
void clk_item_list_remove_at(clk_item_list* list, size_t position);
void clk_item_list_clear(clk_item_list* list);

bool clk_item_list_set_value_str(clk_item_list* list, int item_id, const char* val);
bool clk_item_list_set_value_int(clk_item_list* list, int item_id, double val);
bool clk_item_list_set_value_bool(clk_item_list* list, int item_id, bool val);

void clk_item_list_add_option(clk_item_list* list, int item_id, const char* opt);
void clk_item_list_remove_option(clk_item_list* list, int item_id, int idx);
void clk_item_list_clear_options(clk_item_list* list, int item_id);

void clk_item_list_set_range(clk_item_list* list, int item_id, double min_val, double max_val,
                             double step_val);

void clk_item_list_rebuild_item(clk_item_list* list, int item_id, const char** options, int count,
                                int new_index);

/* ================================================================
 *  clk_tab_list — tab list container
 * ================================================================ */

clk_tab_list* clk_tab_list_create(void);
void clk_tab_list_destroy(clk_tab_list* list);

clk_menu_tab* clk_tab_list_find(const clk_tab_list* list, int tab_id);
clk_menu_tab* clk_tab_list_get_at(const clk_tab_list* list, size_t position);
size_t clk_tab_list_count(const clk_tab_list* list);

int clk_tab_list_add(clk_tab_list* list, int tab_id, const char* name);
void clk_tab_list_remove(clk_tab_list* list, int tab_id);

void clk_tab_list_set_item_list(clk_tab_list* list, int tab_id, clk_item_list* new_item_list);
const clk_item_list* clk_tab_list_get_item_list(const clk_tab_list* list, int tab_id);

#ifdef __cplusplus
}
#endif

#endif /* CLK_MENU_H */
