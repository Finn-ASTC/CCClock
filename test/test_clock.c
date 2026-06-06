#include <stdio.h>

#include "clk_clock.h"
#include "test_utils.h"

#define TEST_FONT_PATH "../test/test_clock_config.json"

int main(void) {
    if (isatty_fd(fileno(stdout))) {
        g_pass = "  \033[32m[PASS]\033[0m ";
        g_fail = "  \033[31m[FAIL]\033[0m ";
    }

    clk_clock clk;

    /* === clk_clock_create — invalid args === */
    bool ok = clk_clock_create(NULL, "%H:%M:%S", TEST_FONT_PATH);
    TEST("clock_create NULL clock fails", !ok);

    ok = clk_clock_create(&clk, NULL, TEST_FONT_PATH);
    TEST("clock_create NULL format fails", !ok);

    ok = clk_clock_create(&clk, "%H:%M:%S", NULL);
    TEST("clock_create NULL font path fails", !ok);

    ok = clk_clock_create(&clk, "%H:%M:%S", "nonexistent.json");
    TEST("clock_create missing file fails", !ok);

    /* === clk_clock_create — success === */
    ok = clk_clock_create(&clk, "%H:%M:%S", TEST_FONT_PATH);
    TEST_REQUIRE("clock_create succeeds", ok);

    /* font texture size */
    int fw, fh;
    TEST("get_font_texture_size succeeds", clk_clock_get_font_texture_size(&clk, &fw, &fh));
    TEST("font_w == 5", fw == 5);
    TEST("font_h == 5", fh == 5);

    /* sprite position */
    int px, py;
    TEST("get_sprite_pos succeeds", clk_clock_get_sprite_pos(&clk, &px, &py));
    TEST("default pos == 0,0", px == 0 && py == 0);

    clk_clock_set_sprite_pos(&clk, 10, 5); clk_clock_get_sprite_pos(&clk, &px, &py);
    TEST("set_sprite_pos reflects", px == 10 && py == 5);

    /* NULL protection */
    TEST("get_sprite_pos NULL fails", !clk_clock_get_sprite_pos(&clk, NULL, &py));
    TEST("get_font_texture_size NULL fails", !clk_clock_get_font_texture_size(&clk, NULL, &fh));

    /* === clk_clock_update === */
    clk_clock_update(&clk);
    TEST("clock_update doesn't crash", 1);

    /* sprite size */
    int sw, sh;
    TEST("get_sprite_size succeeds", clk_clock_get_clock_size(&clk, &sw, &sh));
    TEST("sprite total_w > 0", sw > 0);
    TEST("sprite total_h == 5", sh == 5);

    /* === change_time_format === */
    /* shorten format */
    TEST("change_time_format succeeds", clk_clock_change_time_format(&clk, "%H:%M"));
    TEST("change_time_format NULL fails", !clk_clock_change_time_format(&clk, NULL));
    clk_clock_update(&clk);
    TEST("update after shorten doesn't crash", 1);

    /* expand format (短→长：验证 sprite 扩容） */
    TEST("change_time_format to HH:MM:SS succeeds", clk_clock_change_time_format(&clk, "%H:%M:%S"));
    clk_clock_update(&clk);
    TEST("update after expand doesn't crash", 1);

    /* === reload_config === */
    TEST("reload_config succeeds", clk_clock_reload_config(&clk));

    /* === change_font_path === */
    TEST("change_font_path succeeds", clk_clock_change_font_path(&clk, TEST_FONT_PATH));
    TEST("change_font_path NULL fails", !clk_clock_change_font_path(&clk, NULL));

    /* === add_to_term === */
    TEST("add_to_term succeeds", clk_clock_add_to_term(&clk));
    TEST("add_to_term NULL fails", !clk_clock_add_to_term(NULL));

    /* === destroy === */
    clk_clock_destroy(&clk);
    TEST("clock_destroy no crash", 1);
    clk_clock_destroy(&clk);
    TEST("clock_destroy double no crash", 1);

    /* re-create after destroy */
    ok = clk_clock_create(&clk, "%H:%M:%S", TEST_FONT_PATH);
    TEST("clock re-create after destroy succeeds", ok);
    clk_clock_destroy(&clk);

test_cleanup:
    printf("\n%d/%d passed\n", test_total - test_failed, test_total);
    return test_failed > 0 ? 1 : 0;
}
