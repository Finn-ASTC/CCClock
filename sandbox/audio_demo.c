#include <stdbool.h>
#include <stdio.h>

#include "clk_audio.h"
#include "clk_key_io.h"
#include "clk_term.h"

#define SOUND1_PATH "sandbox/audio/alarm.mp3"
#define SOUND2_PATH "sandbox/audio/rain.mp3"

typedef enum { FOCUS_SOUND1, FOCUS_SOUND2 } focus_t;

static void draw_volume_bar(int width, float volume) {
    int filled = (int)(volume * width + 0.5f);
    for (int i = 0; i < filled; ++i)
        printf("\u2588");
    for (int i = filled; i < width; ++i)
        printf("\u2591");
}

static const char* state_label(clk_audio_sound* sound) {
    if (!sound)
        return "UNLOADED";
    if (clk_audio_is_playing(sound))
        return "\033[32m\u25b6 PLAYING\033[0m";
    if (clk_audio_is_paused(sound))
        return "\033[33m\u23f8 PAUSED\033[0m";
    return "\u23f9 STOPPED";
}

int main(void) {
    clk_term_init();

    clk_audio_engine* engine = NULL;
    if (!clk_audio_init(&engine)) {
        printf("audio init fail\n");
        clk_term_close();
        return 1;
    }

    clk_audio_sound* sound1 = clk_audio_load(engine, SOUND1_PATH);
    clk_audio_sound* sound2 = clk_audio_load(engine, SOUND2_PATH);
    if (!sound1 || !sound2) {
        printf("load fail\n");
        clk_audio_shutdown(engine);
        clk_term_close();
        return 1;
    }

    clk_audio_sound_set_volume(sound1, 0.8f);
    clk_audio_sound_set_volume(sound2, 0.8f);

    focus_t focus = FOCUS_SOUND1;
    bool loop1 = false, loop2 = false;
    float vol1 = 0.8f, vol2 = 0.8f;
    bool running = true;

    while (running) {
        clk_audio_sound* active = (focus == FOCUS_SOUND1) ? sound1 : sound2;
        bool* active_loop = (focus == FOCUS_SOUND1) ? &loop1 : &loop2;
        float* active_vol = (focus == FOCUS_SOUND1) ? &vol1 : &vol2;

        clk_key_event key_event = clk_get_key_event();

        switch (key_event.key) {
            case '1':
                focus = FOCUS_SOUND1;
                break;
            case '2':
                focus = FOCUS_SOUND2;
                break;
            case ' ':
                if (clk_audio_is_playing(active))
                    clk_audio_pause(active);
                else
                    clk_audio_play(active, *active_loop ? CLK_AUDIO_LOOP_ON : CLK_AUDIO_LOOP_OFF);
                break;
            case 's':
            case 'S':
                clk_audio_stop(active);
                break;
            case 'l':
            case 'L':
                *active_loop = !*active_loop;
                break;
            case '+':
            case '=':
                *active_vol += 0.1f;
                if (*active_vol > 1.0f)
                    *active_vol = 1.0f;
                clk_audio_sound_set_volume(active, *active_vol);
                break;
            case '-':
                *active_vol -= 0.1f;
                if (*active_vol < 0.0f)
                    *active_vol = 0.0f;
                clk_audio_sound_set_volume(active, *active_vol);
                break;
            case 'q':
            case 'Q':
                running = false;
                break;
        }

        printf("\033[2J\033[H");

        /* sound 1 */
        printf("%s  Sound 1 \u2014 alarm %s\n", focus == FOCUS_SOUND1 ? "\033[33m" : "",
               focus == FOCUS_SOUND1 ? "\033[0m" : "");
        printf("  %s  Loop: %s  Vol: ", state_label(sound1), loop1 ? "ON" : "OFF");
        draw_volume_bar(10, vol1);
        printf("\n\n");

        /* sound 2 */
        printf("%s  Sound 2 \u2014 rain %s\n", focus == FOCUS_SOUND2 ? "\033[33m" : "",
               focus == FOCUS_SOUND2 ? "\033[0m" : "");
        printf("  %s  Loop: %s  Vol: ", state_label(sound2), loop2 ? "ON" : "OFF");
        draw_volume_bar(10, vol2);
        printf("\n\n");

        printf("  [1/2]Focus  [Space]Play/Pause  [S]Stop  [L]Loop  [+/-]Vol  [Q]Quit\n");

        fflush(stdout);
        clk_term_sleep_ms(16);
    }

    clk_audio_destroy(sound1);
    clk_audio_destroy(sound2);
    clk_audio_shutdown(engine);
    clk_term_close();
    return 0;
}
