#include "clk_menu_instance.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "clk_menu_theme.h"
#include "clk_term.h"

#define CLK_MENU_DEFAULT_ACTIVE_POS 1

#define CLK_MENU_ITEM_VAL_BUF_SIZE 32

/* ================================================================
 *  Lifecycle
 * ================================================================ */

clk_menu_instance* clk_menu_instance_create(clk_menu* menu, clk_menu_theme* theme) {
    if (!menu || !theme)
        return NULL;

    clk_menu_instance* instance = malloc(sizeof(clk_menu_instance));
    if (!instance)
        return NULL;
    memset(instance, 0, sizeof(clk_menu_instance));

    instance->active_item_pos_idx = CLK_MENU_DEFAULT_ACTIVE_POS;
    instance->last_active_item_pos_idx = CLK_MENU_DEFAULT_ACTIVE_POS;
    instance->align_top = true;

    instance->menu = menu;
    instance->theme = theme;

    if (theme->min_width <= 0 || theme->min_height <= 0) {
        free(instance);
        return NULL;
    }

    instance->tex = clk_texture_create(theme->min_width, theme->min_height);
    instance->sprite = clk_sprite_create_with_texture(&instance->tex, 0, 0, 0);
    return instance;
}

void clk_menu_instance_destroy(clk_menu_instance* instance) {
    if (!instance)
        return;
    clk_menu_instance_remove_from_term(instance);
    clk_texture_destroy(&instance->tex);
    clk_sprite_destroy(instance->sprite);
    free(instance);
}

/* ================================================================
 *  Layout & visibility
 * ================================================================ */

void clk_menu_instance_set_position(clk_menu_instance* instance, int x, int y) {
    if (!instance || !instance->sprite)
        return;
    instance->sprite->posx = x;
    instance->sprite->posy = y;
}

void clk_menu_instance_set_size(clk_menu_instance* instance, int w, int h) {
    if (!instance)
        return;
    if (w < instance->theme->min_width)
        w = instance->theme->min_width;
    if (h < instance->theme->min_height)
        h = instance->theme->min_height;
    if (w <= 0 || h <= 0)
        return;
    if (w == instance->tex.tex_w && h == instance->tex.tex_h)
        return;
    clk_texture_destroy(&instance->tex);
    instance->tex = clk_texture_create(w, h);
    if (instance->sprite)
        instance->sprite->tex = &instance->tex;
}

void clk_menu_instance_set_visible(clk_menu_instance* instance, bool visible) {
    if (!instance || !instance->sprite)
        return;
    instance->sprite->is_hidden = !visible;
}

/* ================================================================
 *  Dynamic rebind
 * ================================================================ */

void clk_menu_instance_change_menu(clk_menu_instance* instance, clk_menu* menu) {
    if (!instance || !menu)
        return;
    instance->menu = menu;
    instance->active_item_pos_idx = CLK_MENU_DEFAULT_ACTIVE_POS;
    instance->last_active_item_pos_idx = CLK_MENU_DEFAULT_ACTIVE_POS;
    instance->align_top = true;
}

bool clk_menu_instance_change_theme(clk_menu_instance* instance, const char* theme_path) {
    if (!instance || !theme_path)
        return false;
    return clk_menu_theme_reload(theme_path, instance->theme);
}

/* ================================================================
 *  Render list
 * ================================================================ */

void clk_menu_instance_add_to_term(clk_menu_instance* instance) {
    if (!instance || !instance->sprite || instance->sprite_added)
        return;
    clk_term_add_sprite(instance->sprite);
    instance->sprite_added = true;
}

void clk_menu_instance_remove_from_term(clk_menu_instance* instance) {
    if (!instance || !instance->sprite || !instance->sprite_added)
        return;
    clk_term_remove_sprite(instance->sprite);
    instance->sprite_added = false;
}

/* ================================================================
 *  Interaction
 * ================================================================ */

