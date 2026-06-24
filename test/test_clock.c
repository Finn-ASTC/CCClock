#include <stdio.h>
#include <string.h>
#include <time.h>

#include "clk_ascii_render.h"
#include "clk_clock.h"
#include "clk_term.h"
#include "test_utils.h"

#define TEST_FONT_PATH "../test/test_clock_config.json"

int main(void) {
    if (isatty_fd(fileno(stdout))) {
        g_pass = "  \033[32m[PASS]\033[0m ";
        g_fail = "  \033[31m[FAIL]\033[0m ";
    }

    /* ================================================================
     *  clk_clock — pure time layer (no terminal required)
     * ================================================================ */

    /* --- translate_format --- */
    char translated[128];
    bool ok;

    ok = clk_clock_translate_format("hh:MM:ss", translated, sizeof(translated));
    TEST("translate hh:MM:ss → %H:%M:%S", ok && strcmp(translated, "%H:%M:%S") == 0);

    ok = clk_clock_translate_format("yyyy-MM-dd", translated, sizeof(translated));
    TEST("translate yyyy-MM-dd → %Y-%M-%d", ok && strcmp(translated, "%Y-%M-%d") == 0);

    ok = clk_clock_translate_format(NULL, translated, sizeof(translated));
    TEST("translate NULL format fails", !ok);

    ok = clk_clock_translate_format("hh:MM", NULL, 64);
    TEST("translate NULL output fails", !ok);

    ok = clk_clock_translate_format("hh:MM", translated, 0);
    TEST("translate zero size fails", !ok);

    ok = clk_clock_translate_format("hh:MM:ss:dd\n", translated, sizeof(translated));
    TEST("translate with newline", ok);
    TEST("translate passes literal chars", strstr(translated, "\n") != NULL);

    /* --- format_now --- */
    char time_str[64];
    ok = clk_clock_format_now("%H:%M:%S", time_str, sizeof(time_str));
    TEST("format_now succeeds", ok && strlen(time_str) == 8);

    ok = clk_clock_format_now(NULL, time_str, sizeof(time_str));
    TEST("format_now NULL format fails", !ok);

    ok = clk_clock_format_now("%H", NULL, 64);
    TEST("format_now NULL buffer fails", !ok);

    /* ================================================================
     *  Timer
     * ================================================================ */

    /* --- zero-duration fires immediately --- */
    clk_timer timer;
    memset(&timer, 0, sizeof(timer));
    clk_timer_start(&timer, 0);
    TEST("timer start(0) immediately finished", clk_timer_finished(&timer));
    TEST("timer start(0) remaining == 0", clk_timer_remaining(&timer) == 0);

    /* --- normal countdown --- */
    clk_timer_start(&timer, 60);
    TEST("timer start(60) running", timer.running && !timer.paused);
    int64_t rem = clk_timer_remaining(&timer);
    TEST("timer remaining ≤ 60", rem <= 60 && rem > 0);

    /* --- pause / resume --- */
    int64_t before_pause = clk_timer_remaining(&timer);
    clk_timer_pause(&timer);
    TEST("timer paused flag", timer.paused);
    int64_t after_pause = clk_timer_remaining(&timer);
    TEST("timer paused remaining ≤ before", after_pause <= before_pause);
    TEST("timer paused remaining unchanging", after_pause == before_pause);

    clk_timer_resume(&timer);
    TEST("timer resumed flag", !timer.paused);

    /* finish it: restart with very short duration so we can test finish */
    clk_timer_start(&timer, 0);
    TEST("timer finished after restart(0)", clk_timer_finished(&timer));

    /* --- elapsed --- */
    clk_timer_start(&timer, 3600);
    int64_t e0 = clk_timer_elapsed(&timer);
    TEST("timer elapsed ≥ 0 at start", e0 >= 0);
    /* pause accumulates elapsed into consumed */
    clk_timer_pause(&timer);
    int64_t e1 = clk_timer_elapsed(&timer);
    TEST("timer elapsed after pause ≥ before", e1 >= e0);
    clk_timer_resume(&timer);
    /* NULL safety */
    clk_timer_start(NULL, 5);
    clk_timer_pause(NULL);
    clk_timer_resume(NULL);
    TEST("timer NULL ops don't crash", 1);
    TEST("timer remaining NULL → 0", clk_timer_remaining(NULL) == 0);
    TEST("timer finished NULL → false", !clk_timer_finished(NULL));

    /* ================================================================
     *  Alarm
     * ================================================================ */

    clk_alarm alarm;
    memset(&alarm, 0, sizeof(alarm));

    /* --- fire: set alarm for current wall-clock time --- */
    {
        time_t now = time(NULL);
        struct tm time_info;
#if defined(_WIN32) || defined(_WIN64)
        localtime_s(&time_info, &now);
#else
        localtime_r(&now, &time_info);
#endif
        clk_alarm_set(&alarm, time_info.tm_hour, time_info.tm_min, time_info.tm_sec);
    }
    bool fired = clk_alarm_check(&alarm);
    TEST("alarm fires at current time", fired && alarm.triggered);

    /* --- one-shot: second check returns false --- */
    TEST("alarm one-shot (second check)", !clk_alarm_check(&alarm));

    /* --- rearm --- */
    clk_alarm_rearm(&alarm);
    TEST("alarm rearm clears triggered", !alarm.triggered && alarm.enabled);

    /* --- far future does not fire --- */
    clk_alarm_set(&alarm, 3, 0, 0);
    TEST("alarm distant time does not fire", !clk_alarm_check(&alarm));

    /* --- disable / enable --- */
    clk_alarm_disable(&alarm);
    TEST("alarm disabled flag", !alarm.enabled);
    TEST("alarm disabled check → false", !clk_alarm_check(&alarm));
    clk_alarm_enable(&alarm);
    TEST("alarm re-enabled flag", alarm.enabled);

    /* --- NULL safety --- */
    clk_alarm_set(NULL, 12, 0, 0);
    clk_alarm_check(NULL);
    TEST("alarm NULL ops don't crash", 1);

    /* ================================================================
     *  clk_ascii_render — font rendering layer (needs term)
     * ================================================================ */

    if (!clk_term_init()) {
        printf("  [SKIP] No terminal — render tests skipped\n");
        goto test_cleanup;
    }

    clk_ascii_render render;

    /* --- create --- */
    ok = clk_ascii_render_create(NULL, TEST_FONT_PATH);
    TEST("render_create NULL render fails", !ok);

    ok = clk_ascii_render_create(&render, NULL);
    TEST("render_create NULL path fails", !ok);

    ok = clk_ascii_render_create(&render, "nonexistent.json");
    TEST("render_create missing file fails", !ok);

    ok = clk_ascii_render_create(&render, TEST_FONT_PATH);
    TEST_REQUIRE("render_create succeeds", ok);

    /* --- glyph size --- */
    int gw, gh;
    TEST("get_glyph_size succeeds", clk_ascii_render_get_glyph_size(&render, &gw, &gh));
    TEST("glyph_w == 5", gw == 5);
    TEST("glyph_h == 5", gh == 5);

    /* --- position --- */
    int px, py;
    TEST("get_pos succeeds", clk_ascii_render_get_pos(&render, &px, &py));
    TEST("default pos == 0,0", px == 0 && py == 0);

    clk_ascii_render_set_pos(&render, 10, 5);
    clk_ascii_render_get_pos(&render, &px, &py);
    TEST("set_pos reflects", px == 10 && py == 5);

    TEST("get_pos NULL fails", !clk_ascii_render_get_pos(&render, NULL, &py));
    TEST("get_glyph_size NULL fails", !clk_ascii_render_get_glyph_size(&render, NULL, &gh));

    /* --- update: single character --- */
    clk_ascii_render_update(&render, "1");
    TEST("update single char: sprite_count == 1", render.sprite_count == 1);
    TEST("update single char: sprite tex assigned", render.sprites[0]->tex != NULL);

    /* --- update: two different characters get different glyphs --- */
    clk_ascii_render_update(&render, "12");
    TEST("update '12': sprite_count == 2", render.sprite_count == 2);
    TEST("update '12': sprites have different textures",
         render.sprites[0]->tex != NULL && render.sprites[1]->tex != NULL &&
             render.sprites[0]->tex != render.sprites[1]->tex);

    /* --- update: longer than previous (sprite count shrinks then grows) --- */
    clk_ascii_render_update(&render, "12:34:56");
    TEST("update long string: sprite_count == 8", render.sprite_count == 8);

    /* --- update: shorter --- */
    clk_ascii_render_update(&render, "12:34");
    TEST("update short string: doesn't crash", 1);

    /* --- update: empty string --- */
    clk_ascii_render_update(&render, "");
    TEST("update empty string: doesn't crash", 1);

    /* --- update: NULL --- */
    clk_ascii_render_update(&render, NULL);
    TEST("update NULL: doesn't crash", 1);

    /* --- get_size --- */
    int sw, sh;
    TEST("get_size succeeds", clk_ascii_render_get_size(&render, "hh:MM:ss", &sw, &sh));
    TEST("get_size: total_w > 0", sw > 0);
    TEST("get_size: total_h == 5 (single row)", sh == 5);

    /* --- get_size with multi-line --- */
    int sw2, sh2;
    TEST("get_size multi-line succeeds", clk_ascii_render_get_size(&render, "12\n34", &sw2, &sh2));
    TEST("get_size multi-line: height > glyph_h", sh2 > gw);

    /* --- reload --- */
    TEST("reload succeeds", clk_ascii_render_reload(&render));

    /* --- change_font --- */
    TEST("change_font succeeds", clk_ascii_render_change_font(&render, TEST_FONT_PATH));
    TEST("change_font NULL fails", !clk_ascii_render_change_font(&render, NULL));

    /* --- add_to_term --- */
    TEST("add_to_term succeeds", clk_ascii_render_add_to_term(&render));
    TEST("add_to_term NULL fails", !clk_ascii_render_add_to_term(NULL));

    /* --- z_order --- */
    clk_ascii_render_set_z_order(&render, 3);
    TEST("set_z_order → get reflects", clk_ascii_render_get_z_order(&render) == 3);

    /* --- destroy --- */
    clk_ascii_render_destroy(&render);
    TEST("render_destroy no crash", 1);
    clk_ascii_render_destroy(&render);
    TEST("render_destroy double no crash", 1);

    /* re-create after destroy */
    ok = clk_ascii_render_create(&render, TEST_FONT_PATH);
    TEST("render re-create after destroy succeeds", ok);
    clk_ascii_render_destroy(&render);

    clk_term_close();

test_cleanup:
    printf("\n%d/%d passed\n", test_total - test_failed, test_total);
    return test_failed > 0 ? 1 : 0;
}
