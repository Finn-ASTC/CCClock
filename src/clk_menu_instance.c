#include "clk_menu_instance.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "clk_menu_theme.h"
#include "clk_term.h"

#define CLK_MENU_ITEM_VAL_BUF_SIZE 32

/* ================================================================
 *  Lifecycle
 * ================================================================ */

clk_menu_instance* clk_menu_instance_create(clk_menu* menu, const clk_menu_theme* theme) {
    if (!menu || !theme)
        return NULL;

    clk_menu_instance* inst = malloc(sizeof(clk_menu_instance));
    if (!inst)
        return NULL;
    memset(inst, 0, sizeof(clk_menu_instance));

    inst->active_item_pos_idx = 1;
    inst->last_active_item_pos_idx = 1;
    inst->align_top = true;

    inst->menu = menu;
    inst->theme = theme;

    if (theme->min_width <= 0 || theme->min_height <= 0) {
        free(inst);
        return NULL;
    }

    inst->tex = clk_texture_create(theme->min_width, theme->min_height);
    inst->sprite = clk_sprite_create_with_texture(&inst->tex, 0, 0, 0);
    return inst;
}

void clk_menu_instance_destroy(clk_menu_instance* inst) {
    if (!inst)
        return;
    clk_menu_instance_remove_from_term(inst);
    clk_texture_destroy(&inst->tex);
    clk_sprite_destroy(inst->sprite);
    free(inst);
}

/* ================================================================
 *  Layout & visibility
 * ================================================================ */

void clk_menu_instance_set_position(clk_menu_instance* inst, int x, int y) {
    if (!inst || !inst->sprite)
        return;
    inst->sprite->posx = x;
    inst->sprite->posy = y;
}

void clk_menu_instance_set_size(clk_menu_instance* inst, int w, int h) {
    if (!inst)
        return;
    if (w < inst->theme->min_width)
        w = inst->theme->min_width;
    if (h < inst->theme->min_height)
        h = inst->theme->min_height;
    if (w <= 0 || h <= 0)
        return;
    if (w == inst->tex.tex_w && h == inst->tex.tex_h)
        return;
    clk_texture_destroy(&inst->tex);
    inst->tex = clk_texture_create(w, h);
    if (inst->sprite)
        inst->sprite->tex = &inst->tex;
}

void clk_menu_instance_set_visible(clk_menu_instance* inst, bool v) {
    if (!inst || !inst->sprite)
        return;
    inst->sprite->is_hidden = !v;
}

/* ================================================================
 *  Dynamic rebind
 * ================================================================ */

void clk_menu_instance_change_menu(clk_menu_instance* inst, clk_menu* menu) {
    if (!inst || !menu)
        return;
    inst->menu = menu;
    inst->active_item_pos_idx = 1;
    inst->last_active_item_pos_idx = 1;
    inst->align_top = true;
}

bool clk_menu_instance_change_theme(clk_menu_instance* inst, const char* theme_path) {
    if (!inst || !theme_path)
        return false;
    return clk_menu_theme_reload(theme_path, (clk_menu_theme*)inst->theme);
}

/* ================================================================
 *  Render list
 * ================================================================ */

void clk_menu_instance_add_to_term(clk_menu_instance* inst) {
    if (!inst || !inst->sprite || inst->sprite_added)
        return;
    clk_term_add_sprite(inst->sprite);
    inst->sprite_added = true;
}

void clk_menu_instance_remove_from_term(clk_menu_instance* inst) {
    if (!inst || !inst->sprite || !inst->sprite_added)
        return;
    clk_term_remove_sprite(inst->sprite);
    inst->sprite_added = false;
}

/* ================================================================
 *  Interaction
 * ================================================================ */

