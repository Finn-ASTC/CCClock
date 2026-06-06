#ifndef CLK_CLOCK_H
#define CLK_CLOCK_H

#include <stdbool.h>
#include <stddef.h>

#include "clk_term.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CLK_CLOCK_TIME_FORMAT_MAX_LENGTH (64)
#define CLK_CLOCK_NUM_TEXTURE_COUNT (11) /* '0'–'9' + ':' */

/* ------------------------------------------------------------------
 *  Types
 * ------------------------------------------------------------------ */

typedef struct {
    char clk_clock_time_format[CLK_CLOCK_TIME_FORMAT_MAX_LENGTH];
    char* clk_clock_font_path;

    clk_texture clk_clock_num_font_texture[CLK_CLOCK_NUM_TEXTURE_COUNT];
    clk_sprite** clk_clock_sprites;
    size_t clk_clock_sprite_count;
    size_t clk_clock_sprite_capacity;

    int clk_clock_glyph_spacing;
    int clk_clock_line_spacing;
    int clk_clock_z_order;
    int posx, posy;
} clk_clock;

/* ------------------------------------------------------------------
 *  Lifecycle
 * ------------------------------------------------------------------ */

/** Create a clock from @p time_format and @p font_path (JSON config).
 *  Allocates glyph textures, parses the font, and builds the sprite
 *  table.  Returns false if any step fails — clk_clock_destroy() is
 *  safe to call regardless. */
bool clk_clock_create(clk_clock* clk, const char* time_format, const char* font_path);

/** Release all resources held by the clock.  NULL-safe and
 *  idempotent — safe to call more than once. */
void clk_clock_destroy(clk_clock* clk);

/** Re-parse the font JSON and rebuild glyph textures in-place.
 *  Sprite pointers stay valid — only the texture data is replaced. */
bool clk_clock_reload_config(clk_clock* clk);

/* ------------------------------------------------------------------
 *  Runtime
 * ------------------------------------------------------------------ */

/** Format the current time according to the clock's time_format and
 *  assign glyph textures to sprites.  Handles multi-line formats
 *  (\\n) and literal spacing. */
void clk_clock_update(clk_clock* clk);

/* ------------------------------------------------------------------
 *  Configuration
 * ------------------------------------------------------------------ */

bool clk_clock_change_time_format(clk_clock* clk, const char* new_format);
bool clk_clock_change_font_path(clk_clock* clk, const char* new_path);

/* ------------------------------------------------------------------
 *  Query
 * ------------------------------------------------------------------ */

/** Return the total pixel size of the rendered clock on screen. */
bool clk_clock_get_clock_size(const clk_clock* clk, int* w, int* h);

/** Return the dimensions of a single glyph texture. */
bool clk_clock_get_font_texture_size(const clk_clock* clk, int* w, int* h);

/** Read / write the top-left screen position of the entire clock. */
bool clk_clock_get_sprite_pos(const clk_clock* clk, int* px, int* py);
void clk_clock_set_sprite_pos(clk_clock* clk, int px, int py);

/** Set the z-order for all clock sprites at once. */
void clk_clock_set_z_order(clk_clock* clk, int z);

/** Get the current z-order of the clock. */
int clk_clock_get_z_order(const clk_clock* clk);

/* ------------------------------------------------------------------
 *  Rendering
 * ------------------------------------------------------------------ */

/** Register all sprites with the term render list.  Call once after
 *  clk_clock_create(). */
bool clk_clock_add_to_term(clk_clock* clk);

#ifdef __cplusplus
}
#endif

#endif /* CLK_CLOCK_H */
