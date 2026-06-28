#ifndef CLK_CLOCK_H
#define CLK_CLOCK_H

#include <stdbool.h>
#include <stddef.h>
#include <time.h>

#include "clk_audio.h"
#include "clk_time.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CLK_CLOCK_FORMAT_MAX_LENGTH 64
#define CLK_POMODORO_MAX_SEGMENTS 12
#define CLK_ALARM_MAX 64
#define CLK_POMODORO_MAX 64

/* Alarm repeat day constants */
#define CLK_REPEAT_TODAY 0
#define CLK_REPEAT_EVERYDAY 1
#define CLK_REPEAT_MONDAY 2
#define CLK_REPEAT_TUESDAY 3
#define CLK_REPEAT_WEDNESDAY 4
#define CLK_REPEAT_THURSDAY 5
#define CLK_REPEAT_FRIDAY 6
#define CLK_REPEAT_SATURDAY 7
#define CLK_REPEAT_SUNDAY 8

/* ------------------------------------------------------------------
 *  Types
 * ------------------------------------------------------------------ */

typedef struct {
    char name[64];
    clk_alarm alarm;
    clk_audio_sound* sound;
    int repeat_count;     /* times to repeat the sound (when !loop) */
    int repeat_days;      /* one of CLK_REPEAT_* */
    time_t today_date;    /* used only when repeat_days == CLK_REPEAT_TODAY */
    float volume;         /* 0.0 — 1.0 */
    bool loop;            /* true = infinite repeat */
} clk_clock_alarm;

typedef struct {
    char name[64];
    int duration_seconds;
    clk_audio_sound* sound;
    int repeat_count;
    float volume;         /* 0.0 — 1.0 */
    bool loop;            /* true = infinite repeat */
} clk_clock_pomodoro_segment;

typedef struct {
    char name[64];
    clk_clock_pomodoro_segment segments[CLK_POMODORO_MAX_SEGMENTS];
    int segment_count;
    clk_timer timer;     /* current segment countdown */
    int current_segment; /* -1 = not running */
    bool enabled;
    bool paused;
} clk_clock_pomodoro;

typedef struct {
    clk_clock_alarm alarms[CLK_ALARM_MAX];
    int alarm_count;
    clk_clock_pomodoro pomodoros[CLK_POMODORO_MAX];
    int pomodoro_count;
    clk_audio_engine* audio_engine;
} clk_clock;

/* ================================================================
 *  Lifecycle
 * ================================================================ */

/** Zero-initialise and bind an audio engine. */
void clk_clock_init(clk_clock* clock, clk_audio_engine* audio_engine);

/** Stop all active bells.  Does NOT free any clk_audio_sound*. */
void clk_clock_deinit(clk_clock* clock);

/* ================================================================
 *  Alarms
 * ================================================================ */

/** Append a copy of @p alarm.  Returns false if the array is full. */
bool clk_clock_add_alarm(clk_clock* clock, const clk_clock_alarm* alarm);

/** Remove the alarm at @p index.  Returns false on out-of-range. */
bool clk_clock_remove_alarm(clk_clock* clock, int index);

/** Enable or disable without deleting. */
void clk_clock_alarm_set_enabled(clk_clock* clock, int index, bool enabled);

int clk_clock_alarm_count(const clk_clock* clock);

/* ================================================================
 *  Pomodoro groups
 * ================================================================ */

bool clk_clock_add_pomodoro(clk_clock* clock, const clk_clock_pomodoro* pomodoro);
bool clk_clock_remove_pomodoro(clk_clock* clock, int index);
int clk_clock_pomodoro_count(const clk_clock* clock);

/** Append a segment to an existing pomodoro group. */
bool clk_clock_pomodoro_add_segment(clk_clock* clock, int pomodoro_index,
                                    const clk_clock_pomodoro_segment* segment);
/** Remove a segment by index. */
bool clk_clock_pomodoro_remove_segment(clk_clock* clock, int pomodoro_index, int segment_index);

/** Start cycling from segment 0. */
void clk_clock_pomodoro_start(clk_clock* clock, int index);
void clk_clock_pomodoro_pause(clk_clock* clock, int index);
void clk_clock_pomodoro_resume(clk_clock* clock, int index);
/** Stop and reset to segment 0 (not running). */
void clk_clock_pomodoro_stop(clk_clock* clock, int index);

/* ================================================================
 *  Per-frame update
 * ================================================================ */

/** Check all alarms / pomodoros.  Triggers audio via
 *  clk_audio_play_loop / clk_audio_play_times when timers fire.
 *  Caller must also call clk_audio_update() each frame. */
void clk_clock_update(clk_clock* clock);

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

#ifdef __cplusplus
}
#endif

#endif
