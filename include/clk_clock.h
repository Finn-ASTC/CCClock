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
#define CLK_CLOCK_NAME_MAX 64
#define CLK_POMODORO_MAX_SEGMENTS 12
#define CLK_ALARM_MAX 64
#define CLK_POMODORO_MAX 64

/* Alarm repeat day constants
 *
 * Weekday values match tm_wday after Sunday→7 mapping:
 *   tm_wday: Sun=0 Mon=1 Tue=2 Wed=3 Thu=4 Fri=5 Sat=6
 *   mapped:  Sun=7 Mon=1 Tue=2 Wed=3 Thu=4 Fri=5 Sat=6
 *
 * TODAY and EVERYDAY use non-weekday values so they never collide.
 */
typedef enum {
    CLK_REPEAT_TODAY = 0,
    CLK_REPEAT_MONDAY = 1,
    CLK_REPEAT_TUESDAY,
    CLK_REPEAT_WEDNESDAY,
    CLK_REPEAT_THURSDAY,
    CLK_REPEAT_FRIDAY,
    CLK_REPEAT_SATURDAY,
    CLK_REPEAT_SUNDAY,
    CLK_REPEAT_EVERYDAY
} clk_repeat_days;

/* ------------------------------------------------------------------
 *  Types
 * ------------------------------------------------------------------ */

typedef struct {
    int id;
    char name[CLK_CLOCK_NAME_MAX];
    clk_alarm alarm;
    clk_audio_sound* sound;
    int repeat_count;
    clk_repeat_days repeat_days;
    time_t today_date;
    float volume;
    bool loop;
} clk_clock_alarm;

typedef struct {
    int id;
    char name[CLK_CLOCK_NAME_MAX];
    int duration_seconds;
    clk_audio_sound* sound;
    int repeat_count;
    float volume;
    bool loop;
} clk_clock_pomodoro_segment;

typedef struct {
    int id;
    char name[CLK_CLOCK_NAME_MAX];
    clk_clock_pomodoro_segment segments[CLK_POMODORO_MAX_SEGMENTS];
    int segment_count;
    clk_timer timer;
    int current_segment;
    bool enabled;
    bool paused;
} clk_clock_pomodoro;

typedef struct {
    clk_clock_alarm alarms[CLK_ALARM_MAX];
    int alarm_count;
    clk_clock_pomodoro pomodoros[CLK_POMODORO_MAX];
    int pomodoro_count;
    clk_audio_engine* audio_engine;
    clk_audio_play_inst* active_bells[CLK_ALARM_MAX + CLK_POMODORO_MAX];
    int active_bell_count;
} clk_clock;

/* ================================================================
 *  Lifecycle
 * ================================================================ */

/** Zero-initialise and bind an audio engine. */
void clk_clock_init(clk_clock* clock, clk_audio_engine* audio_engine);

/** Stop all active bells.  Stops and destroys all bell instances. */
void clk_clock_deinit(clk_clock* clock);

/* ================================================================
 *  Alarms
 * ================================================================ */

/** Append a copy of @p alarm.  Returns false if the array is full. */
bool clk_clock_add_alarm(clk_clock* clock, const clk_clock_alarm* alarm);

/** Insert a copy of @p alarm at @p index (0..alarm_count).
 *  Existing alarms from that position onward are shifted right.
 *  Returns false on out-of-range or array full. */
bool clk_clock_add_alarm_at(clk_clock* clock, const clk_clock_alarm* alarm, int index);

/** Remove the alarm at @p index.  Returns false on out-of-range. */
bool clk_clock_remove_alarm(clk_clock* clock, int index);

/** Remove the alarm whose id matches @p id.  Returns false if not found. */
bool clk_clock_remove_alarm_by_id(clk_clock* clock, int id);

/** Enable or disable without deleting. */
void clk_clock_alarm_set_enabled(clk_clock* clock, int index, bool enabled);

int clk_clock_alarm_count(const clk_clock* clock);

/** Find the first alarm whose id field equals @p id.  Returns NULL if not found. */
clk_clock_alarm* clk_clock_find_alarm_by_id(clk_clock* clock, int id);

/** Find the array index of the alarm whose id field equals @p id.  Returns -1 if not found. */
int clk_clock_find_alarm_index_by_id(const clk_clock* clock, int id);