clk_menu_event clk_menu_instance_handle_input(clk_menu_instance* instance, clk_menu_input input) {
    clk_menu_event ev = {.type = CLK_MENU_EVENT_NONE};
    /* Reject input when the texture is smaller than the theme minimum —
     * would cause rendering out of bounds. */
    if (!instance || (instance->sprite && instance->sprite->is_hidden) ||
        instance->tex.tex_w < instance->theme->min_width ||
        instance->tex.tex_h < instance->theme->min_height)
        return ev;

    if (input == CLK_MENU_INPUT_NEXT_ITEM) {
        instance->last_active_item_pos_idx = instance->active_item_pos_idx;
        instance->active_item_pos_idx++;
    }
    if (input == CLK_MENU_INPUT_PREV_ITEM) {
        instance->last_active_item_pos_idx = instance->active_item_pos_idx;
        instance->active_item_pos_idx--;
    }

    return clk_menu_handle_input(instance->menu, input);
}

/* ================================================================
 *  Render
 * ================================================================ */

static int render_def(const clk_menu* menu, clk_texture* tex, const clk_menu_def* def,
                      int tab_index, int item_idx, int x, int y, int max_x);
static int render_dyn_str(const clk_menu* menu, clk_texture* tex, const clk_menu_def* def,
                          int tab_index, int item_idx, int x, int y, int max_chars, int max_x);

/** Counts display cells (code points) in a UTF-8 string. Returns the cell width. */
static int utf8_cell_width(const char* s) {
    return clk_term_string_width(s);
}

/** Recursively measures the rendered cell width of a def for the given tab/item.
 *  Strings use their UTF-8 width, composites sum their members, and the tab/item
 *  leaves resolve to the live name, label, or formatted value length. Returns the width in cells.
 */
static int measure_def(const clk_menu* menu, const clk_menu_def* def, int tab_index, int item_idx) {
    if (!def)
        return 0;
    switch (def->type) {
        case CLK_MENU_DEF_STRING:
            return def->string_val ? utf8_cell_width(def->string_val) : 0;
        case CLK_MENU_DEF_COMPOSITE: {
            int w = 0;
            for (int i = 0; i < (int)def->member_cnt; ++i)
                w += measure_def(menu, def->members[i], tab_index, item_idx);
            return w;
        }
        case CLK_MENU_DEF_TAB_STR: {
            clk_tab_list* tlist = menu->tab_list;
            return (tab_index >= 0 && (size_t)tab_index < clk_tab_list_count(tlist))
                       ? clk_term_string_width(clk_tab_list_get_at(tlist, (size_t)tab_index)->name)
                       : 0;
        }
        case CLK_MENU_DEF_ITEM_LABEL_STR: {
            const clk_menu_tab* mtab = clk_tab_list_get_at(menu->tab_list, (size_t)tab_index);
            if (!mtab || item_idx < 0 || (size_t)item_idx >= clk_item_list_count(mtab->item_list))
                return 0;
            const clk_menu_item* it = clk_item_list_get_at(mtab->item_list, (size_t)item_idx);
            return it ? clk_term_string_width(it->label) : 0;
        }
        case CLK_MENU_DEF_ITEM_VALUE_STR: {
            const clk_menu_tab* mtab = clk_tab_list_get_at(menu->tab_list, (size_t)tab_index);
            if (!mtab || item_idx < 0 || (size_t)item_idx >= clk_item_list_count(mtab->item_list))
                return 0;
            const clk_menu_item* it = clk_item_list_get_at(mtab->item_list, (size_t)item_idx);
            if (!it)
                return 0;
            switch (it->type) {
                case CLK_MENU_TYPE_INT: {
                    char b[32];
                    return snprintf(b, sizeof(b), "%.0f", it->value.num);
                }
                case CLK_MENU_TYPE_BOOL:
                    return it->value.b ? (int)(sizeof("true") - 1) : (int)(sizeof("false") - 1);
                case CLK_MENU_TYPE_STR:
                    return clk_term_string_width(it->value.str);
                default:
                    return 0;
            }
        }
        default:
            return 0;
    }
}

/* ── render_* helpers ── */

/** Draws a static string def at (x,y) in its own style. Returns the cell width drawn. */
static int render_string(const clk_menu* menu, clk_texture* tex, const clk_menu_def* def, int x,
                         int y, int max_x) {
    (void)menu;
    (void)max_x;
    clk_texture_write_string(tex, x, y, def->string_val, def->style_id);
    return utf8_cell_width(def->string_val);
}

