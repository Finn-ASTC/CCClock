#ifndef CLK_CLOCK_H
#define CLK_CLOCK_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CLK_CLOCK_FORMAT_MAX_LENGTH 64

/* ================================================================
 *  Time format translation
 *
 *  User-friendly tokens → strftime format strings.
 *  yyyy→%Y yy→%y MM→%M dd→%d hh→%H mm→%m ss→%S
 *  Other characters pass through literally.
 * ================================================================ */

bool clk_clock_translate_format(const char* user_format, char* strftime_format,
                                size_t strftime_format_size);

/* ================================================================
 *  Time formatting
 * ================================================================ */

/** Format the current local time into the supplied buffer using a
 *  strftime-style format string.  Returns false if localtime fails
 *  or the buffer is too small. */
bool clk_clock_format_now(const char* strftime_format, char* buffer, size_t buffer_size);

/* ================================================================
 *  Timer — wall-clock based countdown
 *
 *  Uses difftime() deltas; no per-frame tick required.
 *  Pause/resume supported. remaining() is always accurate.
 * ================================================================ */

typedef struct {
    int64_t total_seconds;
    time_t started_at;
    bool running;
    bool paused;
    time_t paused_at;
    int64_t consumed;
} clk_timer;

/** Reset and start counting down from @p seconds. */
void clk_timer_start(clk_timer* timer, int64_t seconds);

/** Freeze the timer at its current remaining time. */
void clk_timer_pause(clk_timer* timer);

/** Resume from a paused state. */
void clk_timer_resume(clk_timer* timer);

/** Seconds remaining (≥0). Always up-to-date. */
int64_t clk_timer_remaining(const clk_timer* timer);

/** True when remaining() has reached 0. */
bool clk_timer_finished(const clk_timer* timer);

/** Elapsed seconds since start (or last reset). */
int64_t clk_timer_elapsed(const clk_timer* timer);

/* ================================================================
 *  Alarm — clock-time based trigger
 *
 *  Compares current wall-clock time against a target HH:MM:SS.
 *  One-shot semantics: check() fires once, then must be re-armed.
 * ================================================================ */

typedef struct {
    int hour;
    int minute;
    int second;
    bool enabled;
    bool triggered;
} clk_alarm;

/** Arm an alarm.  @p hour is 0–23. */
void clk_alarm_set(clk_alarm* alarm, int hour, int minute, int second);

/** Check and fire.  Returns true only on the first match.
 *  Subsequent calls return false until re-armed. */
bool clk_alarm_check(clk_alarm* alarm);

/** Clear triggered flag, keep target time and enabled. */
void clk_alarm_rearm(clk_alarm* alarm);

/** Disable without clearing target time. */
void clk_alarm_disable(clk_alarm* alarm);

/** Enable a previously disabled alarm. */
void clk_alarm_enable(clk_alarm* alarm);

#ifdef __cplusplus
}
#endif

#endif
