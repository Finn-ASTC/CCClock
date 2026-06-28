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

    /* --- init engine --- */
    clk_audio_engine* engine = NULL;
    if (!clk_audio_init(&engine)) {
        printf("  [SKIP] No audio device — tests skipped\n");
        goto test_cleanup;
    }

    TEST("init: playing_count == 0", clk_audio_playing_count() == 0);

    /* --- play_loop --- */
    clk_audio_sound* s = clk_audio_load(engine, "../../assets/audio/u_inx5oo5fv3-alarm-327234.mp3");
    if (!s) {
        printf("  [SKIP] Cannot load test sound — tests skipped\n");
        goto done;
    }

    clk_audio_play_loop(s, 0.5f);
    TEST("play_loop: playing_count == 1", clk_audio_playing_count() == 1);

    /* --- play_times --- */
    clk_audio_stop_all();
    clk_audio_play_times(s, 0.8f, 3);
    TEST("play_times(3): playing_count == 1", clk_audio_playing_count() == 1);

    /* --- stop_all --- */
    clk_audio_stop_all();
    TEST("stop_all: playing_count == 0", clk_audio_playing_count() == 0);

    /* --- multiple additions --- */
    for (int i = 0; i < 5; ++i)
        clk_audio_play_loop(s, 0.5f);
    TEST("5 × play_loop: playing_count == 5", clk_audio_playing_count() == 5);
    clk_audio_stop_all();

    /* --- bad args --- */
    int before = clk_audio_playing_count();
    clk_audio_play_times(NULL, 1.0f, 3);
    TEST("play_times NULL: playing_count unchanged", clk_audio_playing_count() == before);
    clk_audio_play_times(s, 1.0f, 0);
    TEST("play_times count=0: playing_count unchanged", clk_audio_playing_count() == before);
    clk_audio_play_times(s, 1.0f, -1);
    TEST("play_times count<0: playing_count unchanged", clk_audio_playing_count() == before);
    clk_audio_play_loop(NULL, 1.0f);
    TEST("play_loop NULL: playing_count unchanged", clk_audio_playing_count() == before);

    /* --- update: looping entries survive --- */
    clk_audio_play_loop(s, 0.5f);
    clk_audio_update();
    clk_audio_update();
    TEST("update: looping entry still present", clk_audio_playing_count() == 1);
    clk_audio_stop_all();

    /* --- stop_all stopped sound --- */
    clk_audio_play_loop(s, 0.5f);
    clk_audio_stop_all();
    TEST("stop_all stops sound: !is_playing", !clk_audio_is_playing(s));

    clk_audio_destroy(s);

done:
    clk_audio_shutdown(engine);

test_cleanup:
    printf("\n%d/%d passed\n", test_total - test_failed, test_total);
    return test_failed > 0 ? 1 : 0;
}
