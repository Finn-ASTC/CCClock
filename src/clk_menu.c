#include "clk_menu.h"

#include <assert.h>
#include <complex.h>
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

static bool clk_menu_tab_ensure_items_capacity(clk_menu_tab* tab) {
    if (tab->item_count < tab->item_capacity)
        return true;
    size_t new_cap = tab->item_capacity * 2;
    clk_menu_item** tmp = realloc(tab->items, new_cap * sizeof(clk_menu_item*));
    if (!tmp)
        return false;
    memset(tmp + tab->item_capacity, 0, (new_cap - tab->item_capacity) * sizeof(clk_menu_item*));
    tab->items = tmp;
    tab->item_capacity = new_cap;
    return true;
}

void clk_menu_add_item_str(clk_menu* m, int tab_id, int item_id, const char* label, int default_idx,
                           const char** options, int option_count) {
    if (!m || !label || !options || option_count <= 0)
        return;

    clk_menu_tab* tab = find_tab(m, tab_id);
    if (!tab)
        return;

    if (!clk_menu_tab_ensure_items_capacity(tab))
        return;

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
    if (!m || !label)
        return;

    clk_menu_tab* tab = find_tab(m, tab_id);
    if (!tab)
        return;

    if (!clk_menu_tab_ensure_items_capacity(tab))
        return;

    clk_menu_item* item = malloc(sizeof(clk_menu_item));
    if (!item)
        return;
    memset(item, 0, sizeof(clk_menu_item));

    item->id = item_id;
    item->tab_id = tab_id;
    item->type = CLK_MENU_TYPE_INT;
    item->label = strdup(label);
    if (!item->label) {
        free(item);
        return;
    }

    item->min_val = min_val;
    item->max_val = max_val;
    item->step_val = step_val;

    if (default_val < min_val)
        default_val = min_val;
    if (default_val > max_val)
        default_val = max_val;
    item->value.d = default_val;

    tab->items[tab->item_count++] = item;
}

void clk_menu_add_item_bool(clk_menu* m, int tab_id, int item_id, const char* label,
                            bool default_val) {
    if (!m || !label)
        return;

    clk_menu_tab* tab = find_tab(m, tab_id);
    if (!tab)
        return;

    if (!clk_menu_tab_ensure_items_capacity(tab))
        return;

    clk_menu_item* item = malloc(sizeof(clk_menu_item));
    if (!item)
        return;
    memset(item, 0, sizeof(clk_menu_item));

    item->id = item_id;
    item->tab_id = tab_id;
    item->type = CLK_MENU_TYPE_BOOL;
    item->label = strdup(label);
    if (!item->label) {
        free(item);
        return;
    }

    item->value.b = default_val;

    tab->items[tab->item_count++] = item;
}

void clk_menu_add_item_action(clk_menu* m, int tab_id, int item_id, const char* label) {
    if (!m || !label)
        return;

    clk_menu_tab* tab = find_tab(m, tab_id);
    if (!tab)
        return;

    if (!clk_menu_tab_ensure_items_capacity(tab))
        return;

    clk_menu_item* item = malloc(sizeof(clk_menu_item));
    if (!item)
        return;
    memset(item, 0, sizeof(clk_menu_item));

    item->id = item_id;
    item->tab_id = tab_id;
    item->type = CLK_MENU_TYPE_ACTION;
    item->label = strdup(label);
    if (!item->label) {
        free(item);
        return;
    }

    tab->items[tab->item_count++] = item;
}

void clk_menu_remove_item(clk_menu* m, int tab_id, int item_id) {
    clk_menu_tab* tab = find_tab(m, tab_id);
    if (!tab)
        return;

    for (size_t i = 0; i < tab->item_count; ++i) {
        if (tab->items[i]->id == item_id) {
            clk_menu_item_destroy(tab->items[i]);
            for (size_t j = i; j + 1 < tab->item_count; ++j)
                tab->items[j] = tab->items[j + 1];
            tab->items[--tab->item_count] = NULL;
            return;
        }
    }
}

/* ------------------------------------------------------------------
 *  Interaction
 * ------------------------------------------------------------------ */