clk_menu_event clk_menu_instance_handle_input(clk_menu_instance* inst, clk_menu_input input) {
    clk_menu_event ev = {.type = CLK_MENU_EVENT_NONE};
    if (!inst || (inst->sprite && inst->sprite->is_hidden) ||
        inst->tex.tex_w < inst->theme->min_width || inst->tex.tex_h < inst->theme->min_height)
        return ev;

    if (input == CLK_MENU_INPUT_NEXT_ITEM) {
        inst->last_active_item_pos_idx = inst->active_item_pos_idx;
        inst->active_item_pos_idx++;
    }
    if (input == CLK_MENU_INPUT_PREV_ITEM) {
        inst->last_active_item_pos_idx = inst->active_item_pos_idx;
        inst->active_item_pos_idx--;
    }

    return clk_menu_handle_input(inst->menu, input);
}

/* ================================================================
 *  Render
 * ================================================================ */

static int render_def(const clk_menu* menu, clk_texture* tex, const clk_menu_def* def, int tab_idx,
                      int item_idx, int x, int y, int max_x);
static int render_dyn_str(const clk_menu* menu, clk_texture* tex, const clk_menu_def* def,
                          int tab_idx, int item_idx, int x, int y, int max_chars, int max_x);

/** Counts display cells (Unicode code points) in a UTF-8 string by skipping
 *  continuation bytes. Returns the cell width. */
static int utf8_cell_width(const char* s) {
    int w = 0;
    for (const unsigned char* p = (const unsigned char*)s; *p; ++p)
        if ((*p & 0xC0) != 0x80)
            ++w;
    return w;
}

/** Recursively measures the rendered cell width of a def for the given tab/item.
 *  Strings use their UTF-8 width, composites sum their members, and the tab/item
 *  leaves resolve to the live name, label, or formatted value length. Returns the width in cells.
 */
