#ifndef CLK_ASCII_RENDER_H
#define CLK_ASCII_RENDER_H

#include <stdbool.h>
#include <stddef.h>

#include "clk_term.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------
 *  Types
 * ------------------------------------------------------------------ */

typedef struct {
    char        key[5];
    clk_texture texture;
} clk_glyph;

typedef struct {
    clk_glyph*  glyphs;
    int         glyph_count;

    clk_sprite** sprites;
    size_t sprite_count;
    size_t sprite_capacity;

    int glyph_spacing;
    int line_spacing;
    int z_order;
    int pos_x, pos_y;

    char* font_path;
} clk_ascii_render;

/* ------------------------------------------------------------------
 *  Lifecycle
 * ------------------------------------------------------------------ */

/** Create a renderer from a JSON font file.  Allocates glyph textures,
 *  parses the font config, and builds the sprite table.  Returns false
 *  if any step fails — clk_ascii_render_destroy() is safe to call
 *  regardless. */
bool clk_ascii_render_create(clk_ascii_render* render, const char* font_path);

/** Release all resources.  NULL-safe and idempotent. */
void clk_ascii_render_destroy(clk_ascii_render* render);

/** Re-parse the font JSON and rebuild glyph textures in place.
 *  Sprite pointers stay valid — only texture data is replaced. */
bool clk_ascii_render_reload(clk_ascii_render* render);

/* ------------------------------------------------------------------
 *  Runtime
 * ------------------------------------------------------------------ */

/** Assign glyph textures to sprites according to the pre-formatted
 *  string.  Each character in the string is looked up in the glyph
 *  table; characters without a matching glyph are treated as blank.
 *
 *  Handles multi-line strings (\\n). */
void clk_ascii_render_update(clk_ascii_render* render, const char* string);

/* ------------------------------------------------------------------
 *  Configuration
 * ------------------------------------------------------------------ */

/** Replace the font config file and reload. Returns false on failure. */
bool clk_ascii_render_change_font(clk_ascii_render* render, const char* new_font_path);

/* ------------------------------------------------------------------
 *  Query
 * ------------------------------------------------------------------ */

/** Return the total pixel size the rendered string occupies on screen.
 *  Computed from the string's geometry. */
bool clk_ascii_render_get_size(const clk_ascii_render* render, const char* string,
                               int* width, int* height);

/** Return the dimensions of a single glyph texture. */
bool clk_ascii_render_get_glyph_size(const clk_ascii_render* render, int* width, int* height);

/** Read / write the top-left screen position. */
bool clk_ascii_render_get_pos(const clk_ascii_render* render, int* px, int* py);
void clk_ascii_render_set_pos(clk_ascii_render* render, int px, int py);

/** Set / get the z-order for all sprites at once. */
void clk_ascii_render_set_z_order(clk_ascii_render* render, int z);
int  clk_ascii_render_get_z_order(const clk_ascii_render* render);

/* ------------------------------------------------------------------
 *  Rendering
 * ------------------------------------------------------------------ */

/** Register all sprites with the term render list. */
bool clk_ascii_render_add_to_term(clk_ascii_render* render);

#ifdef __cplusplus
}
#endif

#endif