clk_menu_event clk_menu_handle_input(clk_menu* m, clk_menu_input input) {
    clk_menu_event ev = {.type = CLK_MENU_EVENT_NONE};

    if (!m || !m->visible || m->tab_count == 0)
        return ev;

    clk_menu_tab* tab = m->tabs[m->active_tab];
    if (!tab)
        return ev;

    ev.tab_id = tab->id;

    if (tab->item_count > 0 && tab->active_item < (int)tab->item_count) {
        clk_menu_item* item = tab->items[tab->active_item];
        if (item)
            ev.item_id = item->id;
    }

    switch (input) {
        case CLK_MENU_INPUT_NONE:
            break;

        case CLK_MENU_INPUT_PREV_ITEM:
            if (tab->item_count == 0)
                break;
            if (tab->active_item > 0)
                tab->active_item--;
            break;

        case CLK_MENU_INPUT_NEXT_ITEM:
            if (tab->item_count == 0)
                break;
            if (tab->active_item < (int)tab->item_count - 1)
                tab->active_item++;
            break;

        case CLK_MENU_INPUT_DEC_VALUE: {
            if (tab->item_count == 0)
                break;
            clk_menu_item* item = tab->items[tab->active_item];
            if (!item)
                break;

            switch (item->type) {
                case CLK_MENU_TYPE_STR:
                    if (item->option_count == 0)
                        break;
                    if (item->option_idx > 0)
                        item->option_idx--;
                    else
                        item->option_idx = item->option_count - 1;
                    item->value.s = item->options[item->option_idx];
                    ev.type = CLK_MENU_EVENT_VALUE_CHANGED;
                    ev.value.s = item->value.s;
                    break;
                case CLK_MENU_TYPE_INT: {
                    double new_val = item->value.d - item->step_val;
                    if (new_val < item->min_val)
                        new_val = item->min_val;
                    if (new_val != item->value.d) {
                        item->value.d = new_val;
                        ev.type = CLK_MENU_EVENT_VALUE_CHANGED;
                        ev.value.d = item->value.d;
                    }
                    break;
                }
                case CLK_MENU_TYPE_BOOL:
                    item->value.b = !item->value.b;
                    ev.type = CLK_MENU_EVENT_VALUE_CHANGED;
                    ev.value.b = item->value.b;
                    break;
                case CLK_MENU_TYPE_ACTION:
                    break;
            }
            break;
        }

        case CLK_MENU_INPUT_INC_VALUE: {
            if (tab->item_count == 0)
                break;
            clk_menu_item* item = tab->items[tab->active_item];
            if (!item)
                break;

            switch (item->type) {
                case CLK_MENU_TYPE_STR:
                    if (item->option_count == 0)
                        break;
                    if (item->option_idx + 1 < item->option_count)
                        item->option_idx++;
                    else
                        item->option_idx = 0;
                    item->value.s = item->options[item->option_idx];
                    ev.type = CLK_MENU_EVENT_VALUE_CHANGED;
                    ev.value.s = item->value.s;
                    break;
                case CLK_MENU_TYPE_INT: {
                    double new_val = item->value.d + item->step_val;
                    if (new_val > item->max_val)
                        new_val = item->max_val;
                    if (new_val != item->value.d) {
                        item->value.d = new_val;
                        ev.type = CLK_MENU_EVENT_VALUE_CHANGED;
                        ev.value.d = item->value.d;
                    }
                    break;
                }
                case CLK_MENU_TYPE_BOOL:
                    item->value.b = !item->value.b;
                    ev.type = CLK_MENU_EVENT_VALUE_CHANGED;
                    ev.value.b = item->value.b;
                    break;
                case CLK_MENU_TYPE_ACTION:
                    break;
            }
            break;
        }

        case CLK_MENU_INPUT_NEXT_TAB:
            if (m->active_tab + 1 < (int)m->tab_count)
                m->active_tab++;
            else
                m->active_tab = 0;
            m->scroll_offset = 0;
            break;

        case CLK_MENU_INPUT_CONFIRM: {
            if (tab->item_count == 0)
                break;
            clk_menu_item* item = tab->items[tab->active_item];
            if (item && item->type == CLK_MENU_TYPE_ACTION)
                ev.type = CLK_MENU_EVENT_SUBMIT;
            break;
        }
    }

    /* refresh tab_id / item_id in case active changed */
    if (m->tab_count > 0) {
        tab = m->tabs[m->active_tab];
        if (tab) {
            ev.tab_id = tab->id;
            if (tab->item_count > 0 && tab->active_item < (int)tab->item_count) {
                clk_menu_item* item = tab->items[tab->active_item];
                if (item)
                    ev.item_id = item->id;
            }
        }
    }

    return ev;
}

/* ------------------------------------------------------------------
 *  Layout
 * ------------------------------------------------------------------ */

void clk_menu_set_position(clk_menu* m, int x, int y) {
    if (!m)
        return;
    m->posx = x;
    m->posy = y;
}

void clk_menu_set_size(clk_menu* m, int w, int h) {
    if (!m || w <= 0 || h <= 0)
        return;
    m->width = w;
    m->height = h;
}

void clk_menu_set_visible(clk_menu* m, bool v) {
    if (!m)
        return;
    m->visible = v;
}

/* ------------------------------------------------------------------
 *  External sync
 * ------------------------------------------------------------------ */

bool clk_menu_set_value_str(clk_menu* m, int tab_id, int item_id, const char* val) {
    if (!m || !val)
        return false;
    clk_menu_item* item = find_item(m, tab_id, item_id);
    if (!item || item->type != CLK_MENU_TYPE_STR)
        return false;

    for (size_t i = 0; i < item->option_count; ++i) {
        if (strcmp(val, item->options[i]) == 0) {
            item->option_idx = i;
            item->value.s = item->options[i];
            return true;
        }
    }
    return false;
}

bool clk_menu_set_value_int(clk_menu* m, int tab_id, int item_id, double val) {
    if (!m)
        return false;
    clk_menu_item* item = find_item(m, tab_id, item_id);
    if (!item || item->type != CLK_MENU_TYPE_INT)
        return false;

    if (val < item->min_val)
        item->value.d = item->min_val;
    else if (val > item->max_val)
        item->value.d = item->max_val;
    else
        item->value.d = val;
    return true;
}

bool clk_menu_set_value_bool(clk_menu* m, int tab_id, int item_id, bool val) {
    if (!m)
        return false;
    clk_menu_item* item = find_item(m, tab_id, item_id);
    if (!item || item->type != CLK_MENU_TYPE_BOOL)
        return false;

    item->value.b = val;
    return true;
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
