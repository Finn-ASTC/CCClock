#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "clk_app_config.h"
#include "test_utils.h"

#define CONFIG_PATH "../../test/test_app_config.json"

int main(void) {
    if (isatty_fd(fileno(stdout))) {
        g_pass = "  \033[32m[PASS]\033[0m ";
        g_fail = "  \033[31m[FAIL]\033[0m ";
    }

    /* --- load --- */
    clk_app_config cfg;
    bool ok = clk_app_config_load(&cfg, CONFIG_PATH);
    TEST("load valid -> true", ok);

    ok = clk_app_config_load(&cfg, "nonexistent.json");
    TEST("load missing file -> false", !ok);

    memset(&cfg, 0, sizeof(cfg));
    ok = clk_app_config_load(&cfg, CONFIG_PATH);
    TEST_REQUIRE("load valid for real", ok);

    /* --- fonts --- */
    TEST("fonts.count > 0", cfg.fonts.count > 0);
    TEST("fonts.paths != NULL", cfg.fonts.paths != NULL);
    TEST("fonts.names != NULL", cfg.fonts.names != NULL);
    TEST("fonts.idx restored", cfg.fonts.idx >= 0);
    TEST("fonts[0] = test_clock_config",
         strcmp(cfg.fonts.names[cfg.fonts.idx], "test_clock_config") == 0);

    /* --- themes --- */
    TEST("themes.count > 0", cfg.themes.count > 0);
    TEST("themes.paths != NULL", cfg.themes.paths != NULL);
    TEST("themes.names != NULL", cfg.themes.names != NULL);
    TEST("themes.idx restored", cfg.themes.idx >= 0);
    TEST("themes[0] = menu_theme_config",
         strcmp(cfg.themes.names[cfg.themes.idx], "menu_theme_config") == 0);

    /* --- time_formats --- */
    TEST("tfmt.count == 2", cfg.time_formats.count == 2);
    TEST("tfmt.options != NULL", cfg.time_formats.options != NULL);
    TEST("tfmt.strings != NULL", cfg.time_formats.strings != NULL);
    TEST("tfmt.idx == 1 (hh:MM)", cfg.time_formats.idx == 1);
    TEST("tfmt.current = hh:MM", strcmp(cfg.time_formats.current, "hh:MM") == 0);

    /* --- time_formats switch --- */
    cfg.time_formats.idx = 0;
    clk_cfg_time_formats_switch(&cfg.time_formats);
    TEST("tfmt_switch: current = hh:MM:ss",
         strcmp(cfg.time_formats.current, "hh:MM:ss") == 0);

    /* --- reload: non-matching saved name keeps idx --- */
    {
        int saved_idx = cfg.fonts.idx;
        clk_json_value* fonts_obj = clk_json_object_get(cfg.json, "fonts");
        clk_cfg_fonts_reload(&cfg.fonts, fonts_obj, NULL, 0, 0);
        TEST("fonts reload: idx unchanged", cfg.fonts.idx == saved_idx);
    }

    /* --- deinit --- */
    clk_app_config_deinit(&cfg);
    TEST("deinit: json freed", cfg.json == NULL);
    TEST("deinit: fonts cleared", cfg.fonts.count == 0 && cfg.fonts.paths == NULL);
    TEST("deinit: themes cleared", cfg.themes.count == 0 && cfg.themes.paths == NULL);
    TEST("deinit: time_formats cleared", cfg.time_formats.count == 0 && cfg.time_formats.strings == NULL);
    clk_app_config_deinit(&cfg);
    TEST("deinit double safe", 1);

test_cleanup:

    printf("\n%d/%d passed\n", test_total - test_failed, test_total);
    return test_failed > 0 ? 1 : 0;
}