/** Renders each member of a composite def left to right, stopping once x reaches max_x.
 *  Returns the total cells drawn. */
static int render_composite(const clk_menu* menu, clk_texture* tex, const clk_menu_def* def,
                            int tab_index, int item_idx, int x, int y, int max_x) {
    int total = 0;
    for (int i = 0; i < (int)def->member_cnt; ++i) {
        if (x + total >= max_x)
            break;
        total += render_def(menu, tex, def->members[i], tab_index, item_idx, x + total, y, max_x);
    }
    return total;
}

/** Renders the whole tab bar by iterating every tab and drawing its active or
 *  inactive member defs. Each tab is measured first and skipped as a whole if
 *  it would overflow max_x. Returns the total cells drawn. */
static int render_tab_special(const clk_menu* menu, clk_texture* tex, const clk_menu_def* def,
                              int item_idx, int x, int y, int max_x) {
    int total = 0;
    size_t tab_count = clk_tab_list_count(menu->tab_list);
    for (size_t offset = 0; offset < tab_count; ++offset) {
        size_t ti = ((size_t)menu->active_tab + offset) % tab_count;
        bool act = (offset == 0);
        clk_menu_def** mbs =
            act ? (clk_menu_def**)def->active_members : (clk_menu_def**)def->inactive_members;
        int cnt = (int)(act ? def->active_cnt : def->inactive_cnt);

        int mw = 0;
        for (int i = 0; i < cnt; ++i)
            mw += measure_def(menu, mbs[i], (int)ti, item_idx);

        if (x + total + mw > max_x)
            break;

        for (int i = 0; i < cnt; ++i)
            total += render_def(menu, tex, mbs[i], (int)ti, item_idx, x + total, y, max_x);
    }
    return total;
}

/** Renders an item label/value group, picking active or inactive members.
 *  Fixed-width members are measured up front so the remaining space caps the
 *  dynamic label/value strings; any leftover gap up to max_x is padded with
 *  the trailing member's background style. Returns the total cells drawn. */
static int render_item_label_value_special(const clk_menu* menu, clk_texture* tex,
                                           const clk_menu_def* def, int tab_index, int item_idx,
                                           int x, int y, int max_x) {
    const clk_menu_tab* mtab = clk_tab_list_get_at(menu->tab_list, (size_t)tab_index);
    bool act = (item_idx >= 0 && mtab && (size_t)item_idx < clk_item_list_count(mtab->item_list) &&
                (int)mtab->active_item == item_idx);
    clk_menu_def** mbs =
        act ? (clk_menu_def**)def->active_members : (clk_menu_def**)def->inactive_members;
    int cnt = (int)(act ? def->active_cnt : def->inactive_cnt);

    int fixed = 0;
    for (int i = 0; i < cnt; ++i) {
        clk_menu_def_type t = mbs[i]->type;
        if (t != CLK_MENU_DEF_ITEM_LABEL_STR && t != CLK_MENU_DEF_ITEM_VALUE_STR)
            fixed += measure_def(menu, mbs[i], tab_index, item_idx);
    }
    int remaining = max_x - x - fixed;
    if (remaining < 0)
        remaining = 0;

    int total = 0;
    for (int i = 0; i < cnt; ++i) {
        if (x + total >= max_x)
            break;
        clk_menu_def_type t = mbs[i]->type;
        if (t == CLK_MENU_DEF_ITEM_LABEL_STR || t == CLK_MENU_DEF_ITEM_VALUE_STR)
            total += render_dyn_str(menu, tex, mbs[i], tab_index, item_idx, x + total, y, remaining,
                                    max_x);
        else
            total += render_def(menu, tex, mbs[i], tab_index, item_idx, x + total, y, max_x);
    }

    /* pad background to fill anchor */
    {
        int pad = max_x - (x + total);
        if (pad > 0 && cnt > 0) {
            const clk_menu_def* last = mbs[cnt - 1];
            int bg_id = 0;
            if (last->type == CLK_MENU_DEF_ITEM_LABEL_STR ||
                last->type == CLK_MENU_DEF_ITEM_VALUE_STR)
                bg_id = act ? last->active_style_id : last->inactive_style_id;
            else if (last->type == CLK_MENU_DEF_STRING)
                bg_id = last->style_id;
            if (bg_id > 0) {
                for (int i = 0; i < pad && x + total + i < tex->tex_w; ++i)
                    clk_texture_write_cell(tex, x + total + i, y, " ", bg_id);
            }
            total += pad;
        }
    }

    return total;
}

