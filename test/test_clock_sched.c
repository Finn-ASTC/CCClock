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
    TEST("init zeroes active_bell_count", clock.active_bell_count == 0);
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

    clk_clock_pomodoro pomodoro;
    memset(&pomodoro, 0, sizeof(pomodoro));
    strcpy(pomodoro.name, "Work");

    /* --- add pomodoro --- */
    ok = clk_clock_add_pomodoro(&clock, &pomodoro);
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

    /* --- add_segment_at --- */
    {
        int before_count = pp->segment_count;
        strcpy(seg.name, "Head");
        ok = clk_clock_pomodoro_add_segment_at(&clock, 0, &seg, 0);
        TEST("add_segment_at head succeeds", ok);
        TEST("add_segment_at head: seg[0].name == Head", strcmp(pp->segments[0].name, "Head") == 0);
        TEST("add_segment_at head: seg_count ++", pp->segment_count == before_count + 1);
    }

    {
        clk_clock_pomodoro_remove_segment(&clock, 0, (int)pp->segment_count - 1);
        strcpy(seg.name, "Tail");
        ok = clk_clock_pomodoro_add_segment_at(&clock, 0, &seg, pp->segment_count);
        TEST("add_segment_at tail succeeds", ok);
        TEST("add_segment_at tail: last == Tail",
             strcmp(pp->segments[pp->segment_count - 1].name, "Tail") == 0);
    }

    ok = clk_clock_pomodoro_add_segment_at(&clock, 0, &seg, 999);
    TEST("add_segment_at OOB fails", !ok);
    ok = clk_clock_pomodoro_add_segment_at(&clock, 0, &seg, -1);
    TEST("add_segment_at negative fails", !ok);

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

    /* --- set_enabled --- */
    clk_clock_pomodoro_set_enabled(&clock, 0, false);
    TEST("set_enabled false", !pp->enabled);
    clk_clock_pomodoro_set_enabled(&clock, 0, true);
    TEST("set_enabled true", pp->enabled);
    clk_clock_pomodoro_set_enabled(&clock, -1, false);
    TEST("set_enabled OOB safe", 1);
    clk_clock_pomodoro_set_enabled(&clock, 99, false);
    TEST("set_enabled OOB >count safe", 1);

    /* --- count --- */
    TEST("pomodoro_count == 1", clk_clock_pomodoro_count(&clock) == 1);

    /* --- remove pomodoro --- */
    ok = clk_clock_remove_pomodoro(&clock, 0);
    TEST("remove_pomodoro succeeds", ok && clock.pomodoro_count == 0);
    ok = clk_clock_remove_pomodoro(&clock, 0);
    TEST("remove_pomodoro empty fails", !ok);

    /* fill pomodoros to max */
    for (int i = 0; i < CLK_POMODORO_MAX; ++i) {
        snprintf(pomodoro.name, sizeof(pomodoro.name), "p%d", i);
        ok = clk_clock_add_pomodoro(&clock, &pomodoro);
    }
    TEST("add_pomodoro fills to max", ok && clock.pomodoro_count == CLK_POMODORO_MAX);
    ok = clk_clock_add_pomodoro(&clock, &pomodoro);
    TEST("add_pomodoro overflow fails", !ok);

    /* ================================================================
     *  Active bells
     * ================================================================ */

    clk_clock_init(&clock, NULL);

    TEST("bell_count zero", clk_clock_bell_count(&clock) == 0);

    clk_clock_stop_bell(&clock);
    TEST("stop_bell empty safe", clk_clock_bell_count(&clock) == 0);

    clk_clock_stop_all_bells(&clock);
    TEST("stop_all empty safe", 1);

    clock.active_bells[0] = NULL;
    clock.active_bell_count = 1;
    clock.active_bells[1] = NULL;
    clock.active_bell_count = 2;
    TEST("bell_count == 2", clk_clock_bell_count(&clock) == 2);

    clk_clock_stop_bell(&clock);
    TEST("stop_bell reduces count", clk_clock_bell_count(&clock) == 1);
    clk_clock_stop_bell(&clock);
    TEST("stop_bell reduces to zero", clk_clock_bell_count(&clock) == 0);

    clock.active_bells[0] = NULL;
    clock.active_bells[1] = NULL;
    clock.active_bells[2] = NULL;
    clock.active_bell_count = 3;
    clk_clock_stop_all_bells(&clock);
    TEST("stop_all clears", clk_clock_bell_count(&clock) == 0);

    /* ================================================================
     *  New APIs: next_alarm_id / next_pomodoro_id
     * ================================================================ */

    clk_clock_init(&clock, NULL);

    TEST("next_alarm_id empty", clk_clock_next_alarm_id(&clock) == 0);

    clk_clock_alarm na;
    memset(&na, 0, sizeof(na));
    na.id = 5;
    clk_clock_add_alarm(&clock, &na);
    na.id = 3;
    clk_clock_add_alarm(&clock, &na);
    TEST("next_alarm_id returns max+1", clk_clock_next_alarm_id(&clock) == 6);

    TEST("next_pomodoro_id empty", clk_clock_next_pomodoro_id(&clock) == 0);

    clk_clock_pomodoro np;
    memset(&np, 0, sizeof(np));
    np.id = 10;
    clk_clock_add_pomodoro(&clock, &np);
    TEST("next_pomodoro_id returns max+1", clk_clock_next_pomodoro_id(&clock) == 11);

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

    /* ================================================================
     *  New APIs: alarm id lookup
     * ================================================================ */

    clk_clock_init(&clock, NULL);

    clk_clock_alarm ra;
    memset(&ra, 0, sizeof(ra));
    ra.id = 42;
    strcpy(ra.name, "find_me");

    clk_clock_add_alarm(&clock, &ra);
    ra.id = 15;
    strcpy(ra.name, "second_one");
    clk_clock_add_alarm(&clock, &ra);

    /* --- find_alarm_by_id --- */
    TEST("find_alarm_by_id found", clk_clock_find_alarm_by_id(&clock, 42) != NULL);
    TEST("find_alarm_by_id second", clk_clock_find_alarm_by_id(&clock, 15) == &clock.alarms[1]);
    TEST("find_alarm_by_id absent", clk_clock_find_alarm_by_id(&clock, 7) == NULL);
    TEST("find_alarm_by_id NULL clock", clk_clock_find_alarm_by_id(NULL, 42) == NULL);

    /* --- find_alarm_index_by_id --- */
    TEST("find_alarm_index_by_id found", clk_clock_find_alarm_index_by_id(&clock, 42) == 0);
    TEST("find_alarm_index_by_id second", clk_clock_find_alarm_index_by_id(&clock, 15) == 1);
    TEST("find_alarm_index_by_id absent", clk_clock_find_alarm_index_by_id(&clock, 99) == -1);
    TEST("find_alarm_index_by_id NULL", clk_clock_find_alarm_index_by_id(NULL, 42) == -1);

    /* --- find_alarm_by_name --- */
    TEST("find_alarm_by_name found", clk_clock_find_alarm_by_name(&clock, "find_me")->id == 42);
    TEST("find_alarm_by_name second", clk_clock_find_alarm_by_name(&clock, "second_one")->id == 15);
    TEST("find_alarm_by_name absent", clk_clock_find_alarm_by_name(&clock, "nobody") == NULL);
    TEST("find_alarm_by_name NULL name", clk_clock_find_alarm_by_name(&clock, NULL) == NULL);
    TEST("find_alarm_by_name NULL clock", clk_clock_find_alarm_by_name(NULL, "x") == NULL);

    /* ================================================================
     *  New APIs: alarm position insert
     * ================================================================ */

    memset(&ra, 0, sizeof(ra));
    ra.id = 1;
    strcpy(ra.name, "head");
    ok = clk_clock_add_alarm_at(&clock, &ra, 0);
    TEST("add_alarm_at head succeeds", ok);
    TEST("add_alarm_at head id==1",
         clock.alarms[0].id == 1 && strcmp(clock.alarms[0].name, "head") == 0);
    TEST("add_alarm_at shifts old[0]",
         clock.alarms[1].id == 42 && strcmp(clock.alarms[1].name, "find_me") == 0);
    TEST("add_alarm_at count++", clock.alarm_count == 3);

    ra.id = 50;
    strcpy(ra.name, "middle");
    ok = clk_clock_add_alarm_at(&clock, &ra, 1);
    TEST("add_alarm_at middle id==50", clock.alarms[1].id == 50);
    TEST("add_alarm_at middle count==4", clock.alarm_count == 4);

    ra.id = 99;
    strcpy(ra.name, "end");
    ok = clk_clock_add_alarm_at(&clock, &ra, clock.alarm_count);
    TEST("add_alarm_at end succeeds", ok);
    TEST("add_alarm_at end id==99", clock.alarms[4].id == 99);

    ok = clk_clock_add_alarm_at(&clock, &ra, -1);
    TEST("add_alarm_at OOB neg", !ok);

    ok = clk_clock_add_alarm_at(&clock, &ra, 999);
    TEST("add_alarm_at OOB hi", !ok);

    ok = clk_clock_add_alarm_at(NULL, &ra, 0);
    TEST("add_alarm_at NULL clock", !ok);

    /* ================================================================
     *  New APIs: alarm remove_by_id
     * ================================================================ */

    ok = clk_clock_remove_alarm_by_id(&clock, 50);
    TEST("remove_alarm_by_id found", ok && clock.alarm_count == 4);
    TEST("remove_alarm_by_id gone", clk_clock_find_alarm_by_id(&clock, 50) == NULL);

    ok = clk_clock_remove_alarm_by_id(&clock, 999);
    TEST("remove_alarm_by_id absent", !ok);

    ok = clk_clock_remove_alarm_by_id(NULL, 1);
    TEST("remove_alarm_by_id NULL clock", !ok);

    /* ================================================================
     *  New APIs: pomodoro id lookup
     * ================================================================ */

    clk_clock_init(&clock, NULL);

    clk_clock_pomodoro pomo;
    memset(&pomo, 0, sizeof(pomo));
    pomo.id = 10;
    strcpy(pomo.name, "pomo_alpha");
    clk_clock_add_pomodoro(&clock, &pomo);

    pomo.id = 20;
    strcpy(pomo.name, "pomo_beta");
    clk_clock_add_pomodoro(&clock, &pomo);

    TEST("find_pomodoro_by_id found", clk_clock_find_pomodoro_by_id(&clock, 10) != NULL);
    TEST("find_pomodoro_by_id second",
         clk_clock_find_pomodoro_by_id(&clock, 20) == &clock.pomodoros[1]);
    TEST("find_pomodoro_by_id absent", clk_clock_find_pomodoro_by_id(&clock, 5) == NULL);
    TEST("find_pomodoro_by_id NULL", clk_clock_find_pomodoro_by_id(NULL, 10) == NULL);

    TEST("find_pomodoro_index_by_id first", clk_clock_find_pomodoro_index_by_id(&clock, 10) == 0);
    TEST("find_pomodoro_index_by_id second", clk_clock_find_pomodoro_index_by_id(&clock, 20) == 1);
    TEST("find_pomodoro_index_by_id absent", clk_clock_find_pomodoro_index_by_id(&clock, 99) == -1);

    TEST("find_pomodoro_by_name found",
         clk_clock_find_pomodoro_by_name(&clock, "pomo_alpha")->id == 10);
    TEST("find_pomodoro_by_name absent", clk_clock_find_pomodoro_by_name(&clock, "nobody") == NULL);
    TEST("find_pomodoro_by_name NULL name", clk_clock_find_pomodoro_by_name(&clock, NULL) == NULL);

    /* ================================================================
     *  New APIs: pomodoro position insert + remove_by_id
     * ================================================================ */

    memset(&pomo, 0, sizeof(pomo));
    pomo.id = 5;
    strcpy(pomo.name, "head_pomo");
    ok = clk_clock_add_pomodoro_at(&clock, &pomo, 0);
    TEST("add_pomodoro_at head", ok && clock.pomodoros[0].id == 5);

    pomo.id = 25;
    strcpy(pomo.name, "tail_pomo");
    ok = clk_clock_add_pomodoro_at(&clock, &pomo, clock.pomodoro_count);
    TEST("add_pomodoro_at tail", ok && clock.pomodoros[3].id == 25 && clock.pomodoro_count == 4);

    ok = clk_clock_add_pomodoro_at(&clock, &pomo, -1);
    TEST("add_pomodoro_at OOB neg", !ok);

    ok = clk_clock_remove_pomodoro_by_id(&clock, 5);
    TEST("remove_pomodoro_by_id found", ok && clock.pomodoro_count == 3);
    ok = clk_clock_remove_pomodoro_by_id(&clock, 99);
    TEST("remove_pomodoro_by_id absent", !ok);
    ok = clk_clock_remove_pomodoro_by_id(NULL, 1);
    TEST("remove_pomodoro_by_id NULL", !ok);

    /* ================================================================
     *  New APIs: segment
     * ================================================================ */

    /* add segments to pomodoro id=10 (pomo_alpha, now at index 1) */
    int alpha_pomo_index = clk_clock_find_pomodoro_index_by_id(&clock, 10);
    if (alpha_pomo_index < 0)
        goto test_cleanup_new;

    clk_clock_pomodoro* alpha = clk_clock_find_pomodoro_by_id(&clock, 10);

    clk_clock_pomodoro_segment nseg;
    memset(&nseg, 0, sizeof(nseg));
    nseg.id = 100;
    strcpy(nseg.name, "work");
    nseg.duration_seconds = 1500;
    clk_clock_pomodoro_add_segment(&clock, alpha_pomo_index, &nseg);

    nseg.id = 200;
    strcpy(nseg.name, "break");
    nseg.duration_seconds = 300;
    clk_clock_pomodoro_add_segment(&clock, alpha_pomo_index, &nseg);

    /* find_segment_by_id */
    TEST("find_segment_by_id found pomodoro+segment",
         clk_clock_pomodoro_find_segment_by_id(&clock, 10, 100) == &alpha->segments[0]);
    TEST("find_segment_by_id second",
         clk_clock_pomodoro_find_segment_by_id(&clock, 10, 200) == &alpha->segments[1]);
    TEST("find_segment_by_id bad segment",
         clk_clock_pomodoro_find_segment_by_id(&clock, 10, 999) == NULL);
    TEST("find_segment_by_id bad pomodoro",
         clk_clock_pomodoro_find_segment_by_id(&clock, 99, 100) == NULL);
    TEST("find_segment_by_id NULL", clk_clock_pomodoro_find_segment_by_id(NULL, 10, 100) == NULL);

    TEST("find_segment_index_by_id first",
         clk_clock_pomodoro_find_segment_index_by_id(&clock, 10, 100) == 0);
    TEST("find_segment_index_by_id second",
         clk_clock_pomodoro_find_segment_index_by_id(&clock, 10, 200) == 1);
    TEST("find_segment_index_by_id absent",
         clk_clock_pomodoro_find_segment_index_by_id(&clock, 10, 999) == -1);
    TEST("find_segment_index_by_id NULL",
         clk_clock_pomodoro_find_segment_index_by_id(NULL, 10, 100) == -1);

    /* clear_segments */
    alpha->enabled = true;
    alpha->paused = true;
    alpha->current_segment = 1;
    alpha->timer.running = true;
    alpha->timer.paused = true;

    clk_clock_pomodoro_clear_segments(&clock, alpha_pomo_index);
    TEST("clear_segments count zero", alpha->segment_count == 0);
    TEST("clear_segments current -1", alpha->current_segment == -1);
    TEST("clear_segments disabled", !alpha->enabled);
    TEST("clear_segments not paused", !alpha->paused);
    TEST("clear_segments timer stopped", !alpha->timer.running && !alpha->timer.paused);

    clk_clock_pomodoro_clear_segments(&clock, -1);
    TEST("clear_segments OOB neg safe", 1);
    clk_clock_pomodoro_clear_segments(&clock, 99);
    TEST("clear_segments OOB hi safe", 1);
    clk_clock_pomodoro_clear_segments(NULL, 0);
    TEST("clear_segments NULL safe", 1);

test_cleanup_new:
    printf("\n%d/%d passed\n", test_total - test_failed, test_total);
    return test_failed > 0 ? 1 : 0;
}
