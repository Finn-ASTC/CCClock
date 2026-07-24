#include "clk_audio.h"

#include <stdlib.h>
#include <string.h>

#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>

/* ================================================================
 *  Internal structs
 * ================================================================ */

struct clk_audio_engine {
    ma_engine engine;
};

struct clk_audio_sound {
    clk_audio_engine* engine;
    char* file_path;
};

struct clk_audio_play_inst {
    ma_sound sound;
    bool managed_looping;
    int managed_remaining;
    clk_audio_sound* template;
    struct clk_audio_play_inst* next;
    clk_audio_play_inst** bell_ref;
};

static clk_audio_play_inst* clk_audio_managed_head = NULL;

/* ================================================================
 *  Engine lifecycle
 * ================================================================ */

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
    if (volume < CLK_AUDIO_VOLUME_MIN)
        volume = CLK_AUDIO_VOLUME_MIN;
    if (volume > CLK_AUDIO_VOLUME_MAX)
        volume = CLK_AUDIO_VOLUME_MAX;
    ma_engine_set_volume(&engine->engine, volume);
}

/* ================================================================
 *  Template lifecycle
 * ================================================================ */

clk_audio_sound* clk_audio_load(clk_audio_engine* engine, const char* path) {
    if (!engine || !path)
        return NULL;

    clk_audio_sound* sound = malloc(sizeof(clk_audio_sound));
    if (!sound)
        return NULL;

    sound->engine = engine;
    sound->file_path = strdup(path);
    if (!sound->file_path) {
        free(sound);
        return NULL;
    }
    return sound;
}

void clk_audio_destroy(clk_audio_sound* sound) {
    if (!sound)
        return;

    /* stop all managed instances belonging to this template */
    clk_audio_play_inst** pp = &clk_audio_managed_head;
    while (*pp) {
        clk_audio_play_inst* s = *pp;
        if (s->template == sound) {
            if (s->bell_ref)
                *s->bell_ref = NULL;
            ma_sound_uninit(&s->sound);
            *pp = s->next;
            free(s);
        } else {
            pp = &s->next;
        }
    }

    free(sound->file_path);
    free(sound);
}

/* ================================================================
 *  Playback & managed tracking
 * ================================================================ */

/** Clamp @p volume to [CLK_AUDIO_VOLUME_MIN, CLK_AUDIO_VOLUME_MAX]. */
static float clamp_volume(float volume) {
    if (volume < CLK_AUDIO_VOLUME_MIN)
        return CLK_AUDIO_VOLUME_MIN;
    if (volume > CLK_AUDIO_VOLUME_MAX)
        return CLK_AUDIO_VOLUME_MAX;
    return volume;
}

/** Create a new independent play instance from @p sound template.
 *  Calls ma_sound_init_from_file — file I/O and decode happen on the
 *  calling thread.  Fine for short alarm sounds; long BGM could
 *  benefit from pre-decoded caching later. */
clk_audio_play_inst* clk_audio_play(clk_audio_sound* sound, float volume, bool loop, int count) {
    if (!sound || !sound->engine)
        return NULL;
    if (!loop && count <= 0)
        return NULL;

    clk_audio_play_inst* inst = malloc(sizeof(clk_audio_play_inst));
    if (!inst)
        return NULL;
    memset(inst, 0, sizeof(*inst));

    ma_result r = ma_sound_init_from_file(&sound->engine->engine, sound->file_path, 0, NULL, NULL,
                                          &inst->sound);
    if (r != MA_SUCCESS) {
        free(inst);
        return NULL;
    }

    inst->template = sound;
    inst->managed_looping = loop;
    inst->managed_remaining = loop ? 0 : count;

    float vol = clamp_volume(volume);
    ma_sound_set_volume(&inst->sound, vol);
    ma_sound_set_looping(&inst->sound, loop ? MA_TRUE : MA_FALSE);
    ma_sound_start(&inst->sound);

    /* Prepend to head — newest instances processed first */
    inst->next = clk_audio_managed_head;
    clk_audio_managed_head = inst;

    return inst;
}

void clk_audio_stop(clk_audio_play_inst* inst) {
    if (!inst)
        return;

    ma_sound_uninit(&inst->sound);

    if (inst->bell_ref)
        *inst->bell_ref = NULL;

    clk_audio_play_inst** pp = &clk_audio_managed_head;
    while (*pp) {
        if (*pp == inst) {
            *pp = inst->next;
            break;
        }
        pp = &(*pp)->next;
    }

    free(inst);
}

void clk_audio_pause(clk_audio_play_inst* inst) {
    if (!inst)
        return;
    ma_sound_stop(&inst->sound);
}

void clk_audio_resume(clk_audio_play_inst* inst) {
    if (!inst)
        return;
    ma_sound_start(&inst->sound);
}

void clk_audio_inst_set_volume(clk_audio_play_inst* inst, float volume) {
    if (!inst)
        return;
    ma_sound_set_volume(&inst->sound, clamp_volume(volume));
}

const char* clk_audio_sound_get_path(const clk_audio_sound* sound) {
    return sound ? sound->file_path : NULL;
}

void clk_audio_inst_set_bell_ref(clk_audio_play_inst* inst, clk_audio_play_inst** ref) {
    if (inst)
        inst->bell_ref = ref;
}

/* ================================================================
 *  Query
 * ================================================================ */

bool clk_audio_is_playing(const clk_audio_play_inst* inst) {
    if (!inst)
        return false;
    return ma_sound_is_playing(&inst->sound);
}

bool clk_audio_is_finished(const clk_audio_play_inst* inst) {
    if (!inst)
        return true;
    return ma_sound_at_end(&inst->sound);
}

/* ================================================================
 *  Managed playback
 * ================================================================ */

void clk_audio_update(void) {
    clk_audio_play_inst** pp = &clk_audio_managed_head;

    while (*pp) {
        clk_audio_play_inst* s = *pp;

        /* looping or still playing — leave alone */
        if (s->managed_looping || !ma_sound_at_end(&s->sound)) {
            pp = &s->next;
            continue;
        }

        s->managed_remaining--;
        if (s->managed_remaining > 0) {
            ma_sound_seek_to_pcm_frame(&s->sound, 0);
            ma_sound_start(&s->sound);
            pp = &s->next;
        } else {
            if (s->bell_ref)
                *s->bell_ref = NULL;
            ma_sound_uninit(&s->sound);
            *pp = s->next;
            free(s);
        }
    }
}

int clk_audio_playing_count(void) {
    int count = 0;
    for (clk_audio_play_inst* s = clk_audio_managed_head; s; s = s->next)
        count++;
    return count;
}
