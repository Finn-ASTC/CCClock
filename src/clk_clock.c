#include "clk_clock.h"

#include <string.h>

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

bool clk_clock_add_alarm_at(clk_clock* clock, const clk_clock_alarm* alarm, int index) {
    if (!clock || !alarm || clock->alarm_count >= CLK_ALARM_MAX)
        return false;
    if (index < 0 || index > clock->alarm_count)
        return false;
    for (int i = clock->alarm_count; i > index; --i)
        clock->alarms[i] = clock->alarms[i - 1];
    clock->alarms[index] = *alarm;
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

bool clk_clock_remove_alarm_by_id(clk_clock* clock, int id) {
    int index = clk_clock_find_alarm_index_by_id(clock, id);
    return clk_clock_remove_alarm(clock, index);
}

void clk_clock_alarm_set_enabled(clk_clock* clock, int index, bool enabled) {
    if (!clock || index < 0 || index >= clock->alarm_count)
        return;
    clock->alarms[index].alarm.enabled = enabled;
}

int clk_clock_alarm_count(const clk_clock* clock) {
    return clock ? clock->alarm_count : 0;
}

clk_clock_alarm* clk_clock_find_alarm_by_id(clk_clock* clock, int id) {
    if (!clock)
        return NULL;
    for (int i = 0; i < clock->alarm_count; ++i)
        if (clock->alarms[i].id == id)
            return &clock->alarms[i];
    return NULL;
}

int clk_clock_find_alarm_index_by_id(const clk_clock* clock, int id) {
    if (!clock)
        return -1;
    for (int i = 0; i < clock->alarm_count; ++i)
        if (clock->alarms[i].id == id)
            return i;
    return -1;
}

clk_clock_alarm* clk_clock_find_alarm_by_name(clk_clock* clock, const char* name) {
    if (!clock || !name)
        return NULL;
    for (int i = 0; i < clock->alarm_count; ++i)
        if (strcmp(clock->alarms[i].name, name) == 0)
            return &clock->alarms[i];
    return NULL;
}

int clk_clock_next_alarm_id(const clk_clock* clock) {
    int max_id = -1;
    if (!clock)
        return 0;
    for (int i = 0; i < clock->alarm_count; ++i)
        if (clock->alarms[i].id > max_id)
            max_id = clock->alarms[i].id;
    return max_id + 1;
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

bool clk_clock_add_pomodoro_at(clk_clock* clock, const clk_clock_pomodoro* pomodoro, int index) {
    if (!clock || !pomodoro || clock->pomodoro_count >= CLK_POMODORO_MAX)
        return false;
    if (index < 0 || index > clock->pomodoro_count)
        return false;
    for (int i = clock->pomodoro_count; i > index; --i)
        clock->pomodoros[i] = clock->pomodoros[i - 1];
    clock->pomodoros[index] = *pomodoro;
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

bool clk_clock_remove_pomodoro_by_id(clk_clock* clock, int id) {
    int index = clk_clock_find_pomodoro_index_by_id(clock, id);
    return clk_clock_remove_pomodoro(clock, index);
}

int clk_clock_pomodoro_count(const clk_clock* clock) {
    return clock ? clock->pomodoro_count : 0;
}

clk_clock_pomodoro* clk_clock_find_pomodoro_by_id(clk_clock* clock, int id) {
    if (!clock)
        return NULL;
    for (int i = 0; i < clock->pomodoro_count; ++i)
        if (clock->pomodoros[i].id == id)
            return &clock->pomodoros[i];
    return NULL;
}

int clk_clock_find_pomodoro_index_by_id(const clk_clock* clock, int id) {
    if (!clock)
        return -1;
    for (int i = 0; i < clock->pomodoro_count; ++i)
        if (clock->pomodoros[i].id == id)
            return i;
    return -1;
}

clk_clock_pomodoro* clk_clock_find_pomodoro_by_name(clk_clock* clock, const char* name) {
    if (!clock || !name)
        return NULL;
    for (int i = 0; i < clock->pomodoro_count; ++i)
        if (strcmp(clock->pomodoros[i].name, name) == 0)
            return &clock->pomodoros[i];
    return NULL;
}

int clk_clock_next_pomodoro_id(const clk_clock* clock) {
    int max_id = -1;
    if (!clock)
        return 0;
    for (int i = 0; i < clock->pomodoro_count; ++i)
        if (clock->pomodoros[i].id > max_id)
            max_id = clock->pomodoros[i].id;
    return max_id + 1;
}

bool clk_clock_pomodoro_add_segment(clk_clock* clock, int pomodoro_index,
                                    const clk_clock_pomodoro_segment* segment) {
    if (!clock || !segment || pomodoro_index < 0 || pomodoro_index >= clock->pomodoro_count)
        return false;
    clk_clock_pomodoro* pomodoro = &clock->pomodoros[pomodoro_index];
    if (pomodoro->segment_count >= CLK_POMODORO_MAX_SEGMENTS)
        return false;
    pomodoro->segments[pomodoro->segment_count] = *segment;
    pomodoro->segment_count++;
    return true;
}

bool clk_clock_pomodoro_add_segment_at(clk_clock* clock, int pomodoro_index,
                                       const clk_clock_pomodoro_segment* segment,
                                       int segment_index) {
    if (!clock || !segment || pomodoro_index < 0 || pomodoro_index >= clock->pomodoro_count)
        return false;
    clk_clock_pomodoro* pomodoro = &clock->pomodoros[pomodoro_index];
    if (segment_index < 0 || segment_index > pomodoro->segment_count ||
        pomodoro->segment_count >= CLK_POMODORO_MAX_SEGMENTS)
        return false;
    for (int i = pomodoro->segment_count - 1; i >= segment_index; --i)
        pomodoro->segments[i + 1] = pomodoro->segments[i];
    pomodoro->segments[segment_index] = *segment;
    pomodoro->segment_count++;
    return true;
}

bool clk_clock_pomodoro_remove_segment(clk_clock* clock, int pomodoro_index, int segment_index) {
    if (!clock || pomodoro_index < 0 || pomodoro_index >= clock->pomodoro_count)
        return false;
    clk_clock_pomodoro* pomodoro = &clock->pomodoros[pomodoro_index];
    if (segment_index < 0 || segment_index >= pomodoro->segment_count)
        return false;
    for (int i = segment_index; i < pomodoro->segment_count - 1; ++i)
        pomodoro->segments[i] = pomodoro->segments[i + 1];
    pomodoro->segment_count--;
    return true;
}

void clk_clock_pomodoro_clear_segments(clk_clock* clock, int pomodoro_index) {
    if (!clock || pomodoro_index < 0 || pomodoro_index >= clock->pomodoro_count)
        return;
    clk_clock_pomodoro* pomodoro = &clock->pomodoros[pomodoro_index];
    pomodoro->segment_count = 0;
    pomodoro->current_segment = -1;
    pomodoro->enabled = false;
    pomodoro->paused = false;
    pomodoro->timer.running = false;
    pomodoro->timer.paused = false;
}

clk_clock_pomodoro_segment* clk_clock_pomodoro_find_segment_by_id(clk_clock* clock, int pomodoro_id,
                                                                  int segment_id) {
    if (!clock)
        return NULL;
    for (int i = 0; i < clock->pomodoro_count; ++i) {
        if (clock->pomodoros[i].id != pomodoro_id)
            continue;
        for (int j = 0; j < clock->pomodoros[i].segment_count; ++j)
            if (clock->pomodoros[i].segments[j].id == segment_id)
                return &clock->pomodoros[i].segments[j];
    }
    return NULL;
}

void clk_clock_pomodoro_start(clk_clock* clock, int index) {
    if (!clock || index < 0 || index >= clock->pomodoro_count)
        return;
    clk_clock_pomodoro* pomodoro = &clock->pomodoros[index];
    if (pomodoro->segment_count == 0)
        return;
    pomodoro->enabled = true;
    pomodoro->paused = false;
    pomodoro->current_segment = 0;
    clk_timer_start(&pomodoro->timer, pomodoro->segments[0].duration_seconds);
}

void clk_clock_pomodoro_pause(clk_clock* clock, int index) {
    if (!clock || index < 0 || index >= clock->pomodoro_count)
        return;
    clk_clock_pomodoro* pomodoro = &clock->pomodoros[index];
    if (!pomodoro->enabled || pomodoro->paused)
        return;
    clk_timer_pause(&pomodoro->timer);
    pomodoro->paused = true;
}

void clk_clock_pomodoro_resume(clk_clock* clock, int index) {
    if (!clock || index < 0 || index >= clock->pomodoro_count)
        return;
    clk_clock_pomodoro* pomodoro = &clock->pomodoros[index];
    if (!pomodoro->paused)
        return;
    clk_timer_resume(&pomodoro->timer);
    pomodoro->paused = false;
}

void clk_clock_pomodoro_stop(clk_clock* clock, int index) {
    if (!clock || index < 0 || index >= clock->pomodoro_count)
        return;
    clk_clock_pomodoro* pomodoro = &clock->pomodoros[index];
    pomodoro->enabled = false;
    pomodoro->paused = false;
    pomodoro->timer.running = false;
    pomodoro->timer.paused = false;
    pomodoro->current_segment = 0;
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

static void add_bell(clk_clock* clock, clk_audio_play_inst* inst) {
    if (clock->active_bell_count <
        (int)(sizeof(clock->active_bells) / sizeof(clock->active_bells[0])))
        clock->active_bells[clock->active_bell_count++] = inst;
}

void clk_clock_update(clk_clock* clock) {
    if (!clock)
        return;

    struct tm time_info;
    if (!clk_time_localtime(&time_info))
        return;

    for (int i = 0; i < clock->alarm_count; ++i) {
        clk_clock_alarm* alarm_ptr = &clock->alarms[i];
        if (!alarm_ptr->alarm.enabled)
            continue;

        /* repeat_days filter */
        if (alarm_ptr->repeat_days == CLK_REPEAT_TODAY) {
            /* Compare full date (year+month+day) to avoid cross-month false triggers */
            struct tm alarm_tm;
            if (!clk_time_localtime_from(alarm_ptr->today_date, &alarm_tm))
                continue;
            if (alarm_tm.tm_year != time_info.tm_year || alarm_tm.tm_mon != time_info.tm_mon ||
                alarm_tm.tm_mday != time_info.tm_mday)
                continue;
        } else if (alarm_ptr->repeat_days == CLK_REPEAT_EVERYDAY) {
            /* no day-of-week filter */
        } else {
            int today_wday = time_info.tm_wday == 0 ? 7 : time_info.tm_wday;
            if (today_wday != (int)alarm_ptr->repeat_days)
                continue;
        }

        if (!clk_alarm_check(&alarm_ptr->alarm))
            continue;

        if (alarm_ptr->sound) {
            clk_audio_play_inst* inst =
                clk_audio_play(alarm_ptr->sound, alarm_ptr->volume, alarm_ptr->loop,
                               alarm_ptr->loop ? 0 : alarm_ptr->repeat_count);
            if (inst)
                add_bell(clock, inst);
        }

        if (alarm_ptr->repeat_days != CLK_REPEAT_TODAY)
            clk_alarm_rearm(&alarm_ptr->alarm);
    }

    for (int i = 0; i < clock->pomodoro_count; ++i) {
        clk_clock_pomodoro* pomodoro = &clock->pomodoros[i];
        if (!pomodoro->enabled || pomodoro->paused || pomodoro->segment_count == 0)
            continue;
        if (!clk_timer_finished(&pomodoro->timer))
            continue;

        if (pomodoro->current_segment >= 0 && pomodoro->current_segment < pomodoro->segment_count) {
            clk_clock_pomodoro_segment* seg = &pomodoro->segments[pomodoro->current_segment];
            if (seg->sound) {
                clk_audio_play_inst* inst = clk_audio_play(seg->sound, seg->volume, seg->loop,
                                                           seg->loop ? 0 : seg->repeat_count);
                if (inst)
                    add_bell(clock, inst);
            }
        }
        pomodoro->current_segment = (pomodoro->current_segment + 1) % pomodoro->segment_count;
        clk_timer_start(&pomodoro->timer,
                        pomodoro->segments[pomodoro->current_segment].duration_seconds);
    }
}

/* ================================================================
 *  Time format translation
 * ================================================================ */

/** Convert user-facing time tokens to strftime format strings.
 *  yyyy→%Y  yy→%y  MM→%M (minute)  dd→%d  hh→%H  mm→%m (month)  ss→%S
 *  NOTE: MM/mm capitalization is inverted vs strftime — see section header
 *  in clk_clock.h for details. Other characters pass through literally.
 *  Returns false on truncation or unrecognized token. */
bool clk_clock_translate_format(const char* user_format, char* strftime_format,
                                size_t strftime_format_size) {
    if (!user_format || !strftime_format || strftime_format_size == 0)
        return false;

    int i = 0, o = 0;
    int end = (int)strlen(user_format);
    int output_end = (int)strftime_format_size - 1;

    while (i < end && o < output_end) {
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

    struct tm time_info;

    if (!clk_time_localtime(&time_info))
        return false;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
    if (strftime(buffer, buffer_size, strftime_format, &time_info) == 0)
        return false;
#pragma GCC diagnostic pop
    return true;
}
