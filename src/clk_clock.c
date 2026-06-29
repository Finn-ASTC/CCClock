#include "clk_clock.h"

#include <stdbool.h>
#include <string.h>
#include <windows.h>

/* ================================================================
 *  Lifecycle
 * ================================================================ */

void clk_clock_init(clk_clock* clock, clk_audio_engine* audio_engine) {
    if (!clock)
        return;
    memset(clock, 0, sizeof(*clock));
    clock->audio_engine = audio_engine;
}

void clk_clock_deinit(clk_clock* clock) {
    if (!clock)
        return;
    for (int i = 0; i < clock->active_bell_count; ++i)
        if (clock->active_bells[i])
            clk_audio_stop(clock->active_bells[i]);
    clock->active_bell_count = 0;
}

/* ================================================================
 *  Alarms
 * ================================================================ */

bool clk_clock_add_alarm(clk_clock* clock, const clk_clock_alarm* alarm) {
    if (!clock || !alarm || clock->alarm_count >= CLK_ALARM_MAX)
        return false;
    clock->alarms[clock->alarm_count] = *alarm;
    clock->alarm_count++;
    return true;
}

bool clk_clock_remove_alarm(clk_clock* clock, int index) {
    if (!clock || index < 0 || index >= clock->alarm_count)
        return false;
    for (int i = index; i < clock->alarm_count - 1; ++i)
        clock->alarms[i] = clock->alarms[i + 1];
    clock->alarm_count--;
    return true;
}

void clk_clock_alarm_set_enabled(clk_clock* clock, int index, bool enabled) {
    if (!clock || index < 0 || index >= clock->alarm_count)
        return;
    clock->alarms[index].alarm.enabled = enabled;
}

int clk_clock_alarm_count(const clk_clock* clock) {
    return clock ? clock->alarm_count : 0;
}

/* ================================================================
 *  Pomodoro groups
 * ================================================================ */

bool clk_clock_add_pomodoro(clk_clock* clock, const clk_clock_pomodoro* pomodoro) {
    if (!clock || !pomodoro || clock->pomodoro_count >= CLK_POMODORO_MAX)
        return false;
    clock->pomodoros[clock->pomodoro_count] = *pomodoro;
    clock->pomodoro_count++;
    return true;
}

bool clk_clock_remove_pomodoro(clk_clock* clock, int index) {
    if (!clock || index < 0 || index >= clock->pomodoro_count)
        return false;
    for (int i = index; i < clock->pomodoro_count - 1; ++i)
        clock->pomodoros[i] = clock->pomodoros[i + 1];
    clock->pomodoro_count--;
    return true;
}

int clk_clock_pomodoro_count(const clk_clock* clock) {
    return clock ? clock->pomodoro_count : 0;
}

bool clk_clock_pomodoro_add_segment(clk_clock* clock, int pomodoro_index,
                                    const clk_clock_pomodoro_segment* segment) {
    if (!clock || !segment || pomodoro_index < 0 || pomodoro_index >= clock->pomodoro_count)
        return false;
    clk_clock_pomodoro* p = &clock->pomodoros[pomodoro_index];
    if (p->segment_count >= CLK_POMODORO_MAX_SEGMENTS)
        return false;
    p->segments[p->segment_count] = *segment;
    p->segment_count++;
    return true;
}

bool clk_clock_pomodoro_add_segment_at(clk_clock* clock, int pomodoro_index,
                                       const clk_clock_pomodoro_segment* segment,
                                       int segment_index) {
    if (!clock || !segment || pomodoro_index < 0 || pomodoro_index >= clock->pomodoro_count)
        return false;
    clk_clock_pomodoro* p = &clock->pomodoros[pomodoro_index];
    if (segment_index < 0 || segment_index >= CLK_POMODORO_MAX_SEGMENTS ||
        p->segment_count + 1 > CLK_POMODORO_MAX_SEGMENTS)
        return false;
    for (int i = p->segment_count - 1; i >= segment_index; --i)
        p->segments[i + 1] = p->segments[i];
    p->segments[segment_index] = *segment;
    p->segment_count++;
    return true;
}

