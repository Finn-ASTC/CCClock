#include "clk_bgm.h"

#include <string.h>

void clk_bgm_init(clk_bgm* bgm, clk_audio_engine* engine) {
    if (!bgm)
        return;
    memset(bgm, 0, sizeof(*bgm));
    bgm->engine = engine;
}

void clk_bgm_deinit(clk_bgm* bgm) {
    if (!bgm)
        return;
    clk_audio_stop(bgm->instance);
    clk_audio_destroy(bgm->sound);
    memset(bgm, 0, sizeof(*bgm));
}

bool clk_bgm_load_sound(clk_bgm* bgm, const char* path) {
    if (!bgm || !path)
        return false;
    clk_audio_destroy(bgm->sound);
    bgm->sound = NULL;
    bgm->instance = NULL;
    bgm->sound = clk_audio_load(bgm->engine, path);
    return bgm->sound != NULL;
}

void clk_bgm_set_enabled(clk_bgm* bgm, bool enabled) {
    if (!bgm)
        return;
    bgm->enabled = enabled;
    if (enabled) {
        if (bgm->instance)
            clk_audio_resume(bgm->instance);
        else if (bgm->sound)
            bgm->instance = clk_audio_play(bgm->sound, bgm->volume / 100.0f, true, 0);
    } else {
        clk_audio_pause(bgm->instance);
    }
}

void clk_bgm_set_volume(clk_bgm* bgm, int volume) {
    if (!bgm)
        return;
    bgm->volume = volume;
    clk_audio_inst_set_volume(bgm->instance, volume / 100.0f);
}