/** Draws a single tab's name at (x,y) using its active or inactive style. Returns the name length
 * in cells. */
static int render_tab_str(const clk_menu* menu, clk_texture* tex, const clk_menu_def* def,
                          int tab_index, int x, int y, int max_x) {
    (void)max_x;
    const clk_menu_tab* mtab = clk_tab_list_get_at(menu->tab_list, (size_t)tab_index);
    if (!mtab)
        return 0;
    const char* name = mtab->name;
    int style_id =
        ((int)tab_index == (int)menu->active_tab) ? def->active_style_id : def->inactive_style_id;
    clk_texture_write_string(tex, x, y, name, style_id);
    return clk_term_string_width(name);
}

/** Draws an item's label at (x,y) using its active or inactive style. Returns the label length in
 * cells. */
static int render_item_label_str(const clk_menu* menu, clk_texture* tex, const clk_menu_def* def,
                                 int tab_index, int item_idx, int x, int y, int max_x) {
    (void)max_x;
    const clk_menu_tab* mtab = clk_tab_list_get_at(menu->tab_list, (size_t)tab_index);
    if (!mtab || item_idx < 0 || (size_t)item_idx >= clk_item_list_count(mtab->item_list))
        return 0;
    const clk_menu_item* it = clk_item_list_get_at(mtab->item_list, (size_t)item_idx);
    if (!it)
        return 0;
    int style_id =
        (item_idx == (int)mtab->active_item) ? def->active_style_id : def->inactive_style_id;
    clk_texture_write_string(tex, x, y, it->label, style_id);
    return clk_term_string_width(it->label);
}

/** Draws an item's value (int, bool, or string) formatted to text at (x,y) using
 *  its active or inactive style. Returns the value length in cells. */
static int render_item_value_str(const clk_menu* menu, clk_texture* tex, const clk_menu_def* def,
                                 int tab_index, int item_idx, int x, int y, int max_x) {
    (void)max_x;
    const clk_menu_tab* mtab = clk_tab_list_get_at(menu->tab_list, (size_t)tab_index);
    if (!mtab || item_idx < 0 || (size_t)item_idx >= clk_item_list_count(mtab->item_list))
        return 0;
    const clk_menu_item* it = clk_item_list_get_at(mtab->item_list, (size_t)item_idx);
    if (!it)
        return 0;
    int style_id =
        (item_idx == (int)mtab->active_item) ? def->active_style_id : def->inactive_style_id;
    char buf[CLK_MENU_ITEM_VAL_BUF_SIZE];
    const char* ptr = NULL;
    switch (it->type) {
        case CLK_MENU_TYPE_INT:
            snprintf(buf, sizeof(buf), "%.0f", it->value.num);
            ptr = buf;
            break;
        case CLK_MENU_TYPE_BOOL:
            ptr = it->value.b ? "true" : "false";
            break;
        case CLK_MENU_TYPE_STR:
            ptr = it->value.str;
            break;
        default:
            return 0;
    }
    clk_texture_write_string(tex, x, y, ptr, style_id);
    return clk_term_string_width(ptr);
}

/* ── render_def: dispatch ── */

/** Dispatches a def to its type-specific renderer at (x,y), clipping at max_x.
 *  Handles plain strings, composites, the tab bar, label/value groups, and the
 *  individual tab/label/value leaf strings. Returns the cells drawn. */
