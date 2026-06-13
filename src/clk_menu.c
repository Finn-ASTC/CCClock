#include "clk_menu.h"

#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#define CLK_TAB_DEFAULT_CAPACITY 6
#define CLK_ITEM_DEFAULT_CAPACITY 6

/* ------------------------------------------------------------------
 *  Internal helper
 * ------------------------------------------------------------------ */

/** Find a tab by id. Returns NULL if not found. */
static clk_menu_tab* find_tab(clk_menu* m, int tab_id) {
    for (size_t i = 0; i < m->tab_count; ++i)
        if (m->tabs[i]->id == tab_id)
            return m->tabs[i];
    return NULL;
}

/** Find an item by tab_id + item_id. Returns NULL if not found. */
static clk_menu_item* find_item(clk_menu* m, int tab_id, int item_id) {
    clk_menu_tab* tab = find_tab(m, tab_id);
    if (!tab)
        return NULL;
    for (size_t i = 0; i < tab->item_count; ++i)
        if (tab->items[i]->id == item_id)
            return tab->items[i];
    return NULL;
}

/* ------------------------------------------------------------------
 *  Lifecycle
 * ------------------------------------------------------------------ */

clk_menu* clk_menu_create(void) {
    clk_menu* menu = malloc(sizeof(clk_menu));
    if (!menu)
        return NULL;
    memset(menu, 0, sizeof(clk_menu));
    clk_menu_tab** tabs = malloc(CLK_TAB_DEFAULT_CAPACITY * sizeof(clk_menu_tab*));
    if (!tabs) {
        clk_menu_destroy(menu);
        return NULL;
    }
    memset(tabs, 0, CLK_TAB_DEFAULT_CAPACITY * sizeof(clk_menu_tab*));
    menu->tabs = tabs;
    menu->tab_capacity = CLK_TAB_DEFAULT_CAPACITY;
    return menu;
}

static void clk_menu_item_destroy(clk_menu_item* item);

static void clk_menu_tab_destroy(clk_menu_tab* tab);

void clk_menu_destroy(clk_menu* menu) {
    if (!menu)
        return;
    for (size_t i = 0; i < menu->tab_count; ++i)
        clk_menu_tab_destroy(menu->tabs[i]);
    free(menu->tabs);
    free(menu);
}

static void clk_menu_tab_destroy(clk_menu_tab* tab) {
    if (!tab)
        return;
    for (size_t i = 0; i < tab->item_count; ++i)
        clk_menu_item_destroy(tab->items[i]);
    free(tab->items);
    free(tab->name);
    free(tab);
}

static void clk_menu_item_destroy(clk_menu_item* item) {
    if (!item)
        return;

    free(item->label);
    item->label = NULL;

    for (int i = 0; i < item->option_count; ++i) {
        free(item->options[i]);
    }

    free(item->options);
    item->options = NULL;

    free(item);
}

/* ------------------------------------------------------------------
 *  Tabs
 * ------------------------------------------------------------------ */

int clk_menu_add_tab(clk_menu* m, int tab_id, const char* name) {
    if (!m || !name)
        return -1;

    if (m->tab_count >= m->tab_capacity) {
        size_t new_cap = m->tab_capacity * 2;
        clk_menu_tab** tmp = realloc(m->tabs, new_cap * sizeof(clk_menu_tab*));
        if (!tmp)
            return -1;
        memset(tmp + m->tab_capacity, 0, (new_cap - m->tab_capacity) * sizeof(clk_menu_tab*));
        m->tabs = tmp;
        m->tab_capacity = new_cap;
    }

    clk_menu_tab* tab = malloc(sizeof(clk_menu_tab));
    if (!tab)
        return -1;
    memset(tab, 0, sizeof(clk_menu_tab));

    tab->name = strdup(name);
    if (!tab->name) {
        clk_menu_tab_destroy(tab);
        return -1;
    }

    clk_menu_item** items = malloc(CLK_ITEM_DEFAULT_CAPACITY * sizeof(clk_menu_item*));
    if (!items) {
        clk_menu_tab_destroy(tab);
        return -1;
    }
    memset(items, 0, CLK_ITEM_DEFAULT_CAPACITY * sizeof(clk_menu_item*));
    tab->items = items;

    tab->item_capacity = CLK_ITEM_DEFAULT_CAPACITY;

    tab->id = tab_id;

    m->tabs[m->tab_count++] = tab;

    return tab->id;
}

/* ------------------------------------------------------------------
 *  Items
 * ------------------------------------------------------------------ */

