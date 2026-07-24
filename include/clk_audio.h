#ifndef CLK_AUDIO_H
#define CLK_AUDIO_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CLK_AUDIO_VOLUME_MIN 0.0f
#define CLK_AUDIO_VOLUME_MAX 1.0f

/* ------------------------------------------------------------------
 *  Opaque types
 * ------------------------------------------------------------------ */

typedef struct clk_audio_engine clk_audio_engine;

/** Template: stores the file path and a back-reference to the engine.
 *  Owned by the caller (e.g. alarm / pomodoro).  Multiple independent
 *  play instances can be created from a single template. */
typedef struct clk_audio_sound clk_audio_sound;

/** Play instance: created by clk_audio_play().  Owns a miniaudio
 *  ma_sound; automatically tracked by clk_audio_update() and
 *  clk_audio_stop().  clk_audio_update() cleans up finished
 *  non-looping instances. */
typedef struct clk_audio_play_inst clk_audio_play_inst;

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
 *  Template lifecycle
 * ------------------------------------------------------------------ */

/** Create a sound template from @p path.  The engine must outlive the
 *  template.  Returns NULL on allocation failure. */
clk_audio_sound* clk_audio_load(clk_audio_engine* engine, const char* path);

/** Destroy the template and stop all of its active play instances.
 *  NULL-safe. */
void clk_audio_destroy(clk_audio_sound* sound);

/* ------------------------------------------------------------------
 *  Playback (instances)
 * ------------------------------------------------------------------ */

/** Create a new independent play instance from @p sound.
 *
 *  @p loop  = true  → repeat forever (until clk_audio_stop).
 *  @p loop  = false → play @p count times, then stop automatically.
 *  @p volume is clamped to 0.0–1.0.
 *
 *  Returns a handle for clk_audio_stop / query.  Returns NULL on
 *  failure (OOM or file I/O error).
 *
 *  Call clk_audio_update() once per frame to drive retrigger and
 *  automatic cleanup of non-looping instances. */
clk_audio_play_inst* clk_audio_play(clk_audio_sound* sound, float volume, bool loop, int count);

/** Stop and destroy a single play instance.  NULL-safe.
 *  The instance is removed from the managed list immediately. */
void clk_audio_stop(clk_audio_play_inst* inst);

/** Pause without resetting position.  NULL-safe. */
void clk_audio_pause(clk_audio_play_inst* inst);

/** Resume from the position where it was paused.  NULL-safe. */
void clk_audio_resume(clk_audio_play_inst* inst);

/** Set per-instance volume (clamped 0.0–1.0).  NULL-safe. */
void clk_audio_inst_set_volume(clk_audio_play_inst* inst, float volume);

/** Return the file path this sound template was loaded from, or NULL. */
const char* clk_audio_sound_get_path(const clk_audio_sound* sound);

/* ------------------------------------------------------------------
 *  Query
 * ------------------------------------------------------------------ */

bool clk_audio_is_playing(const clk_audio_play_inst* inst);
bool clk_audio_is_finished(const clk_audio_play_inst* inst);

/* ------------------------------------------------------------------
 *  Managed playback
 * ------------------------------------------------------------------ */

/** Process managed instances — retrigger countdown sounds or remove
 *  finished ones.  Call once per frame. */
void clk_audio_update(void);

/** Number of play instances currently being managed. */
int clk_audio_playing_count(void);

#ifdef __cplusplus
}
#endif

#endif
