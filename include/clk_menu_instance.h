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
 *  Instance — bridges menu data + theme + display state
 * ================================================================ */

typedef struct {
    clk_menu* menu;
    const clk_menu_theme* theme;
    int posx, posy;
    int width, height;
    int min_width, min_height;
    int scroll_offset;
    bool visible;
} clk_menu_instance;

/* ------------------------------------------------------------------
 *  Lifecycle
 * ------------------------------------------------------------------ */

clk_menu_instance* clk_menu_instance_create(clk_menu* menu,
                                            const clk_menu_theme* theme);
void clk_menu_instance_destroy(clk_menu_instance* inst);

/* ------------------------------------------------------------------
 *  Layout
 * ------------------------------------------------------------------ */

void clk_menu_instance_set_position(clk_menu_instance* inst, int x, int y);
void clk_menu_instance_set_size(clk_menu_instance* inst, int w, int h);
void clk_menu_instance_set_visible(clk_menu_instance* inst, bool v);

/* ------------------------------------------------------------------
 *  Interaction
 * ------------------------------------------------------------------ */

clk_menu_event clk_menu_instance_handle_input(clk_menu_instance* inst,
                                              clk_menu_input input);

/* ------------------------------------------------------------------
 *  Render
 * ------------------------------------------------------------------ */

void clk_menu_instance_render_to_texture(const clk_menu_instance* inst,
                                         clk_texture* tex);

#ifdef __cplusplus
}
#endif

#endif  /* CLK_MENU_INSTANCE_H */