bool clk_clock_pomodoro_remove_segment(clk_clock* clock, int pomodoro_index, int segment_index) {
    if (!clock || pomodoro_index < 0 || pomodoro_index >= clock->pomodoro_count)
        return false;
    clk_clock_pomodoro* p = &clock->pomodoros[pomodoro_index];
    if (segment_index < 0 || segment_index >= p->segment_count)
        return false;
    for (int i = segment_index; i < p->segment_count - 1; ++i)
        p->segments[i] = p->segments[i + 1];
    p->segment_count--;
    return true;
}

void clk_clock_pomodoro_start(clk_clock* clock, int index) {
    if (!clock || index < 0 || index >= clock->pomodoro_count)
        return;
    clk_clock_pomodoro* p = &clock->pomodoros[index];
    if (p->segment_count == 0)
        return;
    p->enabled = true;
    p->paused = false;
    p->current_segment = 0;
    clk_timer_start(&p->timer, p->segments[0].duration_seconds);
}

void clk_clock_pomodoro_pause(clk_clock* clock, int index) {
    if (!clock || index < 0 || index >= clock->pomodoro_count)
        return;
    clk_clock_pomodoro* p = &clock->pomodoros[index];
    if (!p->enabled || p->paused)
        return;
    clk_timer_pause(&p->timer);
    p->paused = true;
}

void clk_clock_pomodoro_resume(clk_clock* clock, int index) {
    if (!clock || index < 0 || index >= clock->pomodoro_count)
        return;
    clk_clock_pomodoro* p = &clock->pomodoros[index];
    if (!p->paused)
        return;
    clk_timer_resume(&p->timer);
    p->paused = false;
}

void clk_clock_pomodoro_stop(clk_clock* clock, int index) {
    if (!clock || index < 0 || index >= clock->pomodoro_count)
        return;
    clk_clock_pomodoro* p = &clock->pomodoros[index];
    p->enabled = false;
    p->paused = false;
    clk_timer_pause(&p->timer);
    p->current_segment = 0;
}

void clk_clock_pomodoro_set_enabled(clk_clock* clock, int index, bool enabled) {
    if (!clock || index < 0 || index >= clock->pomodoro_count)
        return;
    clock->pomodoros[index].enabled = enabled;
}

/* ================================================================
 *  Active bells
 * ================================================================ */

void clk_clock_stop_bell(clk_clock* clock) {
    if (!clock || clock->active_bell_count == 0)
        return;
    int last = clock->active_bell_count - 1;
    if (clock->active_bells[last])
        clk_audio_stop(clock->active_bells[last]);
    clock->active_bells[last] = NULL;
    clock->active_bell_count--;
}

void clk_clock_stop_all_bells(clk_clock* clock) {
    if (!clock)
        return;
    for (int i = 0; i < clock->active_bell_count; ++i) {
        if (clock->active_bells[i])
            clk_audio_stop(clock->active_bells[i]);
        clock->active_bells[i] = NULL;
    }
    clock->active_bell_count = 0;
}

int clk_clock_bell_count(const clk_clock* clock) {
    return clock ? clock->active_bell_count : 0;
}

/* ================================================================
 *  Per-frame update
 * ================================================================ */

static void add_bell(clk_clock* clock, clk_audio_sound* sound) {
    for (int i = 0; i < clock->active_bell_count; ++i)
        if (clock->active_bells[i] == sound)
            return;
    if (clock->active_bell_count <
        (int)(sizeof(clock->active_bells) / sizeof(clock->active_bells[0])))
        clock->active_bells[clock->active_bell_count++] = sound;
}

