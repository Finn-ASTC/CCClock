#ifndef CLK_AUDIO_H
#define CLK_AUDIO_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------
 *  Opaque types — miniaudio details hidden from callers
 * ------------------------------------------------------------------ */

typedef struct clk_audio_engine clk_audio_engine;
typedef struct clk_audio_sound clk_audio_sound;

/* ------------------------------------------------------------------
 *  Engine lifecycle
 * ------------------------------------------------------------------ */

/** Initialise the audio engine.  Returns false if the output device
 *  cannot be opened.  Call clk_audio_shutdown() to release. */
bool clk_audio_init(clk_audio_engine** out_engine);

/** Stop all sounds and release the audio device.  NULL-safe. */
void clk_audio_shutdown(clk_audio_engine* engine);

/** Set master volume.  @p volume is clamped to 0.0–1.0. */
void clk_audio_engine_set_volume(clk_audio_engine* engine, float volume);

/* ------------------------------------------------------------------
 *  Sound lifecycle
 * ------------------------------------------------------------------ */

/** Load a sound from the given file path.  Returns NULL on failure.
 *  The engine must be alive for the lifetime of every sound. */
clk_audio_sound* clk_audio_load(clk_audio_engine* engine, const char* path);

/** Release a sound.  NULL-safe — safe to call more than once. */
void clk_audio_destroy(clk_audio_sound* sound);

/* ------------------------------------------------------------------
 *  Playback
 * ------------------------------------------------------------------ */

/** Start playback with automatic repeat management.
 *
 *  @p loop      = true  → repeat forever (until clk_audio_stop).
 *  @p loop      = false → play @p count times, then stop automatically.
 *  @p volume    is clamped to 0.0–1.0.
 *
 *  Requires clk_audio_update() to be called each frame so that
 *  non-looping sounds can be retriggered / cleaned up. */
void clk_audio_play(clk_audio_sound* sound, float volume, bool loop, int count);

/** Pause without resetting position.  No-op if already paused.
 *  Does NOT remove the sound from managed tracking. */
void clk_audio_pause(clk_audio_sound* sound);

/** Resume from the position where it was paused. */
void clk_audio_resume(clk_audio_sound* sound);

/** Stop the sound and rewind to the beginning.
 *  If the sound was being managed (started via clk_audio_play),
 *  it is also removed from managed tracking. */
void clk_audio_stop(clk_audio_sound* sound);

/** Set per-sound volume.  @p volume is clamped to 0.0–1.0. */
void clk_audio_sound_set_volume(clk_audio_sound* sound, float volume);

/* ------------------------------------------------------------------
 *  Managed playback
 *  (automatically handles retrigger — call update() each frame)
 * ------------------------------------------------------------------ */

/** Process managed sounds — retrigger countdown sounds or remove
 *  finished ones.  Call once per frame. */
void clk_audio_update(void);

/** Number of sounds currently being managed. */
int clk_audio_playing_count(void);

/* ------------------------------------------------------------------
 *  Query
 * ------------------------------------------------------------------ */

bool clk_audio_is_playing(const clk_audio_sound* sound);
bool clk_audio_is_paused(const clk_audio_sound* sound);
bool clk_audio_is_finished(const clk_audio_sound* sound);

#ifdef __cplusplus
}
#endif

#endif
