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
    int active_item_pos_idx;
    int last_active_item_pos_idx;
    bool align_top;
} clk_menu_instance;

/* ------------------------------------------------------------------
 *  Lifecycle
 * ------------------------------------------------------------------ */

clk_menu_instance* clk_menu_instance_create(clk_menu* menu, const clk_menu_theme* theme);
void clk_menu_instance_destroy(clk_menu_instance* instance);

/* ------------------------------------------------------------------
 *  Layout & visibility
 * ------------------------------------------------------------------ */

void clk_menu_instance_set_position(clk_menu_instance* instance, int x, int y);
void clk_menu_instance_set_size(clk_menu_instance* instance, int w, int h);
void clk_menu_instance_set_visible(clk_menu_instance* instance, bool visible);

/* ------------------------------------------------------------------
 *  Render list
 * ------------------------------------------------------------------ */

void clk_menu_instance_add_to_term(clk_menu_instance* instance);
void clk_menu_instance_remove_from_term(clk_menu_instance* instance);

/* ------------------------------------------------------------------
 *  Interaction
 * ------------------------------------------------------------------ */

clk_menu_event clk_menu_instance_handle_input(clk_menu_instance* instance, clk_menu_input input);

/* ------------------------------------------------------------------
 *  Dynamic rebind
 * ------------------------------------------------------------------ */

/** Swap the menu data pointer without changing theme or display state. */
void clk_menu_instance_change_menu(clk_menu_instance* instance, clk_menu* menu);

/** Hot-reload the theme from @p path. The instance stays alive,
 *  texture and sprite are retained. Returns false on failure. */
bool clk_menu_instance_change_theme(clk_menu_instance* instance, const char* theme_path);

/* ------------------------------------------------------------------
 *  Render
 * ------------------------------------------------------------------ */

void clk_menu_instance_render(clk_menu_instance* instance);

#ifdef __cplusplus
}
#endif

#endif /* CLK_MENU_INSTANCE_H */
