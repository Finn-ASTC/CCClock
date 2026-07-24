#include "clk_menu.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#define CLK_TAB_LIST_DEFAULT_CAPACITY 6
#define CLK_ITEM_LIST_DEFAULT_CAPACITY 6

/* ================================================================
 *  Internal helper — item lifecycle
 * ================================================================ */

static void clk_menu_item_destroy(clk_menu_item* item);

/* ================================================================
 *  clk_item_list
 * ================================================================ */

clk_item_list* clk_item_list_create(void) {
    clk_item_list* list = malloc(sizeof(clk_item_list));
    if (!list)
        return NULL;
    memset(list, 0, sizeof(clk_item_list));

    clk_menu_item** items = malloc(CLK_ITEM_LIST_DEFAULT_CAPACITY * sizeof(clk_menu_item*));
    if (!items) {
        free(list);
        return NULL;
    }
    memset(items, 0, CLK_ITEM_LIST_DEFAULT_CAPACITY * sizeof(clk_menu_item*));
    list->items = items;
    list->capacity = CLK_ITEM_LIST_DEFAULT_CAPACITY;
    return list;
}

void clk_item_list_destroy(clk_item_list* list) {
    if (!list)
        return;
    for (size_t i = 0; i < list->count; ++i)
        clk_menu_item_destroy(list->items[i]);
    free(list->items);
    free(list);
}

static bool clk_item_list_ensure_capacity(clk_item_list* list) {
    if (list->count < list->capacity)
        return true;
    size_t new_capacity = list->capacity > 0 ? list->capacity * 2 : CLK_ITEM_LIST_DEFAULT_CAPACITY;
    clk_menu_item** tmp = realloc(list->items, new_capacity * sizeof(clk_menu_item*));
    if (!tmp)
        return false;
    memset(tmp + list->capacity, 0, (new_capacity - list->capacity) * sizeof(clk_menu_item*));
    list->items = tmp;
    list->capacity = new_capacity;
    return true;
}

static clk_menu_item* clk_item_list_insert(clk_item_list* list, int tab_id, int item_id,
                                           int position) {
    if (!clk_item_list_ensure_capacity(list))
        return NULL;

    if (position < 0 || position > (int)list->count)
        position = (int)list->count;

    clk_menu_item* item = malloc(sizeof(clk_menu_item));
    if (!item)
        return NULL;
    memset(item, 0, sizeof(clk_menu_item));
    item->id = item_id;
    item->tab_id = tab_id;

    for (int i = (int)list->count; i > position; --i)
        list->items[i] = list->items[i - 1];
    list->items[position] = item;
    list->count++;
    return item;
}

static void clk_item_list_shrink_if_half(clk_item_list* list) {
    if (list->count > 0 && list->count <= list->capacity / 2) {
        size_t new_capacity = list->capacity / 2;
        clk_menu_item** tmp = realloc(list->items, new_capacity * sizeof(clk_menu_item*));
        if (tmp) {
            list->items = tmp;
            list->capacity = new_capacity;
        }
    } else if (list->count == 0) {
        free(list->items);
        list->items = NULL;
        list->capacity = 0;
    }
}

clk_menu_item* clk_item_list_find(const clk_item_list* list, int item_id) {
    if (!list)
        return NULL;
    for (size_t i = 0; i < list->count; ++i)
        if (list->items[i]->id == item_id)
            return list->items[i];
    return NULL;
}

clk_menu_item* clk_item_list_get_at(const clk_item_list* list, size_t position) {
    if (!list || position >= list->count)
        return NULL;
    return list->items[position];
}

size_t clk_item_list_count(const clk_item_list* list) {
    return list ? list->count : 0;
}

/* ── add_str ── */

void clk_item_list_add_str(clk_item_list* list, int tab_id, int item_id, const char* label,
                           int default_idx, const char** options, int option_count) {
    clk_item_list_add_str_at(list, tab_id, item_id, label, default_idx, options, option_count, -1);
}

void clk_item_list_add_str_at(clk_item_list* list, int tab_id, int item_id, const char* label,
                              int default_idx, const char** options, int option_count,
                              int position) {
    if (!list || !label || !options || option_count <= 0)
        return;
    clk_menu_item* item = clk_item_list_insert(list, tab_id, item_id, position);
    if (!item)
        return;

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
}

