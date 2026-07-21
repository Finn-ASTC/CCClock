#include "clk_app_setup.h"

#include <stddef.h>
#include <string.h>

/* ================================================================
 *  Internal helpers
 * ================================================================ */

/** Register the "basic" tab items (time format, font, theme, quit). */
static void register_basic_tab(const clk_app_config* cfg, clk_menu* menu) {
    clk_menu_add_tab(menu, CLK_TAB_BASIC, "basic");

    /* ---- time format ---- */
    clk_menu_add_item_str(menu, CLK_TAB_BASIC, CLK_BASIC_ITEM_TIME_FORMAT, "time format",
                          cfg->ascii_clock.time_formats.index,
                          cfg->ascii_clock.time_formats.options,
                          cfg->ascii_clock.time_formats.count);

    /* ---- font ---- */
    clk_menu_add_item_str(menu, CLK_TAB_BASIC, CLK_BASIC_ITEM_FONT, "font",
                          cfg->ascii_clock.fonts.index, (const char**)cfg->ascii_clock.fonts.names,
                          cfg->ascii_clock.fonts.count);

    /* ---- menu theme ---- */
    clk_menu_add_item_str(menu, CLK_TAB_BASIC, CLK_BASIC_ITEM_THEME, "menu theme",
                          cfg->themes.index, (const char**)cfg->themes.names, cfg->themes.count);

    /* ---- quit ---- */
    clk_menu_add_item_action(menu, CLK_TAB_BASIC, CLK_BASIC_ITEM_QUIT, "quit");
}

/* ================================================================
 *  Menu
 * ================================================================ */

clk_menu* clk_app_setup_menu(const clk_app_config* cfg) {
    clk_menu* menu = clk_menu_create();
    if (!menu)
        return NULL;

    register_basic_tab(cfg, menu);

    /* Alarm tab (placeholder — items to be added later) */
    clk_menu_add_tab(menu, CLK_TAB_ALARM, "alarm");

    /* Pomodoro tab (placeholder — items to be added later) */
    clk_menu_add_tab(menu, CLK_TAB_POMODORO, "pomodoro");

    return menu;
}

/* ================================================================
 *  Render & theme
 * ================================================================ */

bool clk_app_setup_render(clk_ascii_render* render, const clk_cfg_ascii_clock_theme* ascii_clock) {
    if (!render || !ascii_clock)
        return false;
    if (ascii_clock->fonts.index < 0 || ascii_clock->fonts.index >= ascii_clock->fonts.count)
        return false;

    if (!clk_ascii_render_create(render, ascii_clock->fonts.paths[ascii_clock->fonts.index]))
        return false;

    clk_ascii_render_set_z_order(render, 0);
    clk_ascii_render_add_to_term(render);
    return true;
}

bool clk_app_setup_theme(clk_menu_theme* theme, const clk_cfg_themes* themes) {
    if (!theme || !themes)
        return false;
    if (themes->index < 0 || themes->index >= themes->count)
        return false;

    memset(theme, 0, sizeof(*theme));
    return clk_menu_theme_load(themes->paths[themes->index], theme);
}
