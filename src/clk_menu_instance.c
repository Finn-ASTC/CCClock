#include "clk_menu_instance.h"

#include <stdlib.h>
#include <string.h>

#include "clk_menu_theme.h"
#include "clk_term.h"

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
    return clk_menu_handle_input(inst->menu, input);
}

/* ================================================================
 *  Render
 * ================================================================ */

static void clk_render_row(const clk_menu* menu, clk_texture* tex, const clk_menu_row* row, int y) {
}

static int render_normal_or_tab_section(const clk_menu* menu, clk_texture* tex,
                                        const clk_menu_section* sec, int y) {
    for (int ri = 0; ri < sec->row_count; ++ri) {
        const clk_menu_row* row = &sec->rows[ri];

        clk_render_row(menu, tex, row, y);
        y++;
    }
    return sec->row_count;
}

static int render_item_list_section(const clk_menu* menu, clk_texture* tex,
                                    const clk_menu_section* sec, int y, int avail_rows) {
    (void)menu;
    (void)tex;
    (void)sec;
    (void)avail_rows;
    return 0;
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
                y += render_item_list_section(inst->menu, &inst->tex, sec, y, items_rows);
                break;
        }
    }
}
