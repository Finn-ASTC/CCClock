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
    clk_app_config_deinit(&cfg);

    ok = clk_app_config_load(&cfg, "nonexistent.json");
    TEST("load missing file -> false", !ok);

    memset(&cfg, 0, sizeof(cfg));
    ok = clk_app_config_load(&cfg, CONFIG_PATH);
    TEST_REQUIRE("load valid for real", ok);

    /* --- fonts --- */
    TEST("fonts.count > 0", cfg.ascii_clock.fonts.count > 0);
    TEST("fonts.paths != NULL", cfg.ascii_clock.fonts.paths != NULL);
    TEST("fonts.names != NULL", cfg.ascii_clock.fonts.names != NULL);
    TEST("fonts.idx restored", cfg.ascii_clock.fonts.idx >= 0);
    TEST("fonts[0] = test_clock_config",
         strcmp(cfg.ascii_clock.fonts.names[cfg.ascii_clock.fonts.idx], "test_clock_config") == 0);

    /* --- themes --- */
    TEST("themes.count > 0", cfg.themes.count > 0);
    TEST("themes.paths != NULL", cfg.themes.paths != NULL);
    TEST("themes.names != NULL", cfg.themes.names != NULL);
    TEST("themes.idx restored", cfg.themes.idx >= 0);
    TEST("themes[0] = menu_theme_config",
         strcmp(cfg.themes.names[cfg.themes.idx], "menu_theme_config") == 0);

    /* --- time_formats --- */
    TEST("tfmt.count == 2", cfg.ascii_clock.time_formats.count == 2);
    TEST("tfmt.options != NULL", cfg.ascii_clock.time_formats.options != NULL);
    TEST("tfmt.strings != NULL", cfg.ascii_clock.time_formats.strings != NULL);
    TEST("tfmt.idx == 1 (hh:MM)", cfg.ascii_clock.time_formats.idx == 1);
    TEST("tfmt.current = hh:MM", strcmp(cfg.ascii_clock.time_formats.current, "hh:MM") == 0);

    /* --- time_formats switch --- */
    cfg.ascii_clock.time_formats.idx = 0;
    clk_cfg_ascii_clock_theme_switch_time(&cfg.ascii_clock);
    TEST("tfmt_switch: current = hh:MM:ss",
         strcmp(cfg.ascii_clock.time_formats.current, "hh:MM:ss") == 0);

    /* --- clock (alarms + pomodoros) --- */
    TEST("clock.alarms.count == 1", cfg.clock.alarms.count == 1);
    TEST("clock.alarms[0].hour == 8", cfg.clock.alarms.items[0].hour == 8);
    TEST("clock.alarms[0].enabled", cfg.clock.alarms.items[0].enabled);
    TEST("clock.pomodoros.count == 1", cfg.clock.pomodoros.count == 1);
    TEST("clock.pomodoros[0].segment_count == 2", cfg.clock.pomodoros.items[0].segment_count == 2);
    TEST("clock.pomodoros[0].seg[0] minutes→seconds == 1500",
         cfg.clock.pomodoros.items[0].segments[0].duration_seconds == 1500);

    /* --- bgm --- */
    TEST("bgm.count == 1", cfg.bgm.count == 1);
    TEST("bgm[0].volume == 80", cfg.bgm.items[0].volume == 80);
    TEST("bgm[0].enabled", cfg.bgm.items[0].enabled);

    /* --- reload: non-matching saved name keeps idx --- */
    {
        int saved_idx = cfg.ascii_clock.fonts.idx;
        clk_json_value* theme_obj = clk_json_object_get(cfg.json, "ascii_clock_theme");
        clk_json_value* fonts_obj = theme_obj ? clk_json_object_get(theme_obj, "fonts") : NULL;
        clk_cfg_ascii_clock_theme_reload(&cfg.ascii_clock, theme_obj, NULL, 0, 0, 0);
        TEST("fonts reload: idx unchanged", cfg.ascii_clock.fonts.idx == saved_idx);
    }

    /* --- deinit --- */
    clk_app_config_deinit(&cfg);
    TEST("deinit: json freed", cfg.json == NULL);
    TEST("deinit: fonts cleared",
         cfg.ascii_clock.fonts.count == 0 && cfg.ascii_clock.fonts.paths == NULL);
    TEST("deinit: themes cleared", cfg.themes.count == 0 && cfg.themes.paths == NULL);
    TEST("deinit: time_formats cleared",
         cfg.ascii_clock.time_formats.count == 0 && cfg.ascii_clock.time_formats.strings == NULL);
    clk_app_config_deinit(&cfg);
    TEST("deinit double safe", 1);

test_cleanup:

    printf("\n%d/%d passed\n", test_total - test_failed, test_total);
    return test_failed > 0 ? 1 : 0;
}
