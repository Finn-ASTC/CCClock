#ifndef CLK_TERM_H
#define CLK_TERM_H

#include <stdbool.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------
 *  ANSI text attribute flags (bitmask)
 * ------------------------------------------------------------------ */

#define ATTR_NONE 0x00
#define ATTR_BOLD 0x01      /* ANSI 1  */
#define ATTR_DIM 0x02       /* ANSI 2  */
#define ATTR_ITALIC 0x04    /* ANSI 3  */
#define ATTR_UNDERLINE 0x08 /* ANSI 4  */
#define ATTR_BLINK 0x10     /* ANSI 5  */
#define ATTR_REVERSE 0x20   /* ANSI 7  */
#define ATTR_HIDDEN 0x40    /* ANSI 8  */
#define ATTR_STRIKE 0x80    /* ANSI 9  */

/* ------------------------------------------------------------------
 *  Types
 * ------------------------------------------------------------------ */

/** 24-bit RGB colour with optional alpha. Access channels via .rgb
 *  or the whole word via .raw for fast comparisons. */
typedef union {
    struct {
        uint8_t r;
        uint8_t g;
        uint8_t b;
    } rgb;
    uint32_t raw;
} Color24;

/** A registered style: foreground / background colour + text attrs. */
typedef struct {
    Color24 fg_color;
    Color24 bg_color;
    uint8_t attrs;
} clk_style;

/** Cell type for wide-character support. */
typedef enum {
    CELL_NORMAL = 0, /* regular single-width cell */
    CELL_WIDE_LEAD,  /* left half of a double-width character */
    CELL_WIDE_TRAIL  /* right half — never rendered alone */
} clk_cell_type;

/** A single screen cell — up to 4 UTF-8 bytes + a style reference. */
typedef struct {
    char cell_tex[5];   /* null-terminated UTF-8 character */
    int style_id;       /* 0 = default (no SGR output) */
    clk_cell_type type; /* CELL_NORMAL / WIDE_LEAD / WIDE_TRAIL */
    bool is_empty;      /* true if this cell should not be rendered */
} clk_cell;

/** A pure data block — a 2D array of cells, no screen coordinates. */
typedef struct {
    int tex_w, tex_h; /* dimensions in cells */
    clk_cell* data;   /* w×h array, row-major */
    bool owns_data;   /* true = destroy() will free(data) */
} clk_texture;

/** A texture instance placed on screen at a specific position.
 *  Multiple sprites can reference the same clk_texture. */
typedef struct clk_sprite {
    clk_texture* tex;       /* referenced texture (not owned); may be NULL */
    int posx, posy;         /* screen coordinates */
    int z_order;            /* higher = on top */
    bool is_invalid;        /* permanently dead — compact() will remove */
    bool is_hidden;         /* alive but skip rendering this frame */
} clk_sprite;

/* ------------------------------------------------------------------
 *  Terminal lifecycle
 * ------------------------------------------------------------------ */

/** Initialise terminal: keyboard input, screen buffer, style registry.
 *  @return true on success. Safe to call more than once. */
bool clk_term_init(void);

/** Release all terminal resources and restore cursor visibility. */
void clk_term_close(void);

/* ------------------------------------------------------------------
 *  Styles
 * ------------------------------------------------------------------ */

/** Register a style (fg colour, bg colour, attrs bitmask).
 *  @return Positive style ID on success, or 0 on failure.
 *          If the same style was already registered its existing ID is
 *          returned (dedup). */
int clk_term_register_style(Color24 fg, Color24 bg, uint8_t attrs);

/** Convenience: register a style from raw RGB ints (0–255) and an
 *  attribute string ("bold", "dim" etc.). Returns 0 on invalid
 *  colour range or if the term is not initialised. */
int clk_term_register_style_rgb(int fg_r, int fg_g, int fg_b, int bg_r, int bg_g, int bg_b,
                                const char* attrs_str);

/** Register a style from "#RRGGBB" hex colour strings.
 *  Convenience wrapper around parse_hex_color + register_style_rgb.
 *  Returns 0 on invalid format. */
int clk_term_register_style_hex(const char* fg_hex, const char* bg_hex, const char* attrs_str);

/** Parse a "#RRGGBB" hex colour string into r/g/b components (0–255).
 *  Returns false if the string is NULL or does not match the format. */
bool clk_term_parse_hex_color(const char* hex, int* r, int* g, int* b);

/* ------------------------------------------------------------------
 *  Texture — data
 * ------------------------------------------------------------------ */

/** Create a texture of @p w × @p h cells, all initially empty.
 *  The data is owned by the texture — clk_texture_destroy() will free it. */
