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

/** A single screen cell — up to 4 UTF-8 bytes + a style reference. */
typedef struct {
    char cell_tex[5]; /* null-terminated UTF-8 character */
    int style_id;     /* 0 = default (no SGR output) */
    bool is_empty;    /* true if this cell should not be rendered */
} clk_cell;

/** A rectangular block of cells that can be positioned on screen. */
typedef struct {
    int posx, posy;
    int tex_w, tex_h;
    int tex_z_order; /* higher = on top */
    clk_cell* data;  /* w×h array, row-major */
    bool is_invalid; /* skip during rendering */
} clk_texture;

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

/* ------------------------------------------------------------------
 *  Texture lifecycle
 * ------------------------------------------------------------------ */

/** Create a texture of @p w × @p h cells, all initially empty.
 *  @return Texture with .data allocated, or {0} on failure. */
clk_texture clk_texture_create(int w, int h);

/** Free texture cell data. Safe to call more than once. */
void clk_texture_destroy(clk_texture* tex);

/* ------------------------------------------------------------------
 *  Texture — cell manipulation
 * ------------------------------------------------------------------ */

/** Write character @p ch with style @p style_id into cell (x,y). */
void clk_texture_set_cell(clk_texture* tex, int x, int y, const char* ch, int style_id);

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

/* ------------------------------------------------------------------
 *  Texture — layout
 * ------------------------------------------------------------------ */

void clk_texture_set_pos(clk_texture* tex, int x, int y);
void clk_texture_set_z_order(clk_texture* tex, int z);
void clk_texture_set_invalid(clk_texture* tex, bool invalid);
void clk_texture_set_pos_z(clk_texture* tex, int x, int y, int z);

/* ------------------------------------------------------------------
 *  Render list
 * ------------------------------------------------------------------ */

/** Register a texture for rendering. It will be drawn on the next
 *  clk_term_draw() call. Texture data must remain valid until it is
 *  removed or the term is closed. */
bool clk_term_add_texture(const clk_texture* texture);

/* ------------------------------------------------------------------
 *  Rendering
 * ------------------------------------------------------------------ */

/** Composite all registered textures into one frame and flush ANSI
 *  output to stdout. Only changed cells are re-drawn (diff-based). */
void clk_term_draw(void);

/** Check terminal dimensions and handle resize if needed. Also
 *  compacts the render list by removing invalid textures. */
bool clk_term_update(void);

/** Query current terminal width and height (in character cells). */
bool clk_term_get_size(int* term_w, int* term_h);

#ifdef __cplusplus
}
#endif

#endif /* CLK_TERM_H */
