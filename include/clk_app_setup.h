#ifndef CLK_APP_SETUP_H
#define CLK_APP_SETUP_H

#include <stdbool.h>

#include "clk_app_config.h"
#include "clk_ascii_render.h"
#include "clk_audio.h"
#include "clk_clock.h"
#include "clk_menu.h"
#include "clk_menu_theme.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Tab IDs */
#define CLK_TAB_BASIC 0
#define CLK_TAB_ALARM 1
#define CLK_TAB_POMODORO 2

/* Basic tab item IDs */
#define CLK_BASIC_ITEM_TIME_FORMAT 1
#define CLK_BASIC_ITEM_FONT 2
#define CLK_BASIC_ITEM_THEME 3
#define CLK_BASIC_ITEM_QUIT 4

/* ── Alarm tab item ID encoding ── */
#define CLK_ALARM_ITEM_STRIDE 16

#define CLK_ALARM_HEADER_OFFSET 0
#define CLK_ALARM_ENABLED_OFFSET 1
#define CLK_ALARM_HOUR_OFFSET 2
#define CLK_ALARM_MINUTE_OFFSET 3
#define CLK_ALARM_REPEAT_OFFSET 4
#define CLK_ALARM_LOOP_OFFSET 5
#define CLK_ALARM_REPEAT_COUNT 6
#define CLK_ALARM_VOLUME_OFFSET 7
#define CLK_ALARM_SOUND_OFFSET 8
#define CLK_ALARM_ADD_OFFSET 9
#define CLK_ALARM_DELETE_OFFSET 10

#define CLK_ALARM_DECODE(item_id, al_id, off) \
    ((al_id) = (item_id) / CLK_ALARM_ITEM_STRIDE, (off) = (item_id) % CLK_ALARM_ITEM_STRIDE)

/* ── Pomodoro tab item ID encoding ── */
#define CLK_POMO_STRIDE 256
#define CLK_POMO_SEGMENT_BASE 64
#define CLK_POMO_SEG_STRIDE 16

#define CLK_POMO_HEADER_OFFSET 0
#define CLK_POMO_ENABLED_OFFSET 1
#define CLK_POMO_ADD_OFFSET 3
#define CLK_POMO_DELETE_OFFSET 4

#define CLK_POMO_SEG_HEADER_OFFSET 0
#define CLK_POMO_SEG_DURATION_OFFSET 1
#define CLK_POMO_SEG_REPEAT_OFFSET 2
#define CLK_POMO_SEG_VOLUME_OFFSET 3
#define CLK_POMO_SEG_SOUND_OFFSET 4
#define CLK_POMO_SEG_ADD_OFFSET 5
#define CLK_POMO_SEG_DELETE_OFFSET 6

#define CLK_POMO_DECODE(item_id, po_id, off) \
    ((po_id) = (item_id) / CLK_POMO_STRIDE, (off) = (item_id) % CLK_POMO_STRIDE)

#define CLK_POMO_SEG_DECODE(pomo_offset, seg_id, field)                        \
    ((seg_id) = ((pomo_offset) - CLK_POMO_SEGMENT_BASE) / CLK_POMO_SEG_STRIDE, \
     (field) = ((pomo_offset) - CLK_POMO_SEGMENT_BASE) % CLK_POMO_SEG_STRIDE)

#define CLK_REPEAT_DAY_OPTION_COUNT 9
extern const char* clk_repeat_day_options[];

/** Create a menu and populate all tabs + items from @p cfg and @p clock.
 *  Returns NULL on allocation failure.  Caller owns the returned
 *  clk_menu* and must free it via clk_menu_destroy. */
clk_menu* clk_app_setup_menu(const clk_app_config* cfg, clk_clock* clock);

/** Create a renderer from the selected font in @p ascii_clock,
 *  set z_order to 0, and add it to the term sprite list.
 *  Returns false on allocation failure. */
bool clk_app_setup_render(clk_ascii_render* render, const clk_cfg_ascii_clock_theme* ascii_clock);

/** Load the theme file selected in @p themes (zero-initialises @p theme
 *  then calls clk_menu_theme_load).  Returns false if loading failed. */
bool clk_app_setup_theme(clk_menu_theme* theme, const clk_cfg_themes* themes);

bool clk_app_setup_clock(clk_clock* clock, clk_audio_engine** out_engine,
                         const clk_app_config* cfg);

void clk_app_setup_clock_deinit(clk_clock* clock, clk_audio_engine* engine);

void clk_app_menu_rebuild(clk_menu* menu, clk_clock* clock, const clk_app_config* cfg);

#ifdef __cplusplus
}
#endif

#endif