clk_texture clk_texture_create(int w, int h);

/** Initialise a texture that borrows an existing cell array.
 *  The data pointer is NOT freed by clk_texture_destroy().
 *  Caller retains ownership and must free @p data when no longer needed. */
void clk_texture_init_borrowed(clk_texture* tex, int w, int h, clk_cell* data);

/** Free texture cell data (only if owned). Safe to call more than once. */
void clk_texture_destroy(clk_texture* tex);

/** Write character @p ch with style @p style_id into cell (x,y).
 *  The cell type is set to CELL_NORMAL. */
void clk_texture_write_cell(clk_texture* tex, int x, int y, const char* ch, int style_id);

/** Write a double-width character as two adjacent cells: LEAD at
 *  (x,y) and TRAIL at (x+1,y). Rejected if x+1 is out of bounds. */
void clk_texture_write_wide_cell(clk_texture* tex, int x, int y, const char* ch, int style_id);

/** Copy an entire clk_cell into (x,y). Useful when the cell has
 *  already been fully constructed (e.g. from a font glyph table). */
void clk_texture_set_cell(clk_texture* tex, int x, int y, const clk_cell* cell);

/** Fill a rectangular region with the same character + style. */
void clk_texture_fill_rect(clk_texture* tex, int x, int y, int w, int h, const char* ch,
                           int style_id);

/** Write a string horizontally, one cell per character.
 *  Handles multi-byte UTF-8 correctly. */
void clk_texture_write_string(clk_texture* tex, int x, int y, const char* str, int style_id);

/** Mark a single cell as empty (skipped during rendering). */
void clk_texture_clear_cell(clk_texture* tex, int x, int y);

/** Mark every cell in the texture as empty. */
void clk_texture_clear_all(clk_texture* tex);

/** Return a read-only pointer to cell (x,y), or NULL if out of bounds. */
const clk_cell* clk_texture_get_cell(const clk_texture* tex, int x, int y);

/* ------------------------------------------------------------------
 *  Sprite — texture with a screen position
 * ------------------------------------------------------------------ */

/** Set the z-order of a sprite. Marks the render list as needing
 *  re-sort before the next clk_term_draw(). */
void clk_sprite_set_z(clk_sprite* s, int z);

/** Allocate a sprite with no texture (tex = NULL). The sprite is not
 *  yet added to the render list — use clk_term_add_sprite() when ready. */
clk_sprite* clk_sprite_create(void);

/** Allocate a sprite and attach @p tex at the given screen position and
 *  z-order. The sprite is not yet added to the render list. */
clk_sprite* clk_sprite_create_with_texture(clk_texture* tex, int x, int y, int z);

/** Remove the sprite from the render list (if present) and free it. */
void clk_sprite_destroy(clk_sprite* s);

/** Bind a texture to an existing sprite. Does nothing if @p s is NULL. */
void clk_sprite_set_texture(clk_sprite* s, clk_texture* tex);

/** Detach the texture from @p s (sets tex = NULL). */
void clk_sprite_remove_texture(clk_sprite* s);

/* ------------------------------------------------------------------
 *  Render list
 * ------------------------------------------------------------------ */

/** Register a sprite for rendering. It will be drawn on the next
 *  clk_term_draw() call. The sprite's texture must remain valid until
 *  the sprite is removed or the term is closed. */
void clk_term_add_sprite(clk_sprite* sprite);

/** Remove @p sprite from the render list. Does nothing if not found. */
void clk_term_remove_sprite(const clk_sprite* sprite);

/** Remove all sprites from the render list at once. */
void clk_term_clear_sprites(void);

/* ------------------------------------------------------------------
 *  Rendering
 * ------------------------------------------------------------------ */

/** Composite all registered sprites into one frame and flush ANSI
 *  output to stdout. Only changed cells are re-drawn (diff-based). */
void clk_term_draw(void);

/** Check terminal dimensions and handle resize if needed. Also
 *  compacts the render list by removing invalid sprites. */
bool clk_term_update(void);

/** Detect terminal size change and resize internal buffers
 *  accordingly. Does NOT compact the sprite render list. */
void clk_term_resize(void);

/** Remove invalid sprites from the render list. Call this each frame
 *  after sprites may have been invalidated (e.g. format change). */
void clk_term_compact(void);

/** Query current terminal width and height (in character cells). */
bool clk_term_get_size(int* term_w, int* term_h);

/** Return true if the terminal size differs from the last known size. */
bool clk_term_size_changed(void);

/** Suspend execution for @p ms milliseconds. Cross-platform. */
void clk_term_sleep_ms(int ms);

#ifdef __cplusplus
}
#endif

#endif /* CLK_TERM_H */
