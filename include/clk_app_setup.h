#ifndef CLK_APP_SETUP_H
#define CLK_APP_SETUP_H

#include <stdbool.h>

#include "clk_app_config.h"
#include "clk_ascii_render.h"
#include "clk_menu.h"
#include "clk_menu_theme.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Tab IDs */
#define CLK_TAB_BASIC 0
#define CLK_TAB_ALARM 1
#define CLK_TAB_POMODORO 2

/* Basic tab item IDs */
#define CLK_BASIC_ITEM_TFMT 1
#define CLK_BASIC_ITEM_FONT 2
#define CLK_BASIC_ITEM_THEME 3
#define CLK_BASIC_ITEM_QUIT 4

/** Create a menu and populate all tabs + items from @p cfg.
 *  Returns NULL on allocation failure.  Caller owns the returned
 *  clk_menu* and must free it via clk_menu_destroy. */
clk_menu* clk_app_setup_menu(const clk_app_config* cfg);

/** Create a renderer from the selected font in @p ascii_clock,
 *  set z_order to 0, and add it to the term sprite list.
 *  Returns false on allocation failure. */
bool clk_app_setup_render(clk_ascii_render* render, const clk_cfg_ascii_clock_theme* ascii_clock);

/** Load the theme file selected in @p themes (zero-initialises @p theme
 *  then calls clk_menu_theme_load). */
void clk_app_setup_theme(clk_menu_theme* theme, const clk_cfg_themes* themes);

#ifdef __cplusplus
}
#endif

#endif