static int render_def(const clk_menu* menu, clk_texture* tex, const clk_menu_def* def,
                      int tab_index, int item_idx, int x, int y, int max_x) {
    if (!def || x >= max_x)
        return 0;

    switch (def->type) {
        case CLK_MENU_DEF_STRING:
            return render_string(menu, tex, def, x, y, max_x);
        case CLK_MENU_DEF_COMPOSITE:
            return render_composite(menu, tex, def, tab_index, item_idx, x, y, max_x);
        case CLK_MENU_DEF_TAB:
            return render_tab_special(menu, tex, def, item_idx, x, y, max_x);
        case CLK_MENU_DEF_ITEM_LABEL:
        case CLK_MENU_DEF_ITEM_VALUE:
            return render_item_label_value_special(menu, tex, def, tab_index, item_idx, x, y,
                                                   max_x);
        case CLK_MENU_DEF_TAB_STR:
            return render_tab_str(menu, tex, def, tab_index, x, y, max_x);
        case CLK_MENU_DEF_ITEM_LABEL_STR:
            return render_item_label_str(menu, tex, def, tab_index, item_idx, x, y, max_x);
        case CLK_MENU_DEF_ITEM_VALUE_STR:
            return render_item_value_str(menu, tex, def, tab_index, item_idx, x, y, max_x);
        default:
            return 0;
    }
}

/** Renders a tab/item label or value string clipped to both max_chars and max_x,
 *  resolving the live text and active/inactive style. It writes one cell at a
 *  time so the string can be cut off mid-way. Returns the full untruncated length in cells. */
static int render_dyn_str(const clk_menu* menu, clk_texture* tex, const clk_menu_def* def,
                          int tab_index, int item_idx, int x, int y, int max_chars, int max_x) {
    if (!def || x >= max_x || max_chars <= 0)
        return 0;

    const char* str = NULL;
    int style_id = 0;
    switch (def->type) {
        case CLK_MENU_DEF_ITEM_LABEL_STR: {
            const clk_menu_tab* mtab = clk_tab_list_get_at(menu->tab_list, (size_t)tab_index);
            if (!mtab || item_idx < 0 || (size_t)item_idx >= clk_item_list_count(mtab->item_list))
                return 0;
            const clk_menu_item* it = clk_item_list_get_at(mtab->item_list, (size_t)item_idx);
            if (!it)
                return 0;
            str = it->label;
            style_id = (item_idx == (int)mtab->active_item) ? def->active_style_id
                                                            : def->inactive_style_id;
            break;
        }
        case CLK_MENU_DEF_ITEM_VALUE_STR: {
            const clk_menu_tab* mtab = clk_tab_list_get_at(menu->tab_list, (size_t)tab_index);
            if (!mtab || item_idx < 0 || (size_t)item_idx >= clk_item_list_count(mtab->item_list))
                return 0;
            const clk_menu_item* it = clk_item_list_get_at(mtab->item_list, (size_t)item_idx);
            if (!it)
                return 0;
            style_id = (item_idx == (int)mtab->active_item) ? def->active_style_id
                                                            : def->inactive_style_id;
            static char buf[CLK_MENU_ITEM_VAL_BUF_SIZE];
            switch (it->type) {
                case CLK_MENU_TYPE_INT:
                    snprintf(buf, sizeof(buf), "%.0f", it->value.num);
                    str = buf;
                    break;
                case CLK_MENU_TYPE_BOOL:
                    str = it->value.b ? "true" : "false";
                    break;
                case CLK_MENU_TYPE_STR:
                    str = it->value.str;
                    break;
                default:
                    return 0;
            }
            break;
        }
        default:
            return 0;
    }

    clk_texture_write_string(tex, x, y, str, style_id);
    return clk_term_string_width(str);
}

/** Walks to the rightmost leaf of a def, recursing into the last composite
 *  member, and returns that leaf's style id. */
static int last_leaf_style(const clk_menu_def* def) {
    if (!def)
        return 0;
    switch (def->type) {
        case CLK_MENU_DEF_STRING:
            return def->style_id;
        case CLK_MENU_DEF_COMPOSITE:
            return def->member_cnt ? last_leaf_style(def->members[def->member_cnt - 1]) : 0;
        case CLK_MENU_DEF_TAB_STR:
        case CLK_MENU_DEF_ITEM_LABEL_STR:
        case CLK_MENU_DEF_ITEM_VALUE_STR:
            return def->inactive_style_id;
        default:
            return 0;
    }
}

/** Lays out one row of elements across the texture width for the given tab/item.
 *  Fill elements are stretched to fill*width, with the last fill element capped
 *  to leave room for trailing fixed-width elements; for a tab group the gap up
 *  to the target is padded with the last leaf's background. Non-fill elements
 *  render at their natural width. Returns 1. */
