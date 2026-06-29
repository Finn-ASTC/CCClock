#include <stdbool.h>
#include <stdio.h>
#include <string.h>

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
    clk_audio_sound* s2 = clk_audio_load(engine, "../../assets/audio/u_inx5oo5fv3-alarm-327234.mp3");
    if (!s || !s2) {
        printf("  [SKIP] Cannot load test sound — tests skipped\n");
        goto done;
    }

    /* --- loop play --- */
    clk_audio_play(s, 0.5f, true, 0);
    TEST("play loop: playing_count == 1", clk_audio_playing_count() == 1);

    /* same sound rejected (already managed) */
    clk_audio_play(s, 0.9f, true, 0);
    TEST("play twice same sound: playing_count == 1", clk_audio_playing_count() == 1);

    /* different sound pointer → separate entry */
    clk_audio_play(s2, 0.5f, true, 0);
    TEST("play different sound: playing_count == 2", clk_audio_playing_count() == 2);

    /* --- stop clears managed --- */
    clk_audio_stop(s);
    clk_audio_stop(s2);
    TEST("stop all: playing_count == 0", clk_audio_playing_count() == 0);

    /* --- countdown play --- */
    clk_audio_play(s, 0.8f, false, 3);
    TEST("play ×3: playing_count == 1", clk_audio_playing_count() == 1);
    clk_audio_stop(s);
    TEST("stop after play_times: playing_count == 0", clk_audio_playing_count() == 0);

    /* --- bad args --- */
    int before = clk_audio_playing_count();
    clk_audio_play(NULL, 1.0f, false, 3);
    TEST("play NULL: unchanged", clk_audio_playing_count() == before);
    clk_audio_play(s, 1.0f, false, 0);
    TEST("play count=0: unchanged", clk_audio_playing_count() == before);
    clk_audio_play(s, 1.0f, false, -1);
    TEST("play count<0: unchanged", clk_audio_playing_count() == before);
    clk_audio_play(NULL, 1.0f, true, 0);
    TEST("play loop NULL: unchanged", clk_audio_playing_count() == before);

    /* --- update: looping entries survive --- */
    clk_audio_play(s, 0.5f, true, 0);
    clk_audio_update();
    clk_audio_update();
    TEST("update: looping entry still present", clk_audio_playing_count() == 1);
    clk_audio_stop(s);

    /* --- stop clears sound --- */
    clk_audio_play(s, 0.5f, true, 0);
    clk_audio_stop(s);
    TEST("stop: !is_playing", !clk_audio_is_playing(s));

    clk_audio_destroy(s);
    clk_audio_destroy(s2);

done:
    clk_audio_shutdown(engine);

test_cleanup:
    printf("\n%d/%d passed\n", test_total - test_failed, test_total);
    return test_failed > 0 ? 1 : 0;
}