/** Find the first alarm whose name field equals @p name.  Returns NULL if not found. */
clk_clock_alarm* clk_clock_find_alarm_by_name(clk_clock* clock, const char* name);

/** Return the smallest non-negative id not used by any existing alarm. */
int clk_clock_next_alarm_id(const clk_clock* clock);

/* ================================================================
 *  Pomodoro groups
 * ================================================================ */

/** Append a copy of @p pomodoro.  Returns false if the array is full. */
bool clk_clock_add_pomodoro(clk_clock* clock, const clk_clock_pomodoro* pomodoro);

/** Insert a copy at @p index (0..pomodoro_count). */
bool clk_clock_add_pomodoro_at(clk_clock* clock, const clk_clock_pomodoro* pomodoro, int index);

/** Remove the pomodoro at @p index. */
bool clk_clock_remove_pomodoro(clk_clock* clock, int index);

/** Remove the pomodoro whose id matches @p id. */
bool clk_clock_remove_pomodoro_by_id(clk_clock* clock, int id);

int clk_clock_pomodoro_count(const clk_clock* clock);

/** Find the first pomodoro whose id matches @p id.  Returns NULL if not found. */
clk_clock_pomodoro* clk_clock_find_pomodoro_by_id(clk_clock* clock, int id);

/** Find the array index of the pomodoro whose id matches.  Returns -1 if not found. */
int clk_clock_find_pomodoro_index_by_id(const clk_clock* clock, int id);

/** Find the first pomodoro whose name matches @p name. */
clk_clock_pomodoro* clk_clock_find_pomodoro_by_name(clk_clock* clock, const char* name);

/** Return the smallest non-negative id not used by any existing pomodoro. */
int clk_clock_next_pomodoro_id(const clk_clock* clock);

/** Append a segment to an existing pomodoro group. */
bool clk_clock_pomodoro_add_segment(clk_clock* clock, int pomodoro_index,
                                    const clk_clock_pomodoro_segment* segment);

/** Insert a segment at @p segment_index (0..segment_count).
 *  Existing segments from that position onward are shifted right.
 *  Returns false on out-of-range or array full. */
bool clk_clock_pomodoro_add_segment_at(clk_clock* clock, int pomodoro_index,
                                       const clk_clock_pomodoro_segment* segment,
                                       int segment_index);

/** Remove a segment by index. */
bool clk_clock_pomodoro_remove_segment(clk_clock* clock, int pomodoro_index, int segment_index);

/** Remove all segments from a pomodoro group.  Safer than looping remove_segment
 *  when you plan to rebuild the segment list entirely. */
void clk_clock_pomodoro_clear_segments(clk_clock* clock, int pomodoro_index);

/** Find the first segment whose id equals @p segment_id within the pomodoro
 *  identified by @p pomodoro_id.  Returns NULL if either id is not found. */
clk_clock_pomodoro_segment* clk_clock_pomodoro_find_segment_by_id(clk_clock* clock, int pomodoro_id,
                                                                  int segment_id);

/** Find the array index of the segment matching both ids.  Returns -1 if not found. */
int clk_clock_pomodoro_find_segment_index_by_id(const clk_clock* clock, int pomodoro_id,
                                                int segment_id);

/** Start cycling from segment 0. */
void clk_clock_pomodoro_start(clk_clock* clock, int index);

/** Freeze the timer at current remaining time. */
void clk_clock_pomodoro_pause(clk_clock* clock, int index);

/** Resume from a paused state. */
void clk_clock_pomodoro_resume(clk_clock* clock, int index);

/** Stop and reset to segment 0 (not running). */
void clk_clock_pomodoro_stop(clk_clock* clock, int index);

/** Enable or disable a pomodoro group without starting/stopping its timer. */
void clk_clock_pomodoro_set_enabled(clk_clock* clock, int index, bool enabled);

/* ================================================================
 *  Active bells
 * ================================================================ */

/** Stop the most recently added bell and remove it from the list. */
void clk_clock_stop_bell(clk_clock* clock);
/** Stop and clear ALL active bells. */
void clk_clock_stop_all_bells(clk_clock* clock);
int clk_clock_bell_count(const clk_clock* clock);

/* ================================================================
 *  Per-frame update
 * ================================================================ */

/** Check all alarms / pomodoros.  Triggers audio via
 *  clk_audio_play() when timers fire.  Caller must also call
 *  clk_audio_update() each frame. */
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