void clk_menu_add_item_str(clk_menu* m, int tab_id, int item_id, const char* label, int default_idx,
                           const char** options, int option_count) {
    if (!m || !label || !options || option_count <= 0)
        return;

    clk_menu_tab* tab = find_tab(m, tab_id);
    if (!tab)
        return;

    if (tab->item_count >= tab->item_capacity) {
        size_t new_cap = tab->item_capacity * 2;
        clk_menu_item** tmp = realloc(tab->items, new_cap * sizeof(clk_menu_item*));
        if (!tmp)
            return;
        memset(tmp + tab->item_capacity, 0,
               (new_cap - tab->item_capacity) * sizeof(clk_menu_item*));
        tab->items = tmp;
        tab->item_capacity = new_cap;
    }

    clk_menu_item* item = malloc(sizeof(clk_menu_item));
    if (!item)
        return;
    memset(item, 0, sizeof(clk_menu_item));

    item->id = item_id;
    item->tab_id = tab_id;
    item->type = CLK_MENU_TYPE_STR;
    item->label = strdup(label);
    if (!item->label) {
        free(item);
        return;
    }

    item->options = malloc(option_count * sizeof(char*));
    if (!item->options) {
        free(item->label);
        free(item);
        return;
    }
    for (int i = 0; i < option_count; ++i) {
        item->options[i] = strdup(options[i]);
        if (!item->options[i]) {
            /* partial free on failure */
            for (int j = 0; j < i; ++j)
                free(item->options[j]);
            free(item->options);
            free(item->label);
            free(item);
            return;
        }
    }
    item->option_count = option_count;

    if (default_idx < 0)
        default_idx = 0;
    if (default_idx >= option_count)
        default_idx = option_count - 1;
    item->option_idx = default_idx;
    item->value.s = item->options[default_idx];

    tab->items[tab->item_count++] = item;
}

void clk_menu_add_item_int(clk_menu* m, int tab_id, int item_id, const char* label,
                           double default_val, double min_val, double max_val, double step_val) {
    /* TODO: find tab, allocate item, set INT fields, clamp value to [min,max], add to tab->items[]
     */
}

void clk_menu_add_item_bool(clk_menu* m, int tab_id, int item_id, const char* label,
                            bool default_val) {
    /* TODO: find tab, allocate item, set BOOL fields, add to tab->items[] */
}

void clk_menu_add_item_action(clk_menu* m, int tab_id, int item_id, const char* label) {
    /* TODO: find tab, allocate item, set ACTION fields, add to tab->items[] */
}

void clk_menu_remove_item(clk_menu* m, int tab_id, int item_id) {
    /* TODO: find tab, find item index, free item + label + options, shift array, NULL out */
}

/* ------------------------------------------------------------------
 *  Interaction
 * ------------------------------------------------------------------ */

clk_menu_event clk_menu_handle_input(clk_menu* m, clk_key_event input) {
    /* TODO:
     *   visible=false → return NONE
     *   switch(input.key):
     *     UP/DOWN   → move active_item (update scroll_offset)
     *     LEFT/RIGHT→ modify value (INT: ±step, BOOL: flip, STR: cycle option_idx)
     *     TAB       → cycle active_tab
     *     ENTER     → if ACTION type → return SUBMIT; else NONE
     *   emit VALUE_CHANGED event when value actually changes
     */
    clk_menu_event ev = {.type = CLK_MENU_EVENT_NONE};
    return ev;
}

/* ------------------------------------------------------------------
 *  Layout
 * ------------------------------------------------------------------ */

void clk_menu_set_position(clk_menu* m, int x, int y) {
    /* TODO: set m->posx, m->posy */
}

void clk_menu_set_size(clk_menu* m, int w, int h) {
    /* TODO: set m->width, m->height */
}

void clk_menu_set_visible(clk_menu* m, bool v) {
    /* TODO: set m->visible */
}

/* ------------------------------------------------------------------
 *  External sync
 * ------------------------------------------------------------------ */

void clk_menu_set_value_str(clk_menu* m, int tab_id, int item_id, const char* val) {
    /* TODO: find item, find val in options[], set option_idx, update value.s */
}

void clk_menu_set_value_int(clk_menu* m, int tab_id, int item_id, double val) {
    /* TODO: find item, clamp to [min,max], set value.d */
}

void clk_menu_set_value_bool(clk_menu* m, int tab_id, int item_id, bool val) {
    /* TODO: find item, set value.b */
}

/* ------------------------------------------------------------------
 *  Dynamic options
 * ------------------------------------------------------------------ */

void clk_menu_add_option(clk_menu* m, int tab_id, int item_id, const char* opt) {
    /* TODO: find item, realloc options[], strdup opt, increment option_count */
}

void clk_menu_remove_option(clk_menu* m, int tab_id, int item_id, int idx) {
    /* TODO: find item, free options[idx], shift array, decrement option_count, clamp option_idx */
}

void clk_menu_clear_options(clk_menu* m, int tab_id, int item_id) {
    /* TODO: find item, free all options[], set option_count=0, option_idx=0 */
}

/* ------------------------------------------------------------------
 *  INT range
 * ------------------------------------------------------------------ */

void clk_menu_set_item_range(clk_menu* m, int tab_id, int item_id, double min_val, double max_val,
                             double step_val) {
    /* TODO: find item, set min/max/step, clamp current value.d into new range */
}
