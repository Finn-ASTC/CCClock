#include "clk_menu_instance.h"

#include <stdlib.h>
#include <string.h>

#include "clk_menu.h"
#include "clk_menu_theme.h"
#include "clk_term.h"

/* ================================================================
 *  Lifecycle
 * ================================================================ */

clk_menu_instance* clk_menu_instance_create(clk_menu* menu,
                                            const clk_menu_theme* theme) {
    if (!menu || !theme)
        return NULL;

    clk_menu_instance* inst = malloc(sizeof(clk_menu_instance));
    if (!inst)
        return NULL;
    memset(inst, 0, sizeof(clk_menu_instance));
    inst->menu = menu;
    inst->theme = theme;
    inst->min_width = theme->min_width;
    inst->min_height = theme->min_height;
    return inst;
}

void clk_menu_instance_destroy(clk_menu_instance* inst) {
    free(inst);
}

/* ================================================================
 *  Layout
 * ================================================================ */

void clk_menu_instance_set_position(clk_menu_instance* inst, int x, int y) {
    if (!inst)
        return;
    inst->posx = x;
    inst->posy = y;
}

void clk_menu_instance_set_size(clk_menu_instance* inst, int w, int h) {
    if (!inst)
        return;
    if (w < inst->min_width)  w = inst->min_width;
    if (h < inst->min_height) h = inst->min_height;
    inst->width = w;
    inst->height = h;
}

void clk_menu_instance_set_visible(clk_menu_instance* inst, bool v) {
    if (!inst)
        return;
    inst->visible = v;
}

/* ================================================================
 *  Interaction
 * ================================================================ */

clk_menu_event clk_menu_instance_handle_input(clk_menu_instance* inst,
                                              clk_menu_input input) {
    clk_menu_event ev = {.type = CLK_MENU_EVENT_NONE};

    if (!inst || !inst->visible || inst->width < inst->min_width ||
        inst->height < inst->min_height)
        return ev;

    ev = clk_menu_handle_input(inst->menu, input);

    if (ev.type == CLK_MENU_EVENT_NONE) {
        /* reset scroll on tab switch (detected via input, not event) */
        if (input == CLK_MENU_INPUT_NEXT_TAB)
            inst->scroll_offset = 0;
    }

    return ev;
}

/* ================================================================
 *  Render
 * ================================================================ */

void clk_menu_instance_render_to_texture(const clk_menu_instance* inst,
                                         clk_texture* tex) {
    /* TODO:
     *   if (!inst || !tex || !inst->visible) return;
     *   if (tex->tex_w < inst->min_width || tex->tex_h < inst->min_height) return;
     *   clear texture
     *   for each section in theme->framework.layout:
     *     if NORMAL:    render rows
     *     if TAB_BAR:   expand tab composite for each tab
     *     if ITEM_LIST: repeat template per visible item
     *   fill anchor calculation + clipping
     */
    (void)inst;
    (void)tex;
}