static int clk_render_row(const clk_menu* menu, clk_texture* tex, const clk_menu_row* row, int y,
                          int tab_index, int item_idx) {
    /* pre-scan: last element with fill:1.0 caps its target to leave
     * room for trailing fixed-width elements */
    int last_fill = -1;
    int trailing = 0;
    for (int ei = (int)row->count - 1; ei >= 0; --ei) {
        if (row->elements[ei].fill >= 0.0) {
            last_fill = ei;
            break;
        }
        trailing += measure_def(menu, row->elements[ei].def, tab_index, item_idx);
    }

    int x = 0;
    for (int ei = 0; ei < (int)row->count; ++ei) {
        const clk_menu_row_elem* elem = &row->elements[ei];
        if (!elem->def)
            continue;

        if (elem->fill >= 0.0) {
            int target = (int)(elem->fill * tex->tex_w);
            if (ei == last_fill && trailing > 0) {
                int cap = tex->tex_w - trailing;
                if (target > cap)
                    target = cap;
                if (target < 0)
                    target = 0;
            }
            clk_menu_def_type t = elem->def->type;
            bool is_special = (t == CLK_MENU_DEF_TAB || t == CLK_MENU_DEF_ITEM_LABEL ||
                               t == CLK_MENU_DEF_ITEM_VALUE);

            if (is_special) {
                x += render_def(menu, tex, elem->def, tab_index, item_idx, x, y, target);
                /* fill remaining gap to target with background of the
                 * last rendered leaf cell */
                if (t == CLK_MENU_DEF_TAB && x < target) {
                    bool last_active =
                        ((int)(clk_tab_list_count(menu->tab_list) - 1) == (int)menu->active_tab);
                    clk_menu_def** mbs = last_active ? (clk_menu_def**)elem->def->active_members
                                                     : (clk_menu_def**)elem->def->inactive_members;
                    int cnt = (int)(last_active ? elem->def->active_cnt : elem->def->inactive_cnt);
                    int fill_style_id = cnt > 0 ? last_leaf_style(mbs[cnt - 1]) : 0;
                    for (; x < target; ++x)
                        clk_texture_write_cell(tex, x, y, " ", fill_style_id);
                }
                x = target;
            } else {
                while (x < target) {
                    int w = render_def(menu, tex, elem->def, tab_index, item_idx, x, y, target);
                    if (w <= 0)
                        break;
                    x += w;
                }
                x = target;
            }
        } else {
            x += render_def(menu, tex, elem->def, tab_index, item_idx, x, y, tex->tex_w);
        }
        if (x >= tex->tex_w)
            break;
    }
    return 1;
}

/* ── section renderers ── */

/** Renders every row of a normal or tab-bar section starting at y. Returns the number of rows
 * drawn. */
static int render_normal_or_tab_section(const clk_menu* menu, clk_texture* tex,
                                        const clk_menu_section* sec, int y) {
    int tab_index = (sec->type == CLK_MENU_SEC_TAB_BAR) ? (int)menu->active_tab : -1;
    for (int ri = 0; ri < (int)sec->row_count; ++ri)
        clk_render_row(menu, tex, &sec->rows[ri], y + ri, tab_index, -1);
    return (int)sec->row_count;
}

/** Renders rows [start_row, end_row) of a single list item at the given y offset,
 *  used to draw items that are partially clipped at the top or bottom edge. */
static void render_single_item_list_section(const clk_menu* menu, clk_texture* tex,
                                            const clk_menu_section* sec, int y, int tab_index,
                                            int item_idx, int start_row, int end_row) {
    for (int ri = start_row; ri < end_row; ++ri)
        clk_render_row(menu, tex, &sec->rows[ri], y + (ri - start_row), tab_index, item_idx);
}

/** Renders the scrolling item list within available_rows, fitting as many items as
 *  possible while tracking the active item's on-screen slot (active_item_pos_idx).
 *  align_top decides whether a partial item is clipped at the top or bottom as
 *  the selection scrolls past either edge; scroll offsets the first visible item
 *  and any leftover height is filled with empty item frames. Returns rows used. */