/* ── add_int ── */

void clk_item_list_add_int(clk_item_list* list, int tab_id, int item_id, const char* label,
                           double default_val, double min_val, double max_val, double step_val) {
    clk_item_list_add_int_at(list, tab_id, item_id, label, default_val, min_val, max_val, step_val,
                             -1);
}

void clk_item_list_add_int_at(clk_item_list* list, int tab_id, int item_id, const char* label,
                              double default_val, double min_val, double max_val, double step_val,
                              int position) {
    if (!list || !label)
        return;
    clk_menu_item* item = clk_item_list_insert(list, tab_id, item_id, position);
    if (!item)
        return;

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
}

/* ── add_bool ── */

void clk_item_list_add_bool(clk_item_list* list, int tab_id, int item_id, const char* label,
                            bool default_val) {
    clk_item_list_add_bool_at(list, tab_id, item_id, label, default_val, -1);
}

void clk_item_list_add_bool_at(clk_item_list* list, int tab_id, int item_id, const char* label,
                               bool default_val, int position) {
    if (!list || !label)
        return;
    clk_menu_item* item = clk_item_list_insert(list, tab_id, item_id, position);
    if (!item)
        return;

    item->type = CLK_MENU_TYPE_BOOL;
    item->label = strdup(label);
    if (!item->label) {
        free(item);
        return;
    }
    item->value.b = default_val;
}

/* ── add_action ── */

void clk_item_list_add_action(clk_item_list* list, int tab_id, int item_id, const char* label) {
    clk_item_list_add_action_at(list, tab_id, item_id, label, -1);
}

void clk_item_list_add_action_at(clk_item_list* list, int tab_id, int item_id, const char* label,
                                 int position) {
    if (!list || !label)
        return;
    clk_menu_item* item = clk_item_list_insert(list, tab_id, item_id, position);
    if (!item)
        return;

    item->type = CLK_MENU_TYPE_ACTION;
    item->label = strdup(label);
    if (!item->label) {
        free(item);
        return;
    }
}

/* ── remove / clear ── */

void clk_item_list_remove(clk_item_list* list, int item_id) {
    if (!list)
        return;
    for (size_t i = 0; i < list->count; ++i) {
        if (list->items[i]->id == item_id) {
            clk_menu_item_destroy(list->items[i]);
            for (size_t j = i; j + 1 < list->count; ++j)
                list->items[j] = list->items[j + 1];
            list->count--;
            list->items[list->count] = NULL;
            clk_item_list_shrink_if_half(list);
            return;
        }
    }
}

void clk_item_list_remove_at(clk_item_list* list, size_t position) {
    if (!list || position >= list->count)
        return;
    clk_menu_item_destroy(list->items[position]);
    for (size_t j = position; j + 1 < list->count; ++j)
        list->items[j] = list->items[j + 1];
    list->count--;
    list->items[list->count] = NULL;
    clk_item_list_shrink_if_half(list);
}

void clk_item_list_clear(clk_item_list* list) {
    if (!list)
        return;
    for (size_t i = 0; i < list->count; ++i)
        clk_menu_item_destroy(list->items[i]);
    free(list->items);
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
}

/* ── set_value ── */

bool clk_item_list_set_value_str(clk_item_list* list, int item_id, const char* val) {
    if (!list || !val)
        return false;
    clk_menu_item* item = clk_item_list_find(list, item_id);
    if (!item || item->type != CLK_MENU_TYPE_STR)
        return false;

    for (size_t i = 0; i < (size_t)item->option_count; ++i) {
        if (strcmp(val, item->options[i]) == 0) {
            item->option_idx = i;
            item->value.str = item->options[i];
            return true;
        }
    }
    return false;
}