void clk_clock_update(clk_clock* clock) {
    if (!clock)
        return;

    struct tm ti;
    if (!clk_time_localtime(&ti))
        return;

    for (int i = 0; i < clock->alarm_count; ++i) {
        clk_clock_alarm* a = &clock->alarms[i];
        if (!a->alarm.enabled)
            continue;

        /* repeat_days filter */
        if (a->repeat_days == CLK_REPEAT_TODAY) {
            if (a->today_date != (time_t)ti.tm_mday)
                continue;
        } else {
            int today_wday = ti.tm_wday == 0 ? 7 : ti.tm_wday; /* Mon=1..Sun=7 */
            if (today_wday != a->repeat_days)
                continue;
        }

        if (!clk_alarm_check(&a->alarm))
            continue;

        if (a->sound) {
            clk_audio_play(a->sound, a->volume, a->loop, a->loop ? 0 : a->repeat_count);
            add_bell(clock, a->sound);
        }

        if (a->repeat_days != CLK_REPEAT_TODAY)
            clk_alarm_rearm(&a->alarm);
    }

    for (int i = 0; i < clock->pomodoro_count; ++i) {
        clk_clock_pomodoro* p = &clock->pomodoros[i];
        if (!p->enabled || p->paused || p->segment_count == 0)
            continue;
        if (!clk_timer_finished(&p->timer))
            continue;

        if (p->current_segment >= 0 && p->current_segment < p->segment_count) {
            clk_clock_pomodoro_segment* seg = &p->segments[p->current_segment];
            if (seg->sound) {
                clk_audio_play(seg->sound, seg->volume, seg->loop,
                               seg->loop ? 0 : seg->repeat_count);
                add_bell(clock, seg->sound);
            }
        }
        p->current_segment = (p->current_segment + 1) % p->segment_count;
        clk_timer_start(&p->timer, p->segments[p->current_segment].duration_seconds);
    }
}

/* ================================================================
 *  Time format translation
 * ================================================================ */

bool clk_clock_translate_format(const char* user_format, char* strftime_format,
                                size_t strftime_format_size) {
    if (!user_format || !strftime_format || strftime_format_size == 0)
        return false;

    int i = 0, o = 0;
    int end = (int)strlen(user_format);
    int out_end = (int)strftime_format_size - 1;

    while (i < end && o < out_end) {
        char c = user_format[i];

        switch (c) {
            case 'y': {
                int n = 0;
                while (i + n < end && user_format[i + n] == 'y')
                    n++;
                if (n == 4) {
                    strftime_format[o++] = '%';
                    strftime_format[o++] = 'Y';
                    i += 4;
                } else if (n == 2) {
                    strftime_format[o++] = '%';
                    strftime_format[o++] = 'y';
                    i += 2;
                } else
                    return false;
                continue;
            }
            case 'M':
                if (i + 1 >= end || user_format[i + 1] != 'M')
                    return false;
                strftime_format[o++] = '%';
                strftime_format[o++] = 'M';
                i += 2;
                continue;
            case 'd':
                if (i + 1 >= end || user_format[i + 1] != 'd')
                    return false;
                strftime_format[o++] = '%';
                strftime_format[o++] = 'd';
                i += 2;
                continue;
            case 'h':
                if (i + 1 >= end || user_format[i + 1] != 'h')
                    return false;
                strftime_format[o++] = '%';
                strftime_format[o++] = 'H';
                i += 2;
                continue;
            case 'm':
                if (i + 1 >= end || user_format[i + 1] != 'm')
                    return false;
                strftime_format[o++] = '%';
                strftime_format[o++] = 'm';
                i += 2;
                continue;
            case 's':
                if (i + 1 >= end || user_format[i + 1] != 's')
                    return false;
                strftime_format[o++] = '%';
                strftime_format[o++] = 'S';
                i += 2;
                continue;
            default:
                strftime_format[o++] = user_format[i++];
                break;
        }
    }

    strftime_format[o] = '\0';
    return true;
}

/* ================================================================
 *  Time formatting
 * ================================================================ */

bool clk_clock_format_now(const char* strftime_format, char* buffer, size_t buffer_size) {
    if (!strftime_format || !buffer || buffer_size == 0)
        return false;

    time_t raw_time;
    struct tm time_info;

    if (!clk_time_localtime(&time_info))
        return false;

    return strftime(buffer, buffer_size, strftime_format, &time_info) > 0;
}
