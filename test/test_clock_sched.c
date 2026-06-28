#include <stdio.h>
#include <string.h>

#include "clk_clock.h"
#include "test_utils.h"

int main(void) {
    if (isatty_fd(fileno(stdout))) {
        g_pass = "  \033[32m[PASS]\033[0m ";
        g_fail = "  \033[31m[FAIL]\033[0m ";
    }

    /* ================================================================
     *  Lifecycle
     * ================================================================ */

    clk_clock clock;
    bool ok;

    clk_clock_init(&clock, NULL);
    TEST("init zeroes alarm_count", clock.alarm_count == 0);
    TEST("init zeroes pomodoro_count", clock.pomodoro_count == 0);
    TEST("init stores audio_engine", clock.audio_engine == NULL);

    clk_clock_deinit(&clock);
    TEST("deinit safe", 1);
    clk_clock_deinit(&clock);
    TEST("deinit double safe", 1);

    /* ================================================================
     *  Alarms
     * ================================================================ */

    clk_clock_init(&clock, NULL);

    clk_clock_alarm a;
    memset(&a, 0, sizeof(a));
    strcpy(a.name, "test_alarm");
    a.alarm.hour = 7;
    a.alarm.minute = 30;
    a.repeat_days = CLK_REPEAT_EVERYDAY;
    a.volume = 0.8f;

    /* --- add --- */
    ok = clk_clock_add_alarm(&clock, &a);
    TEST("add_alarm succeeds", ok && clock.alarm_count == 1);
    TEST("add_alarm copies name", strcmp(clock.alarms[0].name, "test_alarm") == 0);
    TEST("add_alarm copies hour", clock.alarms[0].alarm.hour == 7);
    TEST("add_alarm copies volume", clock.alarms[0].volume == 0.8f);

    /* add second */
    strcpy(a.name, "alarm2");
    a.alarm.hour = 8;
    ok = clk_clock_add_alarm(&clock, &a);
    TEST("add_alarm second", ok && clock.alarm_count == 2);
    TEST("add_alarm second name", strcmp(clock.alarms[1].name, "alarm2") == 0);

    /* --- set_enabled --- */
    clk_clock_alarm_set_enabled(&clock, 0, false);
    TEST("alarm_set_enabled false", !clock.alarms[0].alarm.enabled);
    clk_clock_alarm_set_enabled(&clock, 0, true);
    TEST("alarm_set_enabled true", clock.alarms[0].alarm.enabled);

    clk_clock_alarm_set_enabled(&clock, -1, false);
    TEST("alarm_set_enabled OOB safe", clock.alarms[0].alarm.enabled);
    clk_clock_alarm_set_enabled(&clock, 5, false);
    TEST("alarm_set_enabled OOB >count safe", 1);

    /* --- count --- */
    TEST("alarm_count == 2", clk_clock_alarm_count(&clock) == 2);

    /* --- remove --- */
    ok = clk_clock_remove_alarm(&clock, 0);
    TEST("remove_alarm succeeds", ok && clock.alarm_count == 1);
    TEST("remove_alarm shifts", strcmp(clock.alarms[0].name, "alarm2") == 0);

    ok = clk_clock_remove_alarm(&clock, 0);
    TEST("remove_alarm last", ok && clock.alarm_count == 0);

    ok = clk_clock_remove_alarm(&clock, 0);
    TEST("remove_alarm empty fails", !ok);
    ok = clk_clock_remove_alarm(&clock, -1);
    TEST("remove_alarm negative fails", !ok);

    /* --- fill to max --- */
    for (int i = 0; i < CLK_ALARM_MAX; ++i) {
        snprintf(a.name, sizeof(a.name), "a%d", i);
        ok = clk_clock_add_alarm(&clock, &a);
    }
    TEST("add_alarm fills to max", ok && clock.alarm_count == CLK_ALARM_MAX);
    ok = clk_clock_add_alarm(&clock, &a);
    TEST("add_alarm overflow fails", !ok && clock.alarm_count == CLK_ALARM_MAX);

    /* ================================================================
     *  Pomodoro groups
     * ================================================================ */

    clk_clock_init(&clock, NULL);

    clk_clock_pomodoro p;
    memset(&p, 0, sizeof(p));
    strcpy(p.name, "Work");

    /* --- add pomodoro --- */
    ok = clk_clock_add_pomodoro(&clock, &p);
    TEST("add_pomodoro succeeds", ok && clock.pomodoro_count == 1);
    TEST("add_pomodoro copies name", strcmp(clock.pomodoros[0].name, "Work") == 0);

    /* --- add segment --- */
    clk_clock_pomodoro_segment seg;
    memset(&seg, 0, sizeof(seg));
    strcpy(seg.name, "Focus");
    seg.duration_seconds = 1500;

    ok = clk_clock_pomodoro_add_segment(&clock, 0, &seg);
    TEST("add_segment succeeds", ok && clock.pomodoros[0].segment_count == 1);
    TEST("add_segment copies name", strcmp(clock.pomodoros[0].segments[0].name, "Focus") == 0);
    TEST("add_segment copies duration", clock.pomodoros[0].segments[0].duration_seconds == 1500);

    /* add second segment */
    strcpy(seg.name, "Break");
    seg.duration_seconds = 300;
    ok = clk_clock_pomodoro_add_segment(&clock, 0, &seg);
    TEST("add_segment second", ok && clock.pomodoros[0].segment_count == 2);
    TEST("add_segment second name", strcmp(clock.pomodoros[0].segments[1].name, "Break") == 0);

    /* --- add_segment OOB --- */
    ok = clk_clock_pomodoro_add_segment(&clock, -1, &seg);
    TEST("add_segment negative index fails", !ok);
    ok = clk_clock_pomodoro_add_segment(&clock, 5, &seg);
    TEST("add_segment OOB fails", !ok);

    /* --- fill segments to max --- */
    clk_clock_pomodoro* pp = &clock.pomodoros[0];
    for (int i = 2; i < CLK_POMODORO_MAX_SEGMENTS; ++i) {
        snprintf(seg.name, sizeof(seg.name), "s%d", i);
        ok = clk_clock_pomodoro_add_segment(&clock, 0, &seg);
    }
    TEST("add_segment fills to max", ok && pp->segment_count == CLK_POMODORO_MAX_SEGMENTS);
    ok = clk_clock_pomodoro_add_segment(&clock, 0, &seg);
    TEST("add_segment overflow fails", !ok);

    /* --- remove segment --- */
    ok = clk_clock_pomodoro_remove_segment(&clock, 0, (int)pp->segment_count - 1);
    TEST("remove_segment succeeds", ok && pp->segment_count == CLK_POMODORO_MAX_SEGMENTS - 1);

    ok = clk_clock_pomodoro_remove_segment(&clock, 0, -1);
    TEST("remove_segment negative fails", !ok);
    ok = clk_clock_pomodoro_remove_segment(&clock, 0, (int)pp->segment_count);
    TEST("remove_segment OOB fails", !ok);

    /* --- start / pause / resume / stop --- */
    clk_clock_pomodoro_start(&clock, 0);
    TEST("pomodoro_start sets current", pp->current_segment == 0);
    TEST("pomodoro_start enables", pp->enabled);

    clk_clock_pomodoro_pause(&clock, 0);
    TEST("pomodoro_pause sets flag", pp->paused);

    clk_clock_pomodoro_resume(&clock, 0);
    TEST("pomodoro_resume clears flag", !pp->paused);

    clk_clock_pomodoro_stop(&clock, 0);
    TEST("pomodoro_stop resets current", pp->current_segment == 0);
    TEST("pomodoro_stop not running", !pp->enabled);

    /* start/stop OOB */
    clk_clock_pomodoro_start(&clock, -1);
    TEST("pomodoro_start OOB safe", 1);
    clk_clock_pomodoro_stop(&clock, 99);
    TEST("pomodoro_stop OOB safe", 1);

    /* --- count --- */
    TEST("pomodoro_count == 1", clk_clock_pomodoro_count(&clock) == 1);

    /* --- remove pomodoro --- */
    ok = clk_clock_remove_pomodoro(&clock, 0);
    TEST("remove_pomodoro succeeds", ok && clock.pomodoro_count == 0);
    ok = clk_clock_remove_pomodoro(&clock, 0);
    TEST("remove_pomodoro empty fails", !ok);

    /* fill pomodoros to max */
    for (int i = 0; i < CLK_POMODORO_MAX; ++i) {
        snprintf(p.name, sizeof(p.name), "p%d", i);
        ok = clk_clock_add_pomodoro(&clock, &p);
    }
    TEST("add_pomodoro fills to max", ok && clock.pomodoro_count == CLK_POMODORO_MAX);
    ok = clk_clock_add_pomodoro(&clock, &p);
    TEST("add_pomodoro overflow fails", !ok);

    /* ================================================================
     *  translate_format (regression)
     * ================================================================ */

    char translated[128];
    ok = clk_clock_translate_format("hh:MM:ss", translated, sizeof(translated));
    TEST("translate hh:MM:ss → %H:%M:%S", ok && strcmp(translated, "%H:%M:%S") == 0);

    /* ================================================================
     *  format_now (regression)
     * ================================================================ */

    char time_str[64];
    ok = clk_clock_format_now("%H:%M:%S", time_str, sizeof(time_str));
    TEST("format_now succeeds", ok && strlen(time_str) == 8);

    printf("\n%d/%d passed\n", test_total - test_failed, test_total);
    return test_failed > 0 ? 1 : 0;
}
