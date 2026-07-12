#include <stdbool.h>
#include <stdio.h>

#include "clk_audio.h"
#include "test_utils.h"

int main(void) {
    if (isatty_fd(fileno(stdout))) {
        g_pass = "  \033[32m[PASS]\033[0m ";
        g_fail = "  \033[31m[FAIL]\033[0m ";
    }

    clk_audio_engine* engine = NULL;
    if (!clk_audio_init(&engine)) {
        printf("  [SKIP] No audio device — tests skipped\n");
        goto test_cleanup;
    }

    TEST("init: playing_count == 0", clk_audio_playing_count() == 0);

    clk_audio_sound* s = clk_audio_load(engine, "../../assets/audio/u_inx5oo5fv3-alarm-327234.mp3");
    clk_audio_sound* s2 =
        clk_audio_load(engine, "../../assets/audio/u_inx5oo5fv3-alarm-327234.mp3");
    if (!s || !s2) {
        printf("  [SKIP] Cannot load test sound — tests skipped\n");
        goto done;
    }

    /* --- loop play --- */
    clk_audio_play_inst* i1 = clk_audio_play(s, 0.5f, true, 0);
    TEST("play loop: returns non-NULL", i1 != NULL);
    TEST("play loop: playing_count == 1", clk_audio_playing_count() == 1);

    /* same template → independent instance */
    clk_audio_play_inst* i1b = clk_audio_play(s, 0.9f, true, 0);
    TEST("play same template again: second instance non-NULL", i1b != NULL);
    TEST("play same template again: playing_count == 2", clk_audio_playing_count() == 2);

    /* different template → separate instance */
    clk_audio_play_inst* i2 = clk_audio_play(s2, 0.5f, true, 0);
    TEST("play different template: playing_count == 3", clk_audio_playing_count() == 3);

    /* --- stop clears managed --- */
    clk_audio_stop(i1);
    clk_audio_stop(i1b);
    clk_audio_stop(i2);
    TEST("stop all: playing_count == 0", clk_audio_playing_count() == 0);

    /* --- countdown play --- */
    {
        clk_audio_play_inst* ic = clk_audio_play(s, 0.8f, false, 3);
        TEST("play ×3: playing_count == 1", clk_audio_playing_count() == 1);
        clk_audio_stop(ic);
        TEST("stop after play_times: playing_count == 0", clk_audio_playing_count() == 0);
    }

    /* --- bad args --- */
    int before = clk_audio_playing_count();
    clk_audio_play_inst* nil = clk_audio_play(NULL, 1.0f, false, 3);
    TEST("play NULL: returns NULL", nil == NULL);
    TEST("play NULL: unchanged", clk_audio_playing_count() == before);
    nil = clk_audio_play(s, 1.0f, false, 0);
    TEST("play count=0: returns NULL", nil == NULL);
    TEST("play count=0: unchanged", clk_audio_playing_count() == before);
    nil = clk_audio_play(s, 1.0f, false, -1);
    TEST("play count<0: returns NULL", nil == NULL);
    TEST("play count<0: unchanged", clk_audio_playing_count() == before);

    /* --- update: looping entries survive --- */
    {
        clk_audio_play_inst* il = clk_audio_play(s, 0.5f, true, 0);
        clk_audio_update();
        clk_audio_update();
        TEST("update: looping entry still present", clk_audio_playing_count() == 1);
        clk_audio_stop(il);
    }

    /* --- stop clears sound --- */
    {
        clk_audio_play_inst* is = clk_audio_play(s, 0.5f, true, 0);
        int before_stop = clk_audio_playing_count();
        clk_audio_stop(is);
        TEST("stop: removed from managed list", clk_audio_playing_count() == before_stop - 1);
    }

    /* --- is_finished on NULL --- */
    TEST("is_finished NULL → true", clk_audio_is_finished(NULL));

    /* --- pause / resume --- */
    {
        clk_audio_play_inst* ip = clk_audio_play(s, 0.5f, true, 0);
        TEST("play: is_playing", clk_audio_is_playing(ip));
        clk_audio_pause(ip);
        TEST("paused: !is_playing", !clk_audio_is_playing(ip));
        clk_audio_resume(ip);
        TEST("resumed: is_playing", clk_audio_is_playing(ip));
        clk_audio_stop(ip);
    }

    /* --- destroy template stops all active instances --- */
    {
        clk_audio_play(s, 0.5f, true, 0);
        clk_audio_play(s, 0.5f, true, 0);
        clk_audio_play(s, 0.5f, true, 0);
        TEST("before destroy: playing_count == 3", clk_audio_playing_count() == 3);
    }

    clk_audio_destroy(s);
    TEST("after destroy s: playing_count == 0", clk_audio_playing_count() == 0);
    s = NULL;

    clk_audio_destroy(s2);
    s2 = NULL;

done:
    clk_audio_shutdown(engine);

test_cleanup:
    printf("\n%d/%d passed\n", test_total - test_failed, test_total);
    return test_failed > 0 ? 1 : 0;
}
