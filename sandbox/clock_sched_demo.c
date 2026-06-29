#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "clk_audio.h"
#include "clk_clock.h"
#include "clk_key_io.h"
#include "clk_term.h"

#define SOUND_ALARM "sandbox/audio/alarm.mp3"
#define SOUND_RAIN "sandbox/audio/rain.mp3"

static void set_alarm_time(clk_alarm* alarm, int seconds_from_now) {
    time_t target = time(NULL) + seconds_from_now;
    struct tm tm;
    clk_time_localtime_from(target, &tm);
    clk_alarm_set(alarm, tm.tm_hour, tm.tm_min, tm.tm_sec);
}

int main(void) {
    clk_term_init();

    clk_audio_engine* engine = NULL;
    if (!clk_audio_init(&engine)) {
        printf("audio init fail\n");
        clk_term_close();
        return 1;
    }

    clk_audio_sound* sound_alarm_a = clk_audio_load(engine, SOUND_ALARM);
    clk_audio_sound* sound_alarm_b = clk_audio_load(engine, SOUND_ALARM);
    clk_audio_sound* sound_rain = clk_audio_load(engine, SOUND_RAIN);
    if (!sound_alarm_a || !sound_alarm_b || !sound_rain) {
        printf("load fail\n");
        clk_audio_shutdown(engine);
        clk_term_close();
        return 1;
    }

    /* ── clocks ────────────────────────────────────────────── */
    clk_clock clock;
    clk_clock_init(&clock, engine);

    /* 3 alarms: fire after 8s / 20s / 35s */
    for (int i = 0; i < 3; ++i) {
        int delays[] = {8, 20, 35};
        clk_clock_alarm a;
        memset(&a, 0, sizeof(a));
        snprintf(a.name, sizeof(a.name), "Alarm %d", i + 1);
        set_alarm_time(&a.alarm, delays[i]);
        a.sound = (i == 0) ? sound_alarm_a : (i == 1) ? sound_rain : sound_alarm_b;
        a.volume = 0.8f;
        a.loop = true;
        a.repeat_days = CLK_REPEAT_TODAY;
        {
            struct tm dtm;
            clk_time_localtime(&dtm);
            a.today_date = dtm.tm_mday;
        }
        clk_clock_add_alarm(&clock, &a);
    }

    /* 1 pomodoro: 8s Work → 3s Break → 8s Work */
    clk_clock_pomodoro p;
    memset(&p, 0, sizeof(p));
    strcpy(p.name, "Demo");

    clk_clock_add_pomodoro(&clock, &p);

    clk_clock_pomodoro_segment seg;
    memset(&seg, 0, sizeof(seg));
    seg.sound = sound_alarm_a;
    seg.volume = 0.6f;
    seg.loop = false;
    seg.repeat_count = 2;

    strcpy(seg.name, "Work");
    seg.duration_seconds = 8;
    clk_clock_pomodoro_add_segment(&clock, 0, &seg);
    strcpy(seg.name, "Break");
    seg.duration_seconds = 3;
    clk_clock_pomodoro_add_segment(&clock, 0, &seg);
    strcpy(seg.name, "Work");
    seg.duration_seconds = 8;
    clk_clock_pomodoro_add_segment(&clock, 0, &seg);

    clk_clock_pomodoro_start(&clock, 0);

    bool running = true;

    while (running) {
        clk_clock_update(&clock);
        clk_audio_update();

        clk_key_event ev = clk_get_key_event();
        switch (ev.key) {
            case ' ':
                clk_clock_stop_bell(&clock);
                break;
            case 'e':
            case 'E':
                clk_clock_pomodoro_set_enabled(&clock, 0, !clock.pomodoros[0].enabled);
                break;
            case 'p':
            case 'P':
                if (clock.pomodoros[0].paused)
                    clk_clock_pomodoro_resume(&clock, 0);
                else
                    clk_clock_pomodoro_pause(&clock, 0);
                break;
            case 'q':
            case 'Q':
                running = false;
                break;
        }

        /* ── render ──────────────────────────────────────── */
        printf("\033[2J\033[H");

        printf("  Alarms\n");
        printf("  %s\n", "══════");
        for (int i = 0; i < clock.alarm_count; ++i) {
            clk_clock_alarm* a = &clock.alarms[i];
            bool fired = a->alarm.triggered;
            printf("  %s: ", a->name);
            if (fired)
                printf("\033[32m[%s FIRED!]\033[0m\n", "\u2713");
            else
                printf("\033[33m[waiting]\033[0m\n");
        }

        printf("\n  Pomodoro: %s\n", clock.pomodoros[0].name);
        for (int i = 0; i < clock.pomodoros[0].segment_count; ++i) {
            clk_clock_pomodoro_segment* seg = &clock.pomodoros[0].segments[i];
            printf("    %s%ds %s%s", i == clock.pomodoros[0].current_segment ? "\033[33m" : "",
                   seg->duration_seconds, seg->name,
                   i < clock.pomodoros[0].segment_count - 1 ? " → " : "");
        }
        printf("\033[0m\n");
        printf("  Current: %s [%s %llds]  Enabled: %c  Paused: %c\n",
               clock.pomodoros[0].segments[clock.pomodoros[0].current_segment].name,
               clock.pomodoros[0].paused ? "\u23f8" : "\u25b6",
               (long long)clk_timer_remaining(&clock.pomodoros[0].timer),
               clock.pomodoros[0].enabled ? 'Y' : 'N', clock.pomodoros[0].paused ? 'Y' : 'N');

        printf("\n  Bells: %d  |  Audio managed: %d\n", clk_clock_bell_count(&clock),
               clk_audio_playing_count());
        printf("  [Space]StopBell  [E]TogglePomo  [P]Pause  [Q]Quit\n");

        fflush(stdout);
        clk_term_sleep_ms(100);
    }

    clk_clock_deinit(&clock);
    clk_audio_destroy(sound_alarm_a);
    clk_audio_destroy(sound_alarm_b);
    clk_audio_destroy(sound_rain);
    clk_audio_shutdown(engine);
    clk_term_close();
    return 0;
}
