#ifndef CLK_BGM_H
#define CLK_BGM_H

#include <stdbool.h>

#include "clk_audio.h"

#define CLK_BGM_SOUND_MAX 256

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    clk_audio_sound* sound;
    clk_audio_play_inst* instance;
    clk_audio_engine* engine;
    bool enabled;
    int volume;
    char sound_file[CLK_BGM_SOUND_MAX];
} clk_bgm;

/** Bind the audio engine.  All other fields are zeroed. */
void clk_bgm_init(clk_bgm* bgm, clk_audio_engine* engine);

/** Stop playback and destroy the sound template. */
void clk_bgm_deinit(clk_bgm* bgm);

/** Enable or pause background audio. */
void clk_bgm_set_enabled(clk_bgm* bgm, bool enabled);

/** Set volume 0–100.  Applied to current instance immediately. */
void clk_bgm_set_volume(clk_bgm* bgm, int volume);

/** Load (or reload) the sound file.  Returns false on failure. */
bool clk_bgm_load_sound(clk_bgm* bgm, const char* path);

#ifdef __cplusplus
}
#endif

#endif
