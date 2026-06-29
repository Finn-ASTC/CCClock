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

    focus_t focus = FOCUS_SOUND1;
    bool loop1 = true, loop2 = true;
    int repeat1 = 3, repeat2 = 3;
    float vol1 = 0.8f, vol2 = 0.8f;
    bool running = true;

    while (running) {
        clk_audio_sound* active = (focus == FOCUS_SOUND1) ? sound1 : sound2;
        bool* active_loop = (focus == FOCUS_SOUND1) ? &loop1 : &loop2;
        int* active_repeat = (focus == FOCUS_SOUND1) ? &repeat1 : &repeat2;
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
                if (*active_loop)
                    clk_audio_play(active, *active_vol, true, 0);
                else
                    clk_audio_play(active, *active_vol, false, *active_repeat);
                break;
            case 's':
            case 'S':
                clk_audio_stop(sound1);
                clk_audio_stop(sound2);
                break;
            case 'l':
            case 'L':
                clk_audio_stop(active);
                *active_loop = !*active_loop;
                break;
            case CLK_KEY_UP:
                (*active_repeat)++;
                break;
            case CLK_KEY_DOWN:
                if (*active_repeat > 1)
                    (*active_repeat)--;
                break;
            case '+':
            case '=':
                *active_vol += 0.1f;
                if (*active_vol > 1.0f)
                    *active_vol = 1.0f;
                break;
            case '-':
                *active_vol -= 0.1f;
                if (*active_vol < 0.0f)
                    *active_vol = 0.0f;
                break;
            case 'q':
            case 'Q':
                running = false;
                break;
        }

        clk_audio_update();

        printf("\033[2J\033[H");

        printf("%s  Sound 1 \u2014 alarm %s\n", focus == FOCUS_SOUND1 ? "\033[33m" : "",
               focus == FOCUS_SOUND1 ? "\033[0m" : "");
        printf("  %s  Loop: %s  Repeat: %d  Vol: ",
               clk_audio_is_playing(sound1) ? "\033[32m\u25b6 PLAYING\033[0m" : "\u23f9 STOPPED",
               loop1 ? "ON" : "OFF", repeat1);
        draw_volume_bar(10, vol1);
        printf("\n\n");

        printf("%s  Sound 2 \u2014 rain %s\n", focus == FOCUS_SOUND2 ? "\033[33m" : "",
               focus == FOCUS_SOUND2 ? "\033[0m" : "");
        printf("  %s  Loop: %s  Repeat: %d  Vol: ",
               clk_audio_is_playing(sound2) ? "\033[32m\u25b6 PLAYING\033[0m" : "\u23f9 STOPPED",
               loop2 ? "ON" : "OFF", repeat2);
        draw_volume_bar(10, vol2);
        printf("\n\n");

        printf("  Controls: playing %d\n", clk_audio_playing_count());
        printf(
            "  [1/2]Focus  [Space]Play  [S]StopAll  [L]Loop  [\u2191\u2193]Repeat  [+/-]Vol  "
            "[Q]Quit\n");

        fflush(stdout);
        clk_term_sleep_ms(16);
    }

    clk_audio_stop(sound1);
    clk_audio_stop(sound2);
    clk_audio_destroy(sound1);
    clk_audio_destroy(sound2);
    clk_audio_shutdown(engine);
    clk_term_close();
    return 0;
}