static int render_item_list_section(clk_menu_instance* instance, const clk_menu* menu,
                                    clk_texture* tex, const clk_menu_section* sec, int y,
                                    int available_rows) {
    int item_count = (int)(((size_t)available_rows + sec->row_count - 1) / sec->row_count);

    int remaining_rows = (int)(((size_t)item_count * sec->row_count) % (size_t)available_rows);

    /* True when the active item moved up on screen (its P-index decreased) */
    bool up =
        (instance->active_item_pos_idx - instance->last_active_item_pos_idx > 0) ? false : true;

    /* P-index edge snapping: when the cursor reaches the top or bottom of the
     * visible window, snap the scroll alignment so the active item stays
     * clamped to that edge, leaving a partial item on the trailing side. */
    if (up && instance->last_active_item_pos_idx == 0) {
        instance->align_top = true;
        instance->active_item_pos_idx = 0;
    }
    if (up && instance->last_active_item_pos_idx == 1) {
        instance->align_top = true;
        instance->active_item_pos_idx = 0;
    }
    const clk_menu_tab* act_tab = clk_tab_list_get_at(menu->tab_list, (size_t)menu->active_tab);
    if (up && act_tab && act_tab->active_item == 0) {
        instance->align_top = true;
        instance->active_item_pos_idx = 0;
    }
    if (!up && instance->last_active_item_pos_idx == item_count - 1) {
        instance->align_top = false;
        instance->active_item_pos_idx = item_count - 1;
    }
    if (!up && instance->last_active_item_pos_idx == item_count - 2) {
        instance->align_top = false;
        instance->active_item_pos_idx = item_count - 1;
    }
    if (!act_tab)
        return 0;
    int scroll = act_tab->active_item - instance->active_item_pos_idx;
    if (scroll < 0)
        scroll = 0;

    int tab_index = (int)menu->active_tab;
    int start_y = y;

    for (int idx = 0; idx < item_count; ++idx) {
        int item = scroll + idx;
        if (item >= (int)clk_item_list_count(act_tab->item_list))
            break;
        if (idx == 0 && !instance->align_top) {
            render_single_item_list_section(menu, tex, sec, y, tab_index, item, remaining_rows,
                                            (int)sec->row_count);
            y += (int)sec->row_count - remaining_rows;
        } else if (idx == item_count - 1 && instance->align_top) {
            int rows = (int)sec->row_count - remaining_rows;
            if (rows > 0) {
                render_single_item_list_section(menu, tex, sec, y, tab_index, item, 0, rows);
                y += rows;
            }
        } else {
            render_single_item_list_section(menu, tex, sec, y, tab_index, item, 0,
                                            (int)sec->row_count);
            y += (int)sec->row_count;
        }
    }

    /* fill remaining height with empty items (frames only, no text) */
    while (y - start_y < available_rows) {
        int rows_left = available_rows - (y - start_y);
        int draw = rows_left < (int)sec->row_count ? rows_left : (int)sec->row_count;
        render_single_item_list_section(menu, tex, sec, y, tab_index, -1, 0, draw);
        y += draw;
    }

    return y - start_y;
}

void clk_menu_instance_render(clk_menu_instance* instance) {
    if (!instance || (instance->sprite && instance->sprite->is_hidden))
        return;

    clk_texture_clear_all(&instance->tex);

    int fixed_rows = 0;
    for (int section_index = 0; section_index < (int)instance->theme->section_count;
         ++section_index) {
        const clk_menu_section* sec = &instance->theme->sections[section_index];
        if (sec->type == CLK_MENU_SEC_ITEM_LIST)
            continue;
        fixed_rows += (int)sec->row_count;
    }
    int items_rows = instance->tex.tex_h - fixed_rows;

    int y = 0;
    for (int section_index = 0; section_index < (int)instance->theme->section_count;
         ++section_index) {
        if (y >= instance->tex.tex_h)
            break;
        const clk_menu_section* sec = &instance->theme->sections[section_index];
        switch (sec->type) {
            case CLK_MENU_SEC_NORMAL:
            case CLK_MENU_SEC_TAB_BAR:
                y += render_normal_or_tab_section(instance->menu, &instance->tex, sec, y);
                break;
            case CLK_MENU_SEC_ITEM_LIST:
                y += render_item_list_section(instance, instance->menu, &instance->tex, sec, y,
                                              items_rows);
                break;
            default:
                break;
        }
    }
}
