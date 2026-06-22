#include "clk_clock.h"

#include <string.h>
#include <time.h>

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

    time(&raw_time);

#if defined(_WIN32) || defined(_WIN64)
    if (localtime_s(&time_info, &raw_time) != 0)
        return false;
#else
    if (!localtime_r(&raw_time, &time_info))
        return false;
#endif

    return strftime(buffer, buffer_size, strftime_format, &time_info) > 0;
}

/* ================================================================
 *  Timer
 * ================================================================ */

void clk_timer_start(clk_timer* timer, int64_t seconds) {
    if (!timer)
        return;
    timer->total_seconds = seconds;
    timer->started_at = time(NULL);
    timer->running = true;
    timer->paused = false;
    timer->consumed = 0;
}

void clk_timer_pause(clk_timer* timer) {
    if (!timer || !timer->running || timer->paused)
        return;
    timer->paused = true;
    timer->paused_at = time(NULL);
    timer->consumed += (int64_t)difftime(timer->paused_at, timer->started_at);
}

void clk_timer_resume(clk_timer* timer) {
    if (!timer || !timer->paused)
        return;
    timer->paused = false;
    timer->started_at = time(NULL);
}

int64_t clk_timer_remaining(const clk_timer* timer) {
    if (!timer || !timer->running)
        return 0;

    int64_t elapsed;
    if (timer->paused)
        elapsed = timer->consumed;
    else
        elapsed = timer->consumed + (int64_t)difftime(time(NULL), timer->started_at);

    int64_t remaining = timer->total_seconds - elapsed;
    return remaining > 0 ? remaining : 0;
}

bool clk_timer_finished(const clk_timer* timer) {
    if (!timer || !timer->running)
        return false;
    return clk_timer_remaining(timer) == 0;
}

int64_t clk_timer_elapsed(const clk_timer* timer) {
    if (!timer || !timer->running)
        return 0;
    if (timer->paused)
        return timer->consumed;
    return timer->consumed + (int64_t)difftime(time(NULL), timer->started_at);
}

/* ================================================================
 *  Alarm
 * ================================================================ */

void clk_alarm_set(clk_alarm* alarm, int hour, int minute, int second) {
    if (!alarm)
        return;
    alarm->hour = hour;
    alarm->minute = minute;
    alarm->second = second;
    alarm->enabled = true;
    alarm->triggered = false;
}

bool clk_alarm_check(clk_alarm* alarm) {
    if (!alarm || !alarm->enabled || alarm->triggered)
        return false;

    time_t now = time(NULL);
    struct tm time_info;

#if defined(_WIN32) || defined(_WIN64)
    if (localtime_s(&time_info, &now) != 0)
        return false;
#else
    if (!localtime_r(&now, &time_info))
        return false;
#endif

    if (time_info.tm_hour == alarm->hour && time_info.tm_min == alarm->minute &&
        time_info.tm_sec >= alarm->second) {
        alarm->triggered = true;
        return true;
    }
    return false;
}

void clk_alarm_rearm(clk_alarm* alarm) {
    if (!alarm)
        return;
    alarm->triggered = false;
    alarm->enabled = true;
}

void clk_alarm_disable(clk_alarm* alarm) {
    if (!alarm)
        return;
    alarm->enabled = false;
}

void clk_alarm_enable(clk_alarm* alarm) {
    if (!alarm)
        return;
    alarm->enabled = true;
}