bool clk_item_list_set_value_int(clk_item_list* list, int item_id, double val) {
    if (!list)
        return false;
    clk_menu_item* item = clk_item_list_find(list, item_id);
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

bool clk_item_list_set_value_bool(clk_item_list* list, int item_id, bool val) {
    if (!list)
        return false;
    clk_menu_item* item = clk_item_list_find(list, item_id);
    if (!item || item->type != CLK_MENU_TYPE_BOOL)
        return false;

    item->value.b = val;
    return true;
}

/* ── options ── */

void clk_item_list_add_option(clk_item_list* list, int item_id, const char* opt) {
    if (!list || !opt)
        return;
    clk_menu_item* item = clk_item_list_find(list, item_id);
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

void clk_item_list_remove_option(clk_item_list* list, int item_id, int idx) {
    if (!list)
        return;
    clk_menu_item* item = clk_item_list_find(list, item_id);
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

void clk_item_list_clear_options(clk_item_list* list, int item_id) {
    if (!list)
        return;
    clk_menu_item* item = clk_item_list_find(list, item_id);
    if (!item || item->type != CLK_MENU_TYPE_STR)
        return;

    for (int i = 0; i < item->option_count; ++i)
        free(item->options[i]);
    free(item->options);

    item->options = NULL;
    item->option_count = 0;
    item->option_idx = 0;
}

/* ── range ── */

void clk_item_list_set_range(clk_item_list* list, int item_id, double min_val, double max_val,
                             double step_val) {
    if (!list)
        return;
    clk_menu_item* item = clk_item_list_find(list, item_id);
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

/* ── rebuild ── */

void clk_item_list_rebuild_item(clk_item_list* list, int item_id, const char** options, int count,
                                int new_index) {
    clk_item_list_clear_options(list, item_id);
    for (int i = 0; i < count; ++i)
        clk_item_list_add_option(list, item_id, options[i]);
    if (count > 0)
        clk_item_list_set_value_str(list, item_id, options[new_index]);
}

/* ================================================================
 *  clk_tab_list
 * ================================================================ */

clk_tab_list* clk_tab_list_create(void) {
    clk_tab_list* list = malloc(sizeof(clk_tab_list));
    if (!list)
        return NULL;
    memset(list, 0, sizeof(clk_tab_list));

    clk_menu_tab** tabs = malloc(CLK_TAB_LIST_DEFAULT_CAPACITY * sizeof(clk_menu_tab*));
    if (!tabs) {
        free(list);
        return NULL;
    }
    memset(tabs, 0, CLK_TAB_LIST_DEFAULT_CAPACITY * sizeof(clk_menu_tab*));
    list->tabs = tabs;
    list->capacity = CLK_TAB_LIST_DEFAULT_CAPACITY;
    return list;
}

void clk_tab_list_destroy(clk_tab_list* list) {
    if (!list)
        return;
    for (size_t i = 0; i < list->count; ++i) {
        clk_menu_tab* tab = list->tabs[i];
        clk_item_list_destroy(tab->item_list);
        free(tab->name);
        free(tab);
    }
    free(list->tabs);
    free(list);
}

clk_menu_tab* clk_tab_list_find(const clk_tab_list* list, int tab_id) {
    if (!list)
        return NULL;
    for (size_t i = 0; i < list->count; ++i)
        if (list->tabs[i]->id == tab_id)
            return list->tabs[i];
    return NULL;
}

clk_menu_tab* clk_tab_list_get_at(const clk_tab_list* list, size_t position) {
    if (!list || position >= list->count)
        return NULL;
    return list->tabs[position];
}

size_t clk_tab_list_count(const clk_tab_list* list) {
    return list ? list->count : 0;
}

int clk_tab_list_add(clk_tab_list* list, int tab_id, const char* name) {
    if (!list || !name)
        return -1;

    if (list->count >= list->capacity) {
        size_t new_capacity = list->capacity * 2;
        clk_menu_tab** tmp = realloc(list->tabs, new_capacity * sizeof(clk_menu_tab*));
        if (!tmp)
            return -1;
        memset(tmp + list->capacity, 0, (new_capacity - list->capacity) * sizeof(clk_menu_tab*));
        list->tabs = tmp;
        list->capacity = new_capacity;
    }

    clk_menu_tab* tab = malloc(sizeof(clk_menu_tab));
    if (!tab)
        return -1;
    memset(tab, 0, sizeof(clk_menu_tab));

    tab->name = strdup(name);
    if (!tab->name) {
        free(tab);
        return -1;
    }

    tab->item_list = clk_item_list_create();
    if (!tab->item_list) {
        free(tab->name);
        free(tab);
        return -1;
    }

    tab->id = tab_id;
    list->tabs[list->count++] = tab;
    return tab->id;
}

void clk_tab_list_remove(clk_tab_list* list, int tab_id) {
    if (!list)
        return;
    for (size_t i = 0; i < list->count; ++i) {
        if (list->tabs[i]->id == tab_id) {
            clk_menu_tab* tab = list->tabs[i];
            clk_item_list_destroy(tab->item_list);
            free(tab->name);
            free(tab);
            for (size_t j = i; j + 1 < list->count; ++j)
                list->tabs[j] = list->tabs[j + 1];
            list->count--;
            list->tabs[list->count] = NULL;
            return;
        }
    }
}

void clk_tab_list_set_item_list(clk_tab_list* list, int tab_id, clk_item_list* new_item_list) {
    if (!list)
        return;
    clk_menu_tab* tab = clk_tab_list_find(list, tab_id);
    if (!tab)
        return;
    clk_item_list_destroy(tab->item_list);
    tab->item_list = new_item_list ? new_item_list : clk_item_list_create();
    tab->active_item = 0;
}

const clk_item_list* clk_tab_list_get_item_list(const clk_tab_list* list, int tab_id) {
    clk_menu_tab* tab = clk_tab_list_find((clk_tab_list*)list, tab_id);
    return tab ? tab->item_list : NULL;
}

/* ================================================================
 *  clk_menu_item_destroy (internal)
 * ================================================================ */

static void clk_menu_item_destroy(clk_menu_item* item) {
    if (!item)
        return;

    free(item->label);
    item->label = NULL;

    for (int i = 0; i < item->option_count; ++i)
        free(item->options[i]);

    free(item->options);
    item->options = NULL;

    free(item);
}

/* ================================================================
 *  clk_menu — wrappers
 * ================================================================ */

clk_menu* clk_menu_create(void) {
    clk_menu* menu = malloc(sizeof(clk_menu));
    if (!menu)
        return NULL;
    memset(menu, 0, sizeof(clk_menu));

    menu->tab_list = clk_tab_list_create();
    if (!menu->tab_list) {
        free(menu);
        return NULL;
    }
    return menu;
}

void clk_menu_destroy(clk_menu* menu) {
    if (!menu)
        return;
    clk_tab_list_destroy(menu->tab_list);
    free(menu);
}

/* ── tab ── */

int clk_menu_add_tab(clk_menu* menu, int tab_id, const char* name) {
    if (!menu)
        return -1;
    return clk_tab_list_add(menu->tab_list, tab_id, name);
}

/* ── items ── */

void clk_menu_add_item_str(clk_menu* menu, int tab_id, int item_id, const char* label,
                           int default_idx, const char** options, int option_count) {
    if (!menu)
        return;
    clk_menu_tab* tab = clk_tab_list_find(menu->tab_list, tab_id);
    if (!tab)
        return;
    clk_item_list_add_str(tab->item_list, tab_id, item_id, label, default_idx, options,
                          option_count);
}

void clk_menu_add_item_str_at(clk_menu* menu, int tab_id, int item_id, const char* label,
                              int default_idx, const char** options, int option_count,
                              int position) {
    if (!menu)
        return;
    clk_menu_tab* tab = clk_tab_list_find(menu->tab_list, tab_id);
    if (!tab)
        return;
    clk_item_list_add_str_at(tab->item_list, tab_id, item_id, label, default_idx, options,
                             option_count, position);
}

void clk_menu_add_item_int(clk_menu* menu, int tab_id, int item_id, const char* label,
                           double default_val, double min_val, double max_val, double step_val) {
    if (!menu)
        return;
    clk_menu_tab* tab = clk_tab_list_find(menu->tab_list, tab_id);
    if (!tab)
        return;
    clk_item_list_add_int(tab->item_list, tab_id, item_id, label, default_val, min_val, max_val,
                          step_val);
}

void clk_menu_add_item_int_at(clk_menu* menu, int tab_id, int item_id, const char* label,
                              double default_val, double min_val, double max_val, double step_val,
                              int position) {
    if (!menu)
        return;
    clk_menu_tab* tab = clk_tab_list_find(menu->tab_list, tab_id);
    if (!tab)
        return;
    clk_item_list_add_int_at(tab->item_list, tab_id, item_id, label, default_val, min_val, max_val,
                             step_val, position);
}

void clk_menu_add_item_bool(clk_menu* menu, int tab_id, int item_id, const char* label,
                            bool default_val) {
    if (!menu)
        return;
    clk_menu_tab* tab = clk_tab_list_find(menu->tab_list, tab_id);
    if (!tab)
        return;
    clk_item_list_add_bool(tab->item_list, tab_id, item_id, label, default_val);
}

void clk_menu_add_item_bool_at(clk_menu* menu, int tab_id, int item_id, const char* label,
                               bool default_val, int position) {
    if (!menu)
        return;
    clk_menu_tab* tab = clk_tab_list_find(menu->tab_list, tab_id);
    if (!tab)
        return;
    clk_item_list_add_bool_at(tab->item_list, tab_id, item_id, label, default_val, position);
}

void clk_menu_add_item_action(clk_menu* menu, int tab_id, int item_id, const char* label) {
    if (!menu)
        return;
    clk_menu_tab* tab = clk_tab_list_find(menu->tab_list, tab_id);
    if (!tab)
        return;
    clk_item_list_add_action(tab->item_list, tab_id, item_id, label);
}

void clk_menu_add_item_action_at(clk_menu* menu, int tab_id, int item_id, const char* label,
                                 int position) {
    if (!menu)
        return;
    clk_menu_tab* tab = clk_tab_list_find(menu->tab_list, tab_id);
    if (!tab)
        return;
    clk_item_list_add_action_at(tab->item_list, tab_id, item_id, label, position);
}

void clk_menu_remove_item(clk_menu* menu, int tab_id, int item_id) {
    if (!menu)
        return;
    clk_menu_tab* tab = clk_tab_list_find(menu->tab_list, tab_id);
    if (!tab)
        return;
    clk_item_list* ilist = tab->item_list;
    clk_item_list_remove(ilist, item_id);
    if (tab->active_item > 0 && tab->active_item >= (int)clk_item_list_count(ilist))
        tab->active_item = (int)clk_item_list_count(ilist) - 1;
}

/* ── handle_input ── */

clk_menu_event clk_menu_handle_input(clk_menu* menu, clk_menu_input input) {
    clk_menu_event event = {.type = CLK_MENU_EVENT_NONE};
    clk_tab_list* tlist;

    if (!menu || clk_tab_list_count(menu->tab_list) == 0)
        return event;
    tlist = menu->tab_list;

    clk_menu_tab* tab = clk_tab_list_get_at(tlist, menu->active_tab);
    if (!tab)
        return event;

    event.tab_id = tab->id;
    clk_item_list* ilist = tab->item_list;

    if (clk_item_list_count(ilist) > 0 && tab->active_item < (int)clk_item_list_count(ilist)) {
        clk_menu_item* item = clk_item_list_get_at(ilist, tab->active_item);
        if (item)
            event.item_id = item->id;
    }

    switch (input) {
        case CLK_MENU_INPUT_NONE:
            break;

        case CLK_MENU_INPUT_PREV_ITEM:
            if (clk_item_list_count(ilist) == 0)
                break;
            if (tab->active_item > 0)
                tab->active_item--;
            break;

        case CLK_MENU_INPUT_NEXT_ITEM:
            if (clk_item_list_count(ilist) == 0)
                break;
            if (tab->active_item < (int)clk_item_list_count(ilist) - 1)
                tab->active_item++;
            break;

        case CLK_MENU_INPUT_DEC_VALUE: {
            if (clk_item_list_count(ilist) == 0)
                break;
            clk_menu_item* item = clk_item_list_get_at(ilist, tab->active_item);
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
            if (clk_item_list_count(ilist) == 0)
                break;
            clk_menu_item* item = clk_item_list_get_at(ilist, tab->active_item);
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
            if (menu->active_tab + 1 < (int)clk_tab_list_count(tlist))
                menu->active_tab++;
            else
                menu->active_tab = 0;
            break;

        case CLK_MENU_INPUT_CONFIRM: {
            if (clk_item_list_count(ilist) == 0)
                break;
            clk_menu_item* item = clk_item_list_get_at(ilist, tab->active_item);
            if (item && item->type == CLK_MENU_TYPE_ACTION)
                event.type = CLK_MENU_EVENT_SUBMIT;
            break;
        }
    }

    if (clk_tab_list_count(tlist) > 0) {
        tab = clk_tab_list_get_at(tlist, menu->active_tab);
        if (tab) {
            event.tab_id = tab->id;
            ilist = tab->item_list;
            if (clk_item_list_count(ilist) > 0 &&
                tab->active_item < (int)clk_item_list_count(ilist)) {
                clk_menu_item* item = clk_item_list_get_at(ilist, tab->active_item);
                if (item)
                    event.item_id = item->id;
            }
        }
    }

    return event;
}

/* ── set_value ── */

bool clk_menu_set_value_str(clk_menu* menu, int tab_id, int item_id, const char* val) {
    if (!menu)
        return false;
    clk_menu_tab* tab = clk_tab_list_find(menu->tab_list, tab_id);
    return tab ? clk_item_list_set_value_str(tab->item_list, item_id, val) : false;
}

bool clk_menu_set_value_int(clk_menu* menu, int tab_id, int item_id, double val) {
    if (!menu)
        return false;
    clk_menu_tab* tab = clk_tab_list_find(menu->tab_list, tab_id);
    return tab ? clk_item_list_set_value_int(tab->item_list, item_id, val) : false;
}

bool clk_menu_set_value_bool(clk_menu* menu, int tab_id, int item_id, bool val) {
    if (!menu)
        return false;
    clk_menu_tab* tab = clk_tab_list_find(menu->tab_list, tab_id);
    return tab ? clk_item_list_set_value_bool(tab->item_list, item_id, val) : false;
}

/* ── options ── */

void clk_menu_add_option(clk_menu* menu, int tab_id, int item_id, const char* opt) {
    if (!menu)
        return;
    clk_menu_tab* tab = clk_tab_list_find(menu->tab_list, tab_id);
    if (!tab)
        return;
    clk_item_list_add_option(tab->item_list, item_id, opt);
}

void clk_menu_remove_option(clk_menu* menu, int tab_id, int item_id, int idx) {
    if (!menu)
        return;
    clk_menu_tab* tab = clk_tab_list_find(menu->tab_list, tab_id);
    if (!tab)
        return;
    clk_item_list_remove_option(tab->item_list, item_id, idx);
}

void clk_menu_clear_options(clk_menu* menu, int tab_id, int item_id) {
    if (!menu)
        return;
    clk_menu_tab* tab = clk_tab_list_find(menu->tab_list, tab_id);
    if (!tab)
        return;
    clk_item_list_clear_options(tab->item_list, item_id);
}

/* ── range ── */

void clk_menu_set_item_range(clk_menu* menu, int tab_id, int item_id, double min_val,
                             double max_val, double step_val) {
    if (!menu)
        return;
    clk_menu_tab* tab = clk_tab_list_find(menu->tab_list, tab_id);
    if (!tab)
        return;
    clk_item_list_set_range(tab->item_list, item_id, min_val, max_val, step_val);
}

/* ── path-list helpers ── */

char** clk_menu_build_names(char** paths, int count) {
    char** names = calloc(count, sizeof(char*));
    if (!names)
        return NULL;
    for (int i = 0; i < count; ++i) {
        const char* last_slash = NULL;
        for (const char* p = paths[i]; *p; ++p)
            if (*p == '/' || *p == '\\')
                last_slash = p;
        const char* start = last_slash ? last_slash + 1 : paths[i];
        names[i] = strdup(start);
        char* dot = strrchr(names[i], '.');
        if (dot)
            *dot = '\0';
    }
    return names;
}

const char** clk_menu_wrap_strings(char** strings, int count) {
    const char** result = calloc(count, sizeof(const char*));
    if (!result)
        return NULL;
    for (int i = 0; i < count; ++i)
        result[i] = strings[i];
    return result;
}

int clk_menu_find_index(const char* needle, const char** haystack, int count, int fallback) {
    for (int i = 0; i < count; ++i)
        if (strcmp(needle, haystack[i]) == 0)
            return i;
    return fallback;
}

void clk_menu_rebuild_item(clk_menu* menu, int tab_id, int item_id, const char** options, int count,
                           int new_index) {
    if (!menu)
        return;
    clk_menu_tab* tab = clk_tab_list_find(menu->tab_list, tab_id);
    if (!tab)
        return;
    clk_item_list_rebuild_item(tab->item_list, item_id, options, count, new_index);
}
