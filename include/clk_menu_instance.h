#ifndef CLK_MENU_INSTANCE_H
#define CLK_MENU_INSTANCE_H

#include <stdbool.h>

#include "clk_menu.h"
#include "clk_menu_theme.h"
#include "clk_term.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ================================================================
 *  Instance — owns texture + sprite, bridges menu + theme + display
 * ================================================================ */

typedef struct {
    clk_menu* menu;
    const clk_menu_theme* theme;

    clk_texture tex;
    clk_sprite* sprite;
    bool sprite_added;
} clk_menu_instance;

/* ------------------------------------------------------------------
 *  Lifecycle
 * ------------------------------------------------------------------ */

clk_menu_instance* clk_menu_instance_create(clk_menu* menu, const clk_menu_theme* theme);
void clk_menu_instance_destroy(clk_menu_instance* inst);

/* ------------------------------------------------------------------
 *  Layout & visibility
 * ------------------------------------------------------------------ */

void clk_menu_instance_set_position(clk_menu_instance* inst, int x, int y);
void clk_menu_instance_set_size(clk_menu_instance* inst, int w, int h);
void clk_menu_instance_set_visible(clk_menu_instance* inst, bool v);

/* ------------------------------------------------------------------
 *  Render list
 * ------------------------------------------------------------------ */

void clk_menu_instance_add_to_term(clk_menu_instance* inst);
void clk_menu_instance_remove_from_term(clk_menu_instance* inst);

/* ------------------------------------------------------------------
 *  Interaction
 * ------------------------------------------------------------------ */

clk_menu_event clk_menu_instance_handle_input(clk_menu_instance* inst, clk_menu_input input);

/* ------------------------------------------------------------------
 *  Render
 * ------------------------------------------------------------------ */

void clk_menu_instance_render(clk_menu_instance* inst);

#ifdef __cplusplus
}
#endif

#endif /* CLK_MENU_INSTANCE_H */
