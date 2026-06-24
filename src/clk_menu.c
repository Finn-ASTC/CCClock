#include "clk_menu.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#define CLK_TAB_DEFAULT_CAPACITY 6
#define CLK_ITEM_DEFAULT_CAPACITY 6

/* ------------------------------------------------------------------
 *  Internal helper
 * ------------------------------------------------------------------ */

/** Find a tab by id. Returns NULL if not found. */
static clk_menu_tab* find_tab(clk_menu* menu, int tab_id) {
    for (size_t i = 0; i < menu->tab_count; ++i)
        if (menu->tabs[i]->id == tab_id)
            return menu->tabs[i];
    return NULL;
}

/** Find an item by tab_id + item_id. Returns NULL if not found. */
static clk_menu_item* find_item(clk_menu* menu, int tab_id, int item_id) {
    clk_menu_tab* tab = find_tab(menu, tab_id);
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

/** Free a tab and all of its items. */
static void clk_menu_tab_destroy(clk_menu_tab* tab) {
    if (!tab)
        return;
    for (size_t i = 0; i < tab->item_count; ++i)
        clk_menu_item_destroy(tab->items[i]);
    free(tab->items);
    free(tab->name);
    free(tab);
}

/** Free an item and all of its allocated fields. */
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

int clk_menu_add_tab(clk_menu* menu, int tab_id, const char* name) {
    if (!menu || !name)
        return -1;

    if (menu->tab_count >= menu->tab_capacity) {
        size_t new_cap = menu->tab_capacity * 2;
        clk_menu_tab** tmp = realloc(menu->tabs, new_cap * sizeof(clk_menu_tab*));
        if (!tmp)
            return -1;
        memset(tmp + menu->tab_capacity, 0, (new_cap - menu->tab_capacity) * sizeof(clk_menu_tab*));
        menu->tabs = tmp;
        menu->tab_capacity = new_cap;
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

    menu->tabs[menu->tab_count++] = tab;

    return tab->id;
}

/* ------------------------------------------------------------------
 *  Items
 * ------------------------------------------------------------------ */

/** Grow a tab's item array if it is full. Returns false on allocation failure. */
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

void clk_menu_add_item_str(clk_menu* menu, int tab_id, int item_id, const char* label,
                           int default_idx, const char** options, int option_count) {
    if (!menu || !label || !options || option_count <= 0)
        return;

    clk_menu_tab* tab = find_tab(menu, tab_id);
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
    item->value.str = item->options[default_idx];

    tab->items[tab->item_count++] = item;
}

void clk_menu_add_item_int(clk_menu* menu, int tab_id, int item_id, const char* label,
                           double default_val, double min_val, double max_val, double step_val) {
    if (!menu || !label)
        return;

    clk_menu_tab* tab = find_tab(menu, tab_id);
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
    item->value.num = default_val;

    tab->items[tab->item_count++] = item;
}

void clk_menu_add_item_bool(clk_menu* menu, int tab_id, int item_id, const char* label,
                            bool default_val) {
    if (!menu || !label)
        return;

    clk_menu_tab* tab = find_tab(menu, tab_id);
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

void clk_menu_add_item_action(clk_menu* menu, int tab_id, int item_id, const char* label) {
    if (!menu || !label)
        return;

    clk_menu_tab* tab = find_tab(menu, tab_id);
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

void clk_menu_remove_item(clk_menu* menu, int tab_id, int item_id) {
    clk_menu_tab* tab = find_tab(menu, tab_id);
    if (!tab)
        return;

    for (size_t i = 0; i < tab->item_count; ++i) {
        if (tab->items[i]->id == item_id) {
            clk_menu_item_destroy(tab->items[i]);
            for (size_t j = i; j + 1 < tab->item_count; ++j)
                tab->items[j] = tab->items[j + 1];
            tab->item_count--;
            tab->items[tab->item_count] = NULL;

            if (tab->item_count > 0 && tab->item_count <= tab->item_capacity / 2) {
                size_t new_cap = tab->item_capacity / 2;
                clk_menu_item** tmp = realloc(tab->items, new_cap * sizeof(clk_menu_item*));
                if (tmp) {
                    tab->items = tmp;
                    tab->item_capacity = new_cap;
                }
            } else if (tab->item_count == 0) {
                free(tab->items);
                tab->items = NULL;
                tab->item_capacity = 0;
            }

            if (tab->active_item > 0 && tab->active_item >= (int)tab->item_count)
                tab->active_item = (int)tab->item_count - 1;
            return;
        }
    }
}

/* ------------------------------------------------------------------
 *  Interaction
 * ------------------------------------------------------------------ */

clk_menu_event clk_menu_handle_input(clk_menu* menu, clk_menu_input input) {
    clk_menu_event event = {.type = CLK_MENU_EVENT_NONE};

    if (!menu || menu->tab_count == 0)
        return event;

    clk_menu_tab* tab = menu->tabs[menu->active_tab];
    if (!tab)
        return event;

    event.tab_id = tab->id;

    if (tab->item_count > 0 && tab->active_item < (int)tab->item_count) {
        clk_menu_item* item = tab->items[tab->active_item];
        if (item)
            event.item_id = item->id;
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
                    item->value.str = item->options[item->option_idx];
                    event.type = CLK_MENU_EVENT_VALUE_CHANGED;
                    event.value.str = item->value.str;
                    break;
                case CLK_MENU_TYPE_INT: {
                    double new_val = item->value.num - item->step_val;
                    if (new_val < item->min_val)
                        new_val = item->min_val;
                    if (new_val != item->value.num) {
                        item->value.num = new_val;
                        event.type = CLK_MENU_EVENT_VALUE_CHANGED;
                        event.value.num = item->value.num;
                    }
                    break;
                }
                case CLK_MENU_TYPE_BOOL:
                    item->value.b = !item->value.b;
                    event.type = CLK_MENU_EVENT_VALUE_CHANGED;
                    event.value.b = item->value.b;
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
                    item->value.str = item->options[item->option_idx];
                    event.type = CLK_MENU_EVENT_VALUE_CHANGED;
                    event.value.str = item->value.str;
                    break;
                case CLK_MENU_TYPE_INT: {
                    double new_val = item->value.num + item->step_val;
                    if (new_val > item->max_val)
                        new_val = item->max_val;
                    if (new_val != item->value.num) {
                        item->value.num = new_val;
                        event.type = CLK_MENU_EVENT_VALUE_CHANGED;
                        event.value.num = item->value.num;
                    }
                    break;
                }
                case CLK_MENU_TYPE_BOOL:
                    item->value.b = !item->value.b;
                    event.type = CLK_MENU_EVENT_VALUE_CHANGED;
                    event.value.b = item->value.b;
                    break;
                case CLK_MENU_TYPE_ACTION:
                    break;
            }
            break;
        }

        case CLK_MENU_INPUT_NEXT_TAB:
            if (menu->active_tab + 1 < (int)menu->tab_count)
                menu->active_tab++;
            else
                menu->active_tab = 0;
            break;

        case CLK_MENU_INPUT_CONFIRM: {
            if (tab->item_count == 0)
                break;
            clk_menu_item* item = tab->items[tab->active_item];
            if (item && item->type == CLK_MENU_TYPE_ACTION)
                event.type = CLK_MENU_EVENT_SUBMIT;
            break;
        }
    }

    /* refresh tab_id / item_id in case active changed */
    if (menu->tab_count > 0) {
        tab = menu->tabs[menu->active_tab];
        if (tab) {
            event.tab_id = tab->id;
            if (tab->item_count > 0 && tab->active_item < (int)tab->item_count) {
                clk_menu_item* item = tab->items[tab->active_item];
                if (item)
                    event.item_id = item->id;
            }
        }
    }

    return event;
}

/* ------------------------------------------------------------------
 *  External sync
 * ------------------------------------------------------------------ */

bool clk_menu_set_value_str(clk_menu* menu, int tab_id, int item_id, const char* val) {
    if (!menu || !val)
        return false;
    clk_menu_item* item = find_item(menu, tab_id, item_id);
    if (!item || item->type != CLK_MENU_TYPE_STR)
        return false;

    for (size_t i = 0; i < item->option_count; ++i) {
        if (strcmp(val, item->options[i]) == 0) {
            item->option_idx = i;
            item->value.str = item->options[i];
            return true;
        }
    }
    return false;
}

bool clk_menu_set_value_int(clk_menu* menu, int tab_id, int item_id, double val) {
    if (!menu)
        return false;
    clk_menu_item* item = find_item(menu, tab_id, item_id);
    if (!item || item->type != CLK_MENU_TYPE_INT)
        return false;

    if (val < item->min_val)
        item->value.num = item->min_val;
    else if (val > item->max_val)
        item->value.num = item->max_val;
    else
        item->value.num = val;
    return true;
}

bool clk_menu_set_value_bool(clk_menu* menu, int tab_id, int item_id, bool val) {
    if (!menu)
        return false;
    clk_menu_item* item = find_item(menu, tab_id, item_id);
    if (!item || item->type != CLK_MENU_TYPE_BOOL)
        return false;

    item->value.b = val;
    return true;
}

/* ------------------------------------------------------------------
 *  Dynamic options
 * ------------------------------------------------------------------ */

void clk_menu_add_option(clk_menu* menu, int tab_id, int item_id, const char* opt) {
    if (!menu || !opt)
        return;
    clk_menu_item* item = find_item(menu, tab_id, item_id);
    if (!item || item->type != CLK_MENU_TYPE_STR)
        return;

    char* dup = strdup(opt);
    if (!dup)
        return;

    int n = item->option_count + 1;
    char** tmp = realloc(item->options, n * sizeof(char*));
    if (!tmp) {
        free(dup);
        return;
    }

    tmp[item->option_count] = dup;
    item->options = tmp;
    item->option_count = n;
}

void clk_menu_remove_option(clk_menu* menu, int tab_id, int item_id, int idx) {
    if (!menu)
        return;
    clk_menu_item* item = find_item(menu, tab_id, item_id);
    if (!item || item->type != CLK_MENU_TYPE_STR)
        return;
    if (idx < 0 || idx >= item->option_count)
        return;

    free(item->options[idx]);

    for (int i = idx; i + 1 < item->option_count; ++i)
        item->options[i] = item->options[i + 1];

    item->option_count--;

    if (item->option_count == 0) {
        free(item->options);
        item->options = NULL;
    } else {
        char** tmp = realloc(item->options, item->option_count * sizeof(char*));
        if (tmp)
            item->options = tmp;
    }

    if (item->option_idx >= item->option_count)
        item->option_idx = item->option_count > 0 ? item->option_count - 1 : 0;
    if (item->option_count > 0)
        item->value.str = item->options[item->option_idx];
}

void clk_menu_clear_options(clk_menu* menu, int tab_id, int item_id) {
    if (!menu)
        return;
    clk_menu_item* item = find_item(menu, tab_id, item_id);
    if (!item || item->type != CLK_MENU_TYPE_STR)
        return;

    for (int i = 0; i < item->option_count; ++i)
        free(item->options[i]);
    free(item->options);

    item->options = NULL;
    item->option_count = 0;
    item->option_idx = 0;
}

/* ------------------------------------------------------------------
 *  INT range
 * ------------------------------------------------------------------ */

void clk_menu_set_item_range(clk_menu* menu, int tab_id, int item_id, double min_val,
                             double max_val, double step_val) {
    if (!menu)
        return;
    clk_menu_item* item = find_item(menu, tab_id, item_id);
    if (!item || item->type != CLK_MENU_TYPE_INT)
        return;

    item->min_val = min_val;
    item->max_val = max_val;
    item->step_val = step_val;

    if (item->value.num < min_val)
        item->value.num = min_val;
    else if (item->value.num > max_val)
        item->value.num = max_val;
}
