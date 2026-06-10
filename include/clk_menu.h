#ifndef CLK_MENU_H
#define CLK_MENU_H

#include <stdbool.h>
#include <stddef.h>

#include "clk_term.h"

#ifdef __cplusplus
extern "C" {
#endif

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
        double n;
        bool b;
    } value;
    double min_val, max_val, step_val;
    char** str_options;
    int str_options_count;
} clk_menu_item;

typedef struct {
    int id;
    char* name;
    clk_menu_item** items;
    size_t item_count;
    size_t item_capacity;
} clk_menu_tab;

typedef struct {
    clk_menu_tab** tabs;
    size_t tab_count;
    size_t tab_capacity;
    int active_tab_idx;
    int active_item_idx;
    int posx, posy;
    size_t width, height;
    bool visible;
} clk_menu;

typedef enum {
    CLK_MENU_EVENT_NONE,
    CLK_MENU_EVENT_VALUE_CHANGE,
    CLK_MENU_EVENT_SUBMIT,
} clk_menu_event_type;

typedef struct {
    clk_menu_event_type event;
    int tab_id;
    int item_id;
    union {
        const char* s;
        double n;
        bool b;
    } value;
} clk_menu_event;

clk_menu* clk_menu_create(void);

void clk_menu_destory(clk_menu* menu);

int clk_menu_add_tab(clk_menu* menu, const char* tab_name);

void clk_menu_add_item_str(clk_menu* m, int tab_id, int item_id, const char* label,
                           int default_val_idx, const char** options, int option_count);

void clk_menu_add_item_int(clk_menu* m, int tab_id, int item_id, const char* label,
                           double default_val, double min_val, double max_val, double step_val);

void clk_menu_add_item_bool(clk_menu* m, int tab_id, int item_id, const char* label,
                            bool default_val);

void clk_menu_add_item_action(clk_menu* m, int tab_id, int item_id, const char* label);

void clk_menu_remove_item(clk_menu* m, int tab_id, int item_id);

#ifdef __cplusplus
}
#endif

#endif  // CLK_MENU_H