static int measure_def(const clk_menu* menu, const clk_menu_def* def, int tab_idx, int item_idx) {
    if (!def)
        return 0;
    switch (def->type) {
        case CLK_MENU_DEF_STRING:
            return def->string_val ? utf8_cell_width(def->string_val) : 0;
        case CLK_MENU_DEF_COMPOSITE: {
            int w = 0;
            for (int i = 0; i < def->member_cnt; ++i)
                w += measure_def(menu, def->members[i], tab_idx, item_idx);
            return w;
        }
        case CLK_MENU_DEF_TAB_STR:
            return (tab_idx >= 0 && (size_t)tab_idx < menu->tab_count)
                       ? (int)strlen(menu->tabs[tab_idx]->name)
                       : 0;
        case CLK_MENU_DEF_ITEM_LABEL_STR: {
            if (tab_idx < 0 || item_idx < 0 || (size_t)tab_idx >= menu->tab_count)
                return 0;
            const clk_menu_item* it = menu->tabs[tab_idx]->items[item_idx];
            return it ? (int)strlen(it->label) : 0;
        }
        case CLK_MENU_DEF_ITEM_VALUE_STR: {
            if (tab_idx < 0 || item_idx < 0 || (size_t)tab_idx >= menu->tab_count)
                return 0;
            const clk_menu_item* it = menu->tabs[tab_idx]->items[item_idx];
            if (!it)
                return 0;
            switch (it->type) {
                case CLK_MENU_TYPE_INT: {
                    char b[32];
                    return snprintf(b, sizeof(b), "%.0f", it->value.d);
                }
                case CLK_MENU_TYPE_BOOL:
                    return it->value.b ? 4 : 5;
                case CLK_MENU_TYPE_STR:
                    return (int)strlen(it->value.s);
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
                            int tab_idx, int item_idx, int x, int y, int max_x) {
    int total = 0;
    for (int i = 0; i < def->member_cnt; ++i) {
        if (x + total >= max_x)
            break;
        total += render_def(menu, tex, def->members[i], tab_idx, item_idx, x + total, y, max_x);
    }
    return total;
}

/** Renders the whole tab bar by iterating every tab and drawing its active or
 *  inactive member defs. Each tab is measured first and skipped as a whole if
 *  it would overflow max_x. Returns the total cells drawn. */
static int render_tab_special(const clk_menu* menu, clk_texture* tex, const clk_menu_def* def,
                              int item_idx, int x, int y, int max_x) {
    int total = 0;
    for (size_t ti = 0; ti < menu->tab_count; ++ti) {
        bool act = ((int)ti == (int)menu->active_tab);
        const clk_menu_def** mbs = act ? (const clk_menu_def**)def->active_members
                                       : (const clk_menu_def**)def->inactive_members;
        int cnt = act ? def->active_cnt : def->inactive_cnt;

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
                                           const clk_menu_def* def, int tab_idx, int item_idx,
                                           int x, int y, int max_x) {
    bool act = (item_idx >= 0 && (size_t)item_idx < menu->tabs[tab_idx]->item_count &&
                (int)menu->tabs[tab_idx]->active_item == item_idx);
    const clk_menu_def** mbs = act ? (const clk_menu_def**)def->active_members
                                   : (const clk_menu_def**)def->inactive_members;
    int cnt = act ? def->active_cnt : def->inactive_cnt;

    int fixed = 0;
    for (int i = 0; i < cnt; ++i) {
        clk_menu_def_type t = mbs[i]->type;
        if (t != CLK_MENU_DEF_ITEM_LABEL_STR && t != CLK_MENU_DEF_ITEM_VALUE_STR)
            fixed += measure_def(menu, mbs[i], tab_idx, item_idx);
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
            total += render_dyn_str(menu, tex, mbs[i], tab_idx, item_idx, x + total, y, remaining,
                                    max_x);
        else
            total += render_def(menu, tex, mbs[i], tab_idx, item_idx, x + total, y, max_x);
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
                          int tab_idx, int x, int y, int max_x) {
    (void)max_x;
    if (tab_idx < 0 || (size_t)tab_idx >= menu->tab_count)
        return 0;
    const char* name = menu->tabs[tab_idx]->name;
    int sid =
        ((int)tab_idx == (int)menu->active_tab) ? def->active_style_id : def->inactive_style_id;
    clk_texture_write_string(tex, x, y, name, sid);
    return (int)strlen(name);
}

/** Draws an item's label at (x,y) using its active or inactive style. Returns the label length in
 * cells. */
static int render_item_label_str(const clk_menu* menu, clk_texture* tex, const clk_menu_def* def,
                                 int tab_idx, int item_idx, int x, int y, int max_x) {
    (void)max_x;
    if (tab_idx < 0 || item_idx < 0 || (size_t)tab_idx >= menu->tab_count)
        return 0;
    const clk_menu_item* it = menu->tabs[tab_idx]->items[item_idx];
    if (!it)
        return 0;
    int sid = (item_idx == (int)menu->tabs[tab_idx]->active_item) ? def->active_style_id
                                                                  : def->inactive_style_id;
    clk_texture_write_string(tex, x, y, it->label, sid);
    return (int)strlen(it->label);
}

/** Draws an item's value (int, bool, or string) formatted to text at (x,y) using
 *  its active or inactive style. Returns the value length in cells. */
static int render_item_value_str(const clk_menu* menu, clk_texture* tex, const clk_menu_def* def,
                                 int tab_idx, int item_idx, int x, int y, int max_x) {
    (void)max_x;
    if (tab_idx < 0 || item_idx < 0 || (size_t)tab_idx >= menu->tab_count)
        return 0;
    const clk_menu_item* it = menu->tabs[tab_idx]->items[item_idx];
    if (!it)
        return 0;
    int sid = (item_idx == (int)menu->tabs[tab_idx]->active_item) ? def->active_style_id
                                                                  : def->inactive_style_id;
    char buf[CLK_MENU_ITEM_VAL_BUF_SIZE];
    const char* ptr = NULL;
    switch (it->type) {
        case CLK_MENU_TYPE_INT:
            snprintf(buf, sizeof(buf), "%.0f", it->value.d);
            ptr = buf;
            break;
        case CLK_MENU_TYPE_BOOL:
            ptr = it->value.b ? "true" : "false";
            break;
        case CLK_MENU_TYPE_STR:
            ptr = it->value.s;
            break;
        default:
            return 0;
    }
    clk_texture_write_string(tex, x, y, ptr, sid);
    return (int)strlen(ptr);
}

/* ── render_def: dispatch ── */

/** Dispatches a def to its type-specific renderer at (x,y), clipping at max_x.
 *  Handles plain strings, composites, the tab bar, label/value groups, and the
 *  individual tab/label/value leaf strings. Returns the cells drawn. */
static int render_def(const clk_menu* menu, clk_texture* tex, const clk_menu_def* def, int tab_idx,
                      int item_idx, int x, int y, int max_x) {
    if (!def || x >= max_x)
        return 0;

    switch (def->type) {
        case CLK_MENU_DEF_STRING:
            return render_string(menu, tex, def, x, y, max_x);
        case CLK_MENU_DEF_COMPOSITE:
            return render_composite(menu, tex, def, tab_idx, item_idx, x, y, max_x);
        case CLK_MENU_DEF_TAB:
            return render_tab_special(menu, tex, def, item_idx, x, y, max_x);
        case CLK_MENU_DEF_ITEM_LABEL:
        case CLK_MENU_DEF_ITEM_VALUE:
            return render_item_label_value_special(menu, tex, def, tab_idx, item_idx, x, y, max_x);
        case CLK_MENU_DEF_TAB_STR:
            return render_tab_str(menu, tex, def, tab_idx, x, y, max_x);
        case CLK_MENU_DEF_ITEM_LABEL_STR:
            return render_item_label_str(menu, tex, def, tab_idx, item_idx, x, y, max_x);
        case CLK_MENU_DEF_ITEM_VALUE_STR:
            return render_item_value_str(menu, tex, def, tab_idx, item_idx, x, y, max_x);
        default:
            return 0;
    }
}

/** Renders a tab/item label or value string clipped to both max_chars and max_x,
 *  resolving the live text and active/inactive style. It writes one cell at a
 *  time so the string can be cut off mid-way. Returns the full untruncated length in cells. */
static int render_dyn_str(const clk_menu* menu, clk_texture* tex, const clk_menu_def* def,
                          int tab_idx, int item_idx, int x, int y, int max_chars, int max_x) {
    if (!def || x >= max_x || max_chars <= 0)
        return 0;

    const char* str = NULL;
    int sid = 0;
    switch (def->type) {
        case CLK_MENU_DEF_ITEM_LABEL_STR: {
            if (tab_idx < 0 || item_idx < 0 || (size_t)tab_idx >= menu->tab_count)
                return 0;
            const clk_menu_item* it = menu->tabs[tab_idx]->items[item_idx];
            if (!it)
                return 0;
            str = it->label;
            sid = (item_idx == (int)menu->tabs[tab_idx]->active_item) ? def->active_style_id
                                                                      : def->inactive_style_id;
            break;
        }
        case CLK_MENU_DEF_ITEM_VALUE_STR: {
            if (tab_idx < 0 || item_idx < 0 || (size_t)tab_idx >= menu->tab_count)
                return 0;
            const clk_menu_item* it = menu->tabs[tab_idx]->items[item_idx];
            if (!it)
                return 0;
            sid = (item_idx == (int)menu->tabs[tab_idx]->active_item) ? def->active_style_id
                                                                      : def->inactive_style_id;
            static char buf[CLK_MENU_ITEM_VAL_BUF_SIZE];
            switch (it->type) {
                case CLK_MENU_TYPE_INT:
                    snprintf(buf, sizeof(buf), "%.0f", it->value.d);
                    str = buf;
                    break;
                case CLK_MENU_TYPE_BOOL:
                    str = it->value.b ? "true" : "false";
                    break;
                case CLK_MENU_TYPE_STR:
                    str = it->value.s;
                    break;
                default:
                    return 0;
            }
            break;
        }
        default:
            return 0;
    }

    int len = (int)strlen(str);
    int draw = len < max_chars ? len : max_chars;
    for (int i = 0; i < draw && x + i < max_x; ++i)
        clk_texture_write_cell(tex, x + i, y, (char[]){str[i], '\0'}, sid);
    return len;
}

/* ── clk_render_row ── */

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
                          int tab_idx, int item_idx) {
    /* pre-scan: last element with fill:1.0 caps its target to leave
     * room for trailing fixed-width elements */
    int last_fill = -1;
    int trailing = 0;
    for (int ei = row->count - 1; ei >= 0; --ei) {
        if (row->elems[ei].fill >= 0.0) {
            last_fill = ei;
            break;
        }
        trailing += measure_def(menu, row->elems[ei].def, tab_idx, item_idx);
    }

    int x = 0;
    for (int ei = 0; ei < row->count; ++ei) {
        const clk_menu_row_elem* elem = &row->elems[ei];
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
                int before = x;
                x += render_def(menu, tex, elem->def, tab_idx, item_idx, x, y, target);
                /* fill remaining gap to target with background of the
                 * last rendered leaf cell */
                if (t == CLK_MENU_DEF_TAB && x < target) {
                    bool last_active = ((int)(menu->tab_count - 1) == (int)menu->active_tab);
                    const clk_menu_def** mbs =
                        last_active ? (const clk_menu_def**)elem->def->active_members
                                    : (const clk_menu_def**)elem->def->inactive_members;
                    int cnt = last_active ? elem->def->active_cnt : elem->def->inactive_cnt;
                    int fill_sid = cnt > 0 ? last_leaf_style(mbs[cnt - 1]) : 0;
                    for (; x < target; ++x)
                        clk_texture_write_cell(tex, x, y, " ", fill_sid);
                }
                x = target;
            } else {
                while (x < target) {
                    int w = render_def(menu, tex, elem->def, tab_idx, item_idx, x, y, target);
                    if (w <= 0)
                        break;
                    x += w;
                }
                x = target;
            }
        } else {
            x += render_def(menu, tex, elem->def, tab_idx, item_idx, x, y, tex->tex_w);
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
    int tab_idx = (sec->type == CLK_MENU_SEC_TAB_BAR) ? (int)menu->active_tab : -1;
    for (int ri = 0; ri < sec->row_count; ++ri)
        clk_render_row(menu, tex, &sec->rows[ri], y + ri, tab_idx, -1);
    return sec->row_count;
}

/** Renders rows [start_row, end_row) of a single list item at the given y offset,
 *  used to draw items that are partially clipped at the top or bottom edge. */
static void render_single_item_list_section(const clk_menu* menu, clk_texture* tex,
                                            const clk_menu_section* sec, int y, int tab_idx,
                                            int item_idx, int start_row, int end_row) {
    for (int ri = start_row; ri < end_row; ++ri)
        clk_render_row(menu, tex, &sec->rows[ri], y + (ri - start_row), tab_idx, item_idx);
}

/** Renders the scrolling item list within avail_rows, fitting as many items as
 *  possible while tracking the active item's on-screen slot (active_item_pos_idx).
 *  align_top decides whether a partial item is clipped at the top or bottom as
 *  the selection scrolls past either edge; scroll offsets the first visible item
 *  and any leftover height is filled with empty item frames. Returns rows used. */
static int render_item_list_section(clk_menu_instance* inst, const clk_menu* menu, clk_texture* tex,
                                    const clk_menu_section* sec, int y, int avail_rows) {
    int item_cnt = (avail_rows + sec->row_count - 1) / sec->row_count;
    int item_gap = item_cnt / 4;

    int remaining_rows = (item_cnt * sec->row_count) % avail_rows;

    bool up = (inst->active_item_pos_idx - inst->last_active_item_pos_idx > 0) ? false : true;

    if (up && inst->last_active_item_pos_idx == 0) {
        inst->align_top = true;
        inst->active_item_pos_idx = 0;
    }
    if (up && inst->last_active_item_pos_idx == 1) {
        inst->align_top = true;
        inst->active_item_pos_idx = 0;
    }
    if (up && menu->tabs[menu->active_tab]->active_item == 0) {
        inst->align_top = true;
        inst->active_item_pos_idx = 0;
    }
    if (!up && inst->last_active_item_pos_idx == item_cnt - 1) {
        inst->align_top = false;
        inst->active_item_pos_idx = item_cnt - 1;
    }
    if (!up && inst->last_active_item_pos_idx == item_cnt - 2) {
        inst->align_top = false;
        inst->active_item_pos_idx = item_cnt - 1;
    }
    if (!up &&
        menu->tabs[menu->active_tab]->active_item == menu->tabs[menu->active_tab]->item_count) {
        inst->align_top = false;
        inst->active_item_pos_idx = item_cnt - 1;
    }

    int total = (int)menu->tabs[menu->active_tab]->item_count;

    int scroll = menu->tabs[menu->active_tab]->active_item - inst->active_item_pos_idx;
    if (scroll < 0)
        scroll = 0;

    int tab_idx = (int)menu->active_tab;
    int start_y = y;

    for (int idx = 0; idx < item_cnt; ++idx) {
        int item = scroll + idx;
        if (item >= (int)menu->tabs[tab_idx]->item_count)
            break;
        if (idx == 0 && !inst->align_top) {
            render_single_item_list_section(menu, tex, sec, y, tab_idx, item, remaining_rows,
                                            sec->row_count);
            y += sec->row_count - remaining_rows;
        } else if (idx == item_cnt - 1 && inst->align_top) {
            int rows = sec->row_count - remaining_rows;
            if (rows > 0) {
                render_single_item_list_section(menu, tex, sec, y, tab_idx, item, 0, rows);
                y += rows;
            }
        } else {
            render_single_item_list_section(menu, tex, sec, y, tab_idx, item, 0, sec->row_count);
            y += sec->row_count;
        }
    }

    /* fill remaining height with empty items (frames only, no text) */
    while (y - start_y < avail_rows) {
        int rows_left = avail_rows - (y - start_y);
        int draw = rows_left < sec->row_count ? rows_left : sec->row_count;
        render_single_item_list_section(menu, tex, sec, y, tab_idx, -1, 0, draw);
        y += draw;
    }

    return y - start_y;
}

void clk_menu_instance_render(clk_menu_instance* inst) {
    if (!inst || (inst->sprite && inst->sprite->is_hidden))
        return;

    clk_texture_clear_all(&inst->tex);

    int fixed_rows = 0;
    for (int sec_idx = 0; sec_idx < inst->theme->section_count; ++sec_idx) {
        const clk_menu_section* sec = &inst->theme->sections[sec_idx];
        if (sec->type == CLK_MENU_SEC_ITEM_LIST)
            continue;
        fixed_rows += sec->row_count;
    }
    int items_rows = inst->tex.tex_h - fixed_rows;

    int y = 0;
    for (int sec_idx = 0; sec_idx < inst->theme->section_count; ++sec_idx) {
        if (y >= inst->tex.tex_h)
            break;
        const clk_menu_section* sec = &inst->theme->sections[sec_idx];
        switch (sec->type) {
            case CLK_MENU_SEC_NORMAL:
            case CLK_MENU_SEC_TAB_BAR:
                y += render_normal_or_tab_section(inst->menu, &inst->tex, sec, y);
                break;
            case CLK_MENU_SEC_ITEM_LIST:
                y += render_item_list_section(inst, inst->menu, &inst->tex, sec, y, items_rows);
                break;
        }
    }
}
