#include "clk_app_setup.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

const char* clk_repeat_day_options[] = {
    "Today",  "Monday",   "Tuesday", "Wednesday", "Thursday",
    "Friday", "Saturday", "Sunday",  "Everyday",
};

/* ================================================================
 *  Internal helpers
 * ================================================================ */

/** Register the "basic" tab items (time format, font, theme, quit). */
static void register_basic_tab(const clk_app_config* cfg, clk_menu* menu) {
    clk_menu_add_tab(menu, CLK_TAB_BASIC, "basic");

    /* ---- time format ---- */
    clk_menu_add_item_str(menu, CLK_TAB_BASIC, CLK_BASIC_ITEM_TIME_FORMAT, "time format",
                          cfg->ascii_clock.time_formats.index,
                          cfg->ascii_clock.time_formats.options,
                          cfg->ascii_clock.time_formats.count);

    /* ---- font ---- */
    clk_menu_add_item_str(menu, CLK_TAB_BASIC, CLK_BASIC_ITEM_FONT, "font",
                          cfg->ascii_clock.fonts.index, (const char**)cfg->ascii_clock.fonts.names,
                          cfg->ascii_clock.fonts.count);

    /* ---- menu theme ---- */
    clk_menu_add_item_str(menu, CLK_TAB_BASIC, CLK_BASIC_ITEM_THEME, "menu theme",
                          cfg->themes.index, (const char**)cfg->themes.names, cfg->themes.count);

    /* ---- quit ---- */
    clk_menu_add_item_action(menu, CLK_TAB_BASIC, CLK_BASIC_ITEM_QUIT, "quit");
}

/* ================================================================
 *  Menu
 * ================================================================ */

clk_menu* clk_app_setup_menu(const clk_app_config* cfg) {
    clk_menu* menu = clk_menu_create();
    if (!menu)
        return NULL;

    register_basic_tab(cfg, menu);

    /* Alarm tab (placeholder — items to be added later) */
    clk_menu_add_tab(menu, CLK_TAB_ALARM, "alarm");

    /* Pomodoro tab (placeholder — items to be added later) */
    clk_menu_add_tab(menu, CLK_TAB_POMODORO, "pomodoro");

    return menu;
}

/* ================================================================
 *  Render & theme
 * ================================================================ */

bool clk_app_setup_render(clk_ascii_render* render, const clk_cfg_ascii_clock_theme* ascii_clock) {
    if (!render || !ascii_clock)
        return false;
    if (ascii_clock->fonts.index < 0 || ascii_clock->fonts.index >= ascii_clock->fonts.count)
        return false;

    if (!clk_ascii_render_create(render, ascii_clock->fonts.paths[ascii_clock->fonts.index]))
        return false;

    clk_ascii_render_set_z_order(render, 0);
    clk_ascii_render_add_to_term(render);
    return true;
}

bool clk_app_setup_theme(clk_menu_theme* theme, const clk_cfg_themes* themes) {
    if (!theme || !themes)
        return false;
    if (themes->index < 0 || themes->index >= themes->count)
        return false;

    memset(theme, 0, sizeof(*theme));
    return clk_menu_theme_load(themes->paths[themes->index], theme);
}

/* ================================================================
 *  Clock
 * ================================================================ */

#define CLK_SOUND_PATH_MAX (CLK_CONFIG_ALARM_SOUND_MAX + 512)

bool clk_app_setup_clock(clk_clock* clock, clk_audio_engine** out_engine,
                         const clk_app_config* cfg) {
    if (!clock || !out_engine || !cfg)
        return false;

    if (!clk_audio_init(out_engine))
        return false;

    clk_clock_init(clock, *out_engine);

    const char* audio_dir = NULL;
    clk_json_value* audio_dir_val = clk_json_object_get(cfg->json, "audio_dir");
    if (audio_dir_val && clk_json_is_string(audio_dir_val))
        clk_json_get_string(audio_dir_val, &audio_dir);

    for (int i = 0; i < cfg->clock.alarms.count; ++i) {
        const clk_cfg_alarm* src = &cfg->clock.alarms.items[i];
        clk_clock_alarm alarm;
        memset(&alarm, 0, sizeof(alarm));

        alarm.id = i;
        strncpy(alarm.name, src->name, CLK_CLOCK_NAME_MAX - 1);
        clk_alarm_set(&alarm.alarm, src->hour, src->minute, 0);
        alarm.alarm.enabled = src->enabled;
        alarm.repeat_count = src->sound_repeat;
        alarm.repeat_days = src->repeat_days;
        alarm.today_date = src->today_date;
        alarm.volume = src->volume / 100.0f;
        alarm.loop = src->loop;

        if (audio_dir && src->sound_file[0] != '\0') {
            char sound_path[CLK_SOUND_PATH_MAX];
            snprintf(sound_path, sizeof(sound_path), "%s/%s.mp3", audio_dir, src->sound_file);
            alarm.sound = clk_audio_load(*out_engine, sound_path);
        }

        clk_clock_add_alarm(clock, &alarm);
    }

    for (int i = 0; i < cfg->clock.pomodoros.count; ++i) {
        const clk_cfg_pomodoro* src = &cfg->clock.pomodoros.items[i];
        clk_clock_pomodoro pomodoro;
        memset(&pomodoro, 0, sizeof(pomodoro));

        pomodoro.id = i;
        strncpy(pomodoro.name, src->name, CLK_CLOCK_NAME_MAX - 1);
        pomodoro.current_segment = -1;

        int pomo_index = clock->pomodoro_count;
        clk_clock_add_pomodoro(clock, &pomodoro);

        for (int j = 0; j < src->segment_count; ++j) {
            const clk_cfg_pomodoro_segment* src_seg = &src->segments[j];
            clk_clock_pomodoro_segment seg;
            memset(&seg, 0, sizeof(seg));

            seg.id = j;
            strncpy(seg.name, src_seg->name, CLK_CLOCK_NAME_MAX - 1);
            seg.duration_seconds = src_seg->duration_seconds;
            seg.repeat_count = src_seg->sound_repeat;
            seg.volume = src_seg->volume / 100.0f;
            seg.loop = src_seg->loop;

            if (audio_dir && src_seg->sound_file[0] != '\0') {
                char sound_path[CLK_SOUND_PATH_MAX];
                snprintf(sound_path, sizeof(sound_path), "%s/%s.mp3", audio_dir,
                         src_seg->sound_file);
                seg.sound = clk_audio_load(*out_engine, sound_path);
            }

            clk_clock_pomodoro_add_segment(clock, pomo_index, &seg);
        }
    }

    return true;
}

void clk_app_setup_clock_deinit(clk_clock* clock, clk_audio_engine* engine) {
    if (!clock && !engine)
        return;

    if (clock) {
        clk_clock_deinit(clock);

        for (int i = 0; i < clock->alarm_count; ++i)
            clk_audio_destroy(clock->alarms[i].sound);

        for (int i = 0; i < clock->pomodoro_count; ++i)
            for (int j = 0; j < clock->pomodoros[i].segment_count; ++j)
                clk_audio_destroy(clock->pomodoros[i].segments[j].sound);
    }

    clk_audio_shutdown(engine);
}
