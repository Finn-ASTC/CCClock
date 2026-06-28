#include "clk_audio.h"

#include <stdlib.h>

#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>

/* ------------------------------------------------------------------
 *  Internal structs
 * ------------------------------------------------------------------ */

struct clk_audio_engine {
    ma_engine engine;
};

struct clk_audio_sound {
    ma_sound sound;
};

/* ------------------------------------------------------------------
 *  Engine lifecycle
 * ------------------------------------------------------------------ */

bool clk_audio_init(clk_audio_engine** out_engine) {
    if (!out_engine)
        return false;

    clk_audio_engine* engine = malloc(sizeof(clk_audio_engine));
    if (!engine)
        return false;

    ma_result result = ma_engine_init(NULL, &engine->engine);
    if (result != MA_SUCCESS) {
        free(engine);
        return false;
    }

    *out_engine = engine;
    return true;
}

void clk_audio_shutdown(clk_audio_engine* engine) {
    if (!engine)
        return;
    ma_engine_uninit(&engine->engine);
    free(engine);
}

void clk_audio_engine_set_volume(clk_audio_engine* engine, float volume) {
    if (!engine)
        return;
    if (volume < 0.0f)
        volume = 0.0f;
    if (volume > 1.0f)
        volume = 1.0f;
    ma_engine_set_volume(&engine->engine, volume);
}

/* ------------------------------------------------------------------
 *  Sound lifecycle
 * ------------------------------------------------------------------ */

clk_audio_sound* clk_audio_load(clk_audio_engine* engine, const char* path) {
    if (!engine || !path)
        return NULL;

    clk_audio_sound* sound = malloc(sizeof(clk_audio_sound));
    if (!sound)
        return NULL;

    ma_result result = ma_sound_init_from_file(&engine->engine, path, 0, NULL, NULL, &sound->sound);
    if (result != MA_SUCCESS) {
        free(sound);
        return NULL;
    }

    return sound;
}

void clk_audio_destroy(clk_audio_sound* sound) {
    if (!sound)
        return;
    ma_sound_uninit(&sound->sound);
    free(sound);
}

/* ------------------------------------------------------------------
 *  Playback
 * ------------------------------------------------------------------ */

void clk_audio_play(clk_audio_sound* sound, clk_audio_loop loop) {
    if (!sound)
        return;
    ma_sound_set_looping(&sound->sound, loop == CLK_AUDIO_LOOP_ON);
    ma_sound_start(&sound->sound);
}

void clk_audio_pause(clk_audio_sound* sound) {
    if (!sound)
        return;
    ma_sound_stop(&sound->sound);
}

void clk_audio_resume(clk_audio_sound* sound) {
    if (!sound)
        return;
    ma_sound_start(&sound->sound);
}

void clk_audio_stop(clk_audio_sound* sound) {
    if (!sound)
        return;
    ma_sound_stop(&sound->sound);
    ma_sound_seek_to_pcm_frame(&sound->sound, 0);
}

void clk_audio_sound_set_volume(clk_audio_sound* sound, float volume) {
    if (!sound)
        return;
    if (volume < 0.0f)
        volume = 0.0f;
    if (volume > 1.0f)
        volume = 1.0f;
    ma_sound_set_volume(&sound->sound, volume);
}

/* ------------------------------------------------------------------
 *  Query
 * ------------------------------------------------------------------ */

bool clk_audio_is_playing(const clk_audio_sound* sound) {
    if (!sound)
        return false;
    return ma_sound_is_playing(&sound->sound);
}

bool clk_audio_is_paused(const clk_audio_sound* sound) {
    if (!sound)
        return false;
    return ma_sound_at_end(&sound->sound) == MA_FALSE && !ma_sound_is_playing(&sound->sound);
}

bool clk_audio_is_finished(const clk_audio_sound* sound) {
    if (!sound)
        return true;
    return ma_sound_at_end(&sound->sound);
}

/* ------------------------------------------------------------------
 *  Managed playback
 * ------------------------------------------------------------------ */

#define CLK_AUDIO_MAX_PLAYING 64

typedef struct {
    clk_audio_sound* sound;
    bool looping;
    int remaining;
    float volume;
} clk_playing_entry;

static clk_playing_entry playing[CLK_AUDIO_MAX_PLAYING];
static int playing_count;

void clk_audio_play_loop(clk_audio_sound* sound, float volume) {
    if (!sound || playing_count >= CLK_AUDIO_MAX_PLAYING)
        return;
    clk_audio_sound_set_volume(sound, volume);
    clk_audio_play(sound, CLK_AUDIO_LOOP_ON);
    playing[playing_count++] = (clk_playing_entry){sound, true, 0, volume};
}

void clk_audio_play_times(clk_audio_sound* sound, float volume, int count) {
    if (!sound || count <= 0 || playing_count >= CLK_AUDIO_MAX_PLAYING)
        return;
    clk_audio_sound_set_volume(sound, volume);
    clk_audio_play(sound, CLK_AUDIO_LOOP_OFF);
    playing[playing_count++] = (clk_playing_entry){sound, false, count, volume};
}

void clk_audio_update(void) {
    for (int i = 0; i < playing_count;) {
        clk_playing_entry* e = &playing[i];
        if (e->looping) {
            i++;
            continue;
        }
        if (!clk_audio_is_finished(e->sound)) {
            i++;
            continue;
        }
        e->remaining--;
        if (e->remaining > 0) {
            clk_audio_play(e->sound, CLK_AUDIO_LOOP_OFF);
            i++;
        } else {
            clk_audio_stop(e->sound);
            for (int j = i; j < playing_count - 1; ++j)
                playing[j] = playing[j + 1];
            playing_count--;
        }
    }
}

void clk_audio_stop_all(void) {
    for (int i = 0; i < playing_count; ++i)
        clk_audio_stop(playing[i].sound);
    playing_count = 0;
}

int clk_audio_playing_count(void) {
    return playing_count;
}
