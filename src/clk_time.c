#include "clk_time.h"

#include <time.h>

/* ================================================================
 *  Utility
 * ================================================================ */

bool clk_time_localtime(struct tm* out) {
    return clk_time_localtime_from(time(NULL), out);
}

bool clk_time_localtime_from(time_t timestamp, struct tm* out) {
    if (!out)
        return false;

#if defined(_WIN32) || defined(_WIN64)
    return localtime_s(out, &timestamp) == 0;
#else
    return localtime_r(&timestamp, out) != NULL;
#endif
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

    struct tm time_info;
    if (!clk_time_localtime(&time_info))
        return false;

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
