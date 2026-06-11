#include "clk_menu.h"

#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------
 *  Internal helper
 * ------------------------------------------------------------------ */

/** Find an item by tab_id + item_id. Returns NULL if not found. */
static clk_menu_item* find_item(clk_menu* m, int tab_id, int item_id) {
    for (size_t ti = 0; ti < m->tab_count; ++ti) {
        if (m->tabs[ti]->id != tab_id)
            continue;
        for (size_t ii = 0; ii < m->tabs[ti]->item_count; ++ii) {
            if (m->tabs[ti]->items[ii]->id == item_id)
                return m->tabs[ti]->items[ii];
        }
    }
    return NULL;
}

/* ------------------------------------------------------------------
 *  Lifecycle
 * ------------------------------------------------------------------ */

clk_menu* clk_menu_create(void) {
    /* TODO: allocate clk_menu, zero-init, return */
    return NULL;
}

void clk_menu_destroy(clk_menu* m) {
    /* TODO: free all tabs + items + option strings, then free m */
}

/* ------------------------------------------------------------------
 *  Tabs
 * ------------------------------------------------------------------ */

int clk_menu_add_tab(clk_menu* m, const char* name) {
    /* TODO: allocate clk_menu_tab, set id (auto-increment), strdup name, add to tabs[] */
    return -1;
}

/* ------------------------------------------------------------------
 *  Items
 * ------------------------------------------------------------------ */

void clk_menu_add_item_str(clk_menu* m, int tab_id, int item_id, const char* label, int default_idx,
                           const char** options, int option_count) {
    /* TODO: find tab, allocate item, set STR fields, clamp option_idx, add to tab->items[] */
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
