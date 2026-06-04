#include "clk_term.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "clk_key_io.h"

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#else
#include <sys/ioctl.h>
#include <unistd.h>
#endif

#define CLK_SPRITE_LIST_DEFAULT_CAPACITY (16)
#define CLK_ANSI_OUTPUT_ESTIMATE_PER_CELL (30)
#define CLK_STYLE_DEFAULT_CAPACITY (16)

/* ------------------------------------------------------------------
 *  ANSI buffer formatting macros
 *
 *  NOTE: These macros 'return' from the enclosing function on
 *  overflow.  Only use them in void functions that hold no
 *  allocated resources that need explicit cleanup.
 * ------------------------------------------------------------------ */

#define APPENDF(buf, cap, len, ...)                          \
    do {                                                     \
        int _rem = (int)(cap) - (len);                       \
        if (_rem <= 0)                                       \
            return;                                          \
        int _n = snprintf((buf) + (len), _rem, __VA_ARGS__); \
        if (_n < 0 || _n >= _rem)                            \
            return;                                          \
        (len) += _n;                                         \
    } while (0)

#define APPENDC(buf, cap, len, ch)   \
    do {                             \
        if ((len) + 1 >= (int)(cap)) \
            return;                  \
        (buf)[(len)++] = (ch);       \
    } while (0)

/* ------------------------------------------------------------------
 *  Global state — all file-scope static
 * ------------------------------------------------------------------ */

static int clk_is_term_init = false;
static bool clk_is_sprite_list_sorted = false;

static clk_cell* screen_buffer;
static bool* if_rendered_sign;

static int screen_w, screen_h;
static int screen_size;

static char* ansi_output;
static int ansi_output_length;
static int ansi_output_capacity;

static clk_sprite** sprite_render_list;
static int sprite_list_count = 0;
static int sprite_list_capacity = CLK_SPRITE_LIST_DEFAULT_CAPACITY;

static clk_style* style_registry;
static int style_count = 0;
static int style_capacity = 0;

static const clk_style* clk_get_style(int style_id) {
    if (style_id <= 0 || style_id >= style_count)
        return NULL;
    return &style_registry[style_id];
}

/* ================================================================
 *  Terminal lifecycle
 * ================================================================ */

bool clk_term_init(void) {
    if (clk_is_term_init)
        return true;

    clk_key_io_init();

    int sw, sh;
    if (!clk_term_get_size(&sw, &sh))
        return false;
    if (sw <= 0 || sh <= 0)
        return false;
    screen_size = sw * sh;
    screen_w = sw;
    screen_h = sh;

    clk_cell empty_cell = {.is_empty = true};

    /* screen buffer */
    clk_cell* temp_s = malloc(screen_size * sizeof(clk_cell));
    if (!temp_s)
        return false;
    screen_buffer = temp_s;
    for (int i = 0; i < screen_size; ++i)
        screen_buffer[i] = empty_cell;

    /* sprite render list */
    clk_sprite** temp_l = malloc(sprite_list_capacity * sizeof(clk_sprite*));
    if (!temp_l) {
        free(screen_buffer);
        return false;
    }
    sprite_render_list = temp_l;
    for (int i = 0; i < sprite_list_capacity; ++i)
        sprite_render_list[i] = NULL;

    /* per-cell "already claimed" flags for z-order masking */
    bool* temp_sign = calloc(screen_size, sizeof(bool));
    if (!temp_sign) {
        free(screen_buffer);
        free(sprite_render_list);
        return false;
    }
    if_rendered_sign = temp_sign;

    /* ANSI output buffer */
    char* temp_a = calloc(1, screen_size * CLK_ANSI_OUTPUT_ESTIMATE_PER_CELL);
    if (!temp_a) {
        free(screen_buffer);
        free(sprite_render_list);
        free(if_rendered_sign);
        return false;
    }
    ansi_output = temp_a;
    ansi_output_length = 0;
    ansi_output_capacity = screen_size * CLK_ANSI_OUTPUT_ESTIMATE_PER_CELL;

    /* style registry — slot 0 is the "no-style" default */
    clk_style* temp_st = malloc(CLK_STYLE_DEFAULT_CAPACITY * sizeof(clk_style));
    if (!temp_st) {
        free(screen_buffer);
        free(sprite_render_list);
        free(if_rendered_sign);
        free(ansi_output);
        return false;
    }
    style_registry = temp_st;
    style_registry[0] = (clk_style){{.raw = 0}, {.raw = 0}, ATTR_NONE};
    style_count = 1;
    style_capacity = CLK_STYLE_DEFAULT_CAPACITY;

    clk_is_term_init = true;

    /* clear screen & hide cursor */
    printf("\033[2J\033[H\033[?25l");
    fflush(stdout);
    return true;
}

void clk_term_close(void) {
    if (!clk_is_term_init)
        return;

    clk_key_io_close();

    free(screen_buffer);
    screen_buffer = NULL;
    free(sprite_render_list);
    sprite_render_list = NULL;
    free(ansi_output);
    ansi_output = NULL;
    free(if_rendered_sign);
    if_rendered_sign = NULL;
    free(style_registry);
    style_registry = NULL;

    sprite_list_count = 0;
    sprite_list_capacity = CLK_SPRITE_LIST_DEFAULT_CAPACITY;
    style_count = 0;
    style_capacity = CLK_STYLE_DEFAULT_CAPACITY;
    ansi_output_length = 0;
    ansi_output_capacity = 0;
    screen_w = screen_h = screen_size = 0;

    clk_is_term_init = false;

    /* show cursor */
    printf("\033[?25h");
    fflush(stdout);
}

/* ================================================================
 *  Style registry
 * ================================================================ */

int clk_term_register_style(Color24 fg, Color24 bg, uint8_t attrs) {
    if (!clk_is_term_init)
        return 0;

    for (int i = 1; i < style_count; ++i) {
        if (style_registry[i].fg_color.raw == fg.raw && style_registry[i].bg_color.raw == bg.raw &&
            style_registry[i].attrs == attrs)
            return i;
    }

    if (style_count + 1 > style_capacity) {
        int new_cap = style_capacity * 2;
        clk_style* temp = realloc(style_registry, new_cap * sizeof(clk_style));
        if (!temp)
            return 0;
        style_registry = temp;
        style_capacity = new_cap;
    }
    style_registry[style_count] = (clk_style){fg, bg, attrs};
    return style_count++;
}

/** Parse an attribute string into an ATTR_* bitmask. */
static uint8_t parse_attrs_str(const char* str) {
    if (!str)
        return ATTR_NONE;
    uint8_t attrs = ATTR_NONE;
    if (strstr(str, "bold"))
        attrs |= ATTR_BOLD;
    if (strstr(str, "dim"))
        attrs |= ATTR_DIM;
    if (strstr(str, "italic"))
        attrs |= ATTR_ITALIC;
    if (strstr(str, "underline"))
        attrs |= ATTR_UNDERLINE;
    if (strstr(str, "blink"))
        attrs |= ATTR_BLINK;
    if (strstr(str, "reverse"))
        attrs |= ATTR_REVERSE;
    if (strstr(str, "hidden"))
        attrs |= ATTR_HIDDEN;
    if (strstr(str, "strike"))
        attrs |= ATTR_STRIKE;
    return attrs;
}

int clk_term_register_style_rgb(int fg_r, int fg_g, int fg_b, int bg_r, int bg_g, int bg_b,
                                const char* attrs_str) {
    if (fg_r < 0 || fg_r > 255 || fg_g < 0 || fg_g > 255 || fg_b < 0 || fg_b > 255 || bg_r < 0 ||
        bg_r > 255 || bg_g < 0 || bg_g > 255 || bg_b < 0 || bg_b > 255)
        return 0;

    Color24 fg = {.rgb = {(uint8_t)fg_r, (uint8_t)fg_g, (uint8_t)fg_b}};
    Color24 bg = {.rgb = {(uint8_t)bg_r, (uint8_t)bg_g, (uint8_t)bg_b}};
    uint8_t attrs = parse_attrs_str(attrs_str);
    return clk_term_register_style(fg, bg, attrs);
}

int clk_term_register_style_hex(const char* fg_hex, const char* bg_hex, const char* attrs_str) {
    int fg_r, fg_g, fg_b, bg_r, bg_g, bg_b;
    if (!clk_term_parse_hex_color(fg_hex, &fg_r, &fg_g, &fg_b) ||
        !clk_term_parse_hex_color(bg_hex, &bg_r, &bg_g, &bg_b))
        return 0;
    return clk_term_register_style_rgb(fg_r, fg_g, fg_b, bg_r, bg_g, bg_b, attrs_str);
}

bool clk_term_parse_hex_color(const char* hex, int* r, int* g, int* b) {
    if (!hex || !r || !g || !b)
        return false;
    if (hex[0] != '#')
        return false;
    unsigned int ri, gi, bi;
    if (sscanf(hex + 1, "%2x%2x%2x", &ri, &gi, &bi) != 3)
        return false;
    *r = (int)ri;
    *g = (int)gi;
    *b = (int)bi;
    return true;
}

/* ================================================================
 *  Sprite / render list
 * ================================================================ */

static int cmp_sprite_zorder(const void* s1, const void* s2) {
    const clk_sprite* a = *(const clk_sprite**)s1;
    const clk_sprite* b = *(const clk_sprite**)s2;
    if (a->z_order != b->z_order)
        return (b->z_order > a->z_order) - (b->z_order < a->z_order);
    return 0;
}

void clk_sprite_set_z(clk_sprite* s, int z) {
    if (!s)
        return;
    s->z_order = z;
    clk_is_sprite_list_sorted = false;
}

void clk_term_add_sprite(clk_sprite* sprite) {
    if (!clk_is_term_init || !sprite)
        return;

    int count = sprite_list_count + 1;
    if (count > sprite_list_capacity) {
        int new_capacity = sprite_list_capacity * 2;
        clk_sprite** temp = realloc(sprite_render_list, new_capacity * sizeof(clk_sprite*));
        if (!temp)
            return;
        sprite_render_list = temp;
        for (int i = sprite_list_capacity; i < new_capacity; ++i)
            sprite_render_list[i] = NULL;
        sprite_list_capacity = new_capacity;
    }

    sprite_render_list[sprite_list_count] = sprite;
    sprite_list_count = count;
    clk_is_sprite_list_sorted = false;
}

void clk_term_remove_sprite(const clk_sprite* sprite) {
    if (!sprite || !clk_is_term_init)
        return;

    for (int i = 0; i < sprite_list_count; ++i) {
        if (sprite_render_list[i] == sprite) {
            for (int j = i; j < sprite_list_count - 1; ++j)
                sprite_render_list[j] = sprite_render_list[j + 1];
            sprite_render_list[sprite_list_count - 1] = NULL;
            sprite_list_count--;
            return;
        }
    }
}

void clk_term_clear_sprites(void) {
    if (!clk_is_term_init)
        return;
    for (int i = 0; i < sprite_list_count; ++i)
        sprite_render_list[i] = NULL;
    sprite_list_count = 0;
    clk_is_sprite_list_sorted = true;
}

/* ================================================================
 *  Rendering helpers
 * ================================================================ */

static bool if_cell_equal(const clk_cell* c1, const clk_cell* c2) {
    if (c1 == c2)
        return true;
    if (!c1 || !c2)
        return false;
    return strcmp(c1->cell_tex, c2->cell_tex) == 0 && c1->style_id == c2->style_id &&
           c1->type == c2->type && c1->is_empty == c2->is_empty;
}

static void clk_add_cell_to_ansi_output(const clk_cell* cell, int x, int y) {
    if (!clk_is_term_init)
        return;
    if (!cell || cell->is_empty)
        return;

    const clk_style* style = clk_get_style(cell->style_id);
    char buf[256];
    int len = 0;

    APPENDF(buf, sizeof(buf), len, "\033[%d;%dH", y + 1, x + 1);

    if (style) {
        int params[16], pcount = 0;
        if (style->attrs & ATTR_BOLD)
            params[pcount++] = 1;
        if (style->attrs & ATTR_DIM)
            params[pcount++] = 2;
        if (style->attrs & ATTR_ITALIC)
            params[pcount++] = 3;
        if (style->attrs & ATTR_UNDERLINE)
            params[pcount++] = 4;
        if (style->attrs & ATTR_BLINK)
            params[pcount++] = 5;
        if (style->attrs & ATTR_REVERSE)
            params[pcount++] = 7;
        if (style->attrs & ATTR_HIDDEN)
            params[pcount++] = 8;
        if (style->attrs & ATTR_STRIKE)
            params[pcount++] = 9;

        bool has_fg = style->fg_color.raw != 0;
        bool has_bg = style->bg_color.raw != 0;

        if (pcount > 0 || has_fg || has_bg) {
            APPENDF(buf, sizeof(buf), len, "\033[");
            for (int i = 0; i < pcount; i++)
                APPENDF(buf, sizeof(buf), len, "%s%d", i > 0 ? ";" : "", params[i]);
            if (has_fg) {
                APPENDF(buf, sizeof(buf), len, "%s38;2;%d;%d;%d", pcount > 0 ? ";" : "",
                        style->fg_color.rgb.r, style->fg_color.rgb.g, style->fg_color.rgb.b);
                pcount++;
            }
            if (has_bg) {
                APPENDF(buf, sizeof(buf), len, "%s48;2;%d;%d;%d", pcount > 0 ? ";" : "",
                        style->bg_color.rgb.r, style->bg_color.rgb.g, style->bg_color.rgb.b);
            }
            APPENDF(buf, sizeof(buf), len, "m");
        }
    }

    for (int i = 0; i < 5 && cell->cell_tex[i] != '\0'; i++)
        APPENDC(buf, sizeof(buf), len, cell->cell_tex[i]);

    APPENDF(buf, sizeof(buf), len, "\033[0m");

    if (ansi_output_length + len > ansi_output_capacity) {
        int new_cap = ansi_output_capacity * 2;
        while (new_cap < ansi_output_length + len)
            new_cap *= 2;
        char* temp = realloc(ansi_output, new_cap);
        if (!temp)
            return;
        ansi_output = temp;
        ansi_output_capacity = new_cap;
    }

    memcpy(ansi_output + ansi_output_length, buf, len);
    ansi_output_length += len;
}

/* ================================================================
 *  Frame rendering (diff-based)
 * ================================================================ */

void clk_term_draw(void) {
    if (!clk_is_term_init)
        return;

    for (int i = 0; i < screen_size; ++i)
        if_rendered_sign[i] = 0;

    if (!clk_is_sprite_list_sorted) {
        qsort(sprite_render_list, sprite_list_count, sizeof(clk_sprite*), cmp_sprite_zorder);
        clk_is_sprite_list_sorted = true;
    }

    for (int i = 0; i < sprite_list_count; ++i) {
        const clk_sprite* s = sprite_render_list[i];
        if (!s || s->is_invalid || !s->tex || !s->tex->data)
            continue;

        int pos_x = s->posx, pos_y = s->posy;
        int tw = s->tex->tex_w, th = s->tex->tex_h;

        for (int ty = 0; ty < th; ++ty) {
            for (int tx = 0; tx < tw; ++tx) {
                int x = tx + pos_x;
                int y = ty + pos_y;
                if (x < 0 || x >= screen_w || y < 0 || y >= screen_h)
                    continue;

                int idx = x + y * screen_w;
                const clk_cell* cell = &s->tex->data[tx + ty * tw];

                if (cell->type == CELL_WIDE_TRAIL)
                    continue;
                if (cell->type == CELL_WIDE_LEAD) {
                    const clk_cell* next = (tx + 1 < tw) ? &s->tex->data[tx + 1 + ty * tw] : NULL;
                    if (!next || next->type != CELL_WIDE_TRAIL)
                        continue;
                }

                bool blocked = if_rendered_sign[idx];
                if (cell->type == CELL_WIDE_LEAD && x + 1 < screen_w)
                    blocked = blocked || if_rendered_sign[idx + 1];
                if (blocked || cell->is_empty)
                    continue;

                if (!if_cell_equal(cell, &screen_buffer[idx])) {
                    clk_add_cell_to_ansi_output(cell, x, y);
                    screen_buffer[idx] = *cell;
                }

                if_rendered_sign[idx] = 1;
                if (cell->type == CELL_WIDE_LEAD && x + 1 < screen_w)
                    if_rendered_sign[idx + 1] = 1;
            }
        }
    }

    for (int i = 0; i < screen_size; ++i) {
        if (!if_rendered_sign[i] && !screen_buffer[i].is_empty) {
            int x = i % screen_w, y = i / screen_w;
            clk_cell clear = {.cell_tex = {' ', '\0', 0, 0, 0},
                              .style_id = 0,
                              .type = CELL_NORMAL,
                              .is_empty = false};
            clk_add_cell_to_ansi_output(&clear, x, y);
            screen_buffer[i] = (clk_cell){.type = CELL_NORMAL, .is_empty = true};
        }
    }

    if (ansi_output_length > 0) {
        fwrite(ansi_output, 1, ansi_output_length, stdout);
        fflush(stdout);
        ansi_output_length = 0;
    }
}

/* ================================================================
 *  Terminal resize
 * ================================================================ */

bool clk_resize_term(int new_w, int new_h) {
    if (!clk_is_term_init)
        return false;

    int new_size = new_w * new_h;
    int new_cap = new_size * CLK_ANSI_OUTPUT_ESTIMATE_PER_CELL;

    clk_cell* new_buf = malloc(new_size * sizeof(clk_cell));
    bool* new_sign = malloc(new_size * sizeof(bool));
    char* new_ansi = malloc(new_cap);

    if (!new_buf || !new_sign || !new_ansi) {
        free(new_buf);
        free(new_sign);
        free(new_ansi);
        return false;
    }

    memset(new_sign, 0, new_size * sizeof(bool));

    int copy_count = screen_size < new_size ? screen_size : new_size;
    memcpy(new_buf, screen_buffer, copy_count * sizeof(clk_cell));

    clk_cell empty = {.is_empty = true};
    for (int i = screen_size; i < new_size; ++i)
        new_buf[i] = empty;

    free(screen_buffer);
    screen_buffer = new_buf;
    free(if_rendered_sign);
    if_rendered_sign = new_sign;
    free(ansi_output);
    ansi_output = new_ansi;

    screen_w = new_w;
    screen_h = new_h;
    screen_size = new_size;
    ansi_output_capacity = new_cap;

    printf("\033[2J\033[H");
    fflush(stdout);
    return true;
}

bool clk_term_update(void) {
    if (!clk_is_term_init)
        return false;

    int new_w, new_h;
    if (!clk_term_get_size(&new_w, &new_h))
        return false;
    if (new_w <= 0 || new_h <= 0)
        return false;

    if (new_w != screen_w || new_h != screen_h) {
        if (!clk_resize_term(new_w, new_h))
            return false;
    }

    int write = 0;
    for (int read = 0; read < sprite_list_count; ++read) {
        clk_sprite* s = sprite_render_list[read];
        if (s && !s->is_invalid)
            sprite_render_list[write++] = s;
    }
    for (int i = write; i < sprite_list_count; ++i)
        sprite_render_list[i] = NULL;
    sprite_list_count = write;

    return true;
}

bool clk_term_get_size(int* term_w, int* term_h) {
    if (!term_w || !term_h)
        return false;
#if defined(_WIN32) || defined(_WIN64)
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hConsole == INVALID_HANDLE_VALUE)
        return false;
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (GetConsoleScreenBufferInfo(hConsole, &csbi)) {
        *term_w = csbi.srWindow.Right - csbi.srWindow.Left + 1;
        *term_h = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
        return true;
    }
    return false;
#else
    struct winsize w;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0) {
        *term_w = w.ws_col;
        *term_h = w.ws_row;
        return true;
    }
    return false;
#endif
}

bool clk_term_size_changed(void) {
    int w, h;
    if (!clk_term_get_size(&w, &h))
        return false;
    return (w != screen_w || h != screen_h);
}

void clk_term_sleep_ms(int ms) {
    if (ms <= 0)
        return;
#if defined(_WIN32) || defined(_WIN64)
    Sleep(ms);
#else
    usleep(ms * 1000);
#endif
}

/* ================================================================
 *  Texture lifecycle
 * ================================================================ */

clk_texture clk_texture_create(int w, int h) {
    clk_texture tex = {0};
    if (w <= 0 || h <= 0)
        return tex;

    clk_cell* data = malloc(w * h * sizeof(clk_cell));
    if (!data)
        return tex;

    clk_cell empty = {.is_empty = true, .style_id = 0, .type = CELL_NORMAL};
    for (int i = 0; i < w * h; ++i)
        data[i] = empty;

    tex.tex_w = w;
    tex.tex_h = h;
    tex.data = data;
    tex.owns_data = true;
    return tex;
}

void clk_texture_init_borrowed(clk_texture* tex, int w, int h, clk_cell* data) {
    if (!tex || !data || w <= 0 || h <= 0)
        return;
    memset(tex, 0, sizeof(*tex));
    tex->tex_w = w;
    tex->tex_h = h;
    tex->data = data;
    tex->owns_data = false;
}

void clk_texture_destroy(clk_texture* tex) {
    if (!tex || !tex->data)
        return;
    if (tex->owns_data)
        free(tex->data);
    tex->data = NULL;
    tex->tex_w = 0;
    tex->tex_h = 0;
}

/* ================================================================
 *  Texture — cell manipulation
 * ================================================================ */

void clk_texture_write_cell(clk_texture* tex, int x, int y, const char* ch, int style_id) {
    if (!tex || !tex->data || x < 0 || x >= tex->tex_w || y < 0 || y >= tex->tex_h)
        return;

    clk_cell* cell = &tex->data[x + y * tex->tex_w];
    int i = 0;
    while (i < 4 && ch && ch[i]) {
        cell->cell_tex[i] = ch[i];
        ++i;
    }
    cell->cell_tex[i] = '\0';
    cell->style_id = style_id;
    cell->type = CELL_NORMAL;
    cell->is_empty = false;
}

void clk_texture_write_wide_cell(clk_texture* tex, int x, int y, const char* ch, int style_id) {
    if (!tex || !tex->data || !ch || x < 0 || x + 1 >= tex->tex_w ||
        y < 0 || y >= tex->tex_h)
        return;

    clk_cell cell = {.style_id = style_id, .type = CELL_WIDE_LEAD, .is_empty = false};
    int i = 0;
    while (i < 4 && ch[i]) cell.cell_tex[i] = ch[i], ++i;
    cell.cell_tex[i] = '\0';
    clk_texture_set_cell(tex, x, y, &cell);
}

void clk_texture_set_cell(clk_texture* tex, int x, int y, const clk_cell* cell) {
    if (!tex || !tex->data || !cell ||
        x < 0 || x >= tex->tex_w || y < 0 || y >= tex->tex_h)
        return;

    /* TRAIL cells are created automatically by LEAD — never set directly */
    if (cell->type == CELL_WIDE_TRAIL)
        return;

    tex->data[x + y * tex->tex_w] = *cell;

    /* LEAD automatically writes a TRAIL at x+1 */
    if (cell->type == CELL_WIDE_LEAD && x + 1 < tex->tex_w) {
        clk_cell trail = {.type = CELL_WIDE_TRAIL, .is_empty = false};
        tex->data[x + 1 + y * tex->tex_w] = trail;
    }
}

static int clk_cell_char_width(const char* utf8) {
    if (!utf8 || !utf8[0])
        return 0;
    unsigned char c0 = (unsigned char)utf8[0];
    unsigned int cp;
    int len;
    if ((c0 & 0x80) == 0) {
        cp = c0;
        len = 1;
    } else if ((c0 & 0xE0) == 0xC0) {
        cp = c0 & 0x1F;
        len = 2;
    } else if ((c0 & 0xF0) == 0xE0) {
        cp = c0 & 0x0F;
        len = 3;
    } else if ((c0 & 0xF8) == 0xF0) {
        cp = c0 & 0x07;
        len = 4;
    } else
        return 1;
    for (int i = 1; i < len; i++) {
        unsigned char cb = (unsigned char)utf8[i];
        if ((cb & 0xC0) != 0x80)
            return 1;
        cp = (cp << 6) | (cb & 0x3F);
    }
    if (cp >= 0x4E00 && cp <= 0x9FFF)
        return 2;
    if (cp >= 0x3400 && cp <= 0x4DBF)
        return 2;
    if (cp >= 0xF900 && cp <= 0xFAFF)
        return 2;
    if (cp >= 0xFF01 && cp <= 0xFF60)
        return 2;
    if (cp >= 0xFFE0 && cp <= 0xFFE6)
        return 2;
    if (cp >= 0x1F300 && cp <= 0x1F9FF)
        return 2;
    if (cp >= 0x20000 && cp <= 0x2EBE0)
        return 2;
    return 1;
}

void clk_texture_fill_rect(clk_texture* tex, int x, int y, int w, int h, const char* ch,
                           int style_id) {
    if (!tex || !tex->data || w <= 0 || h <= 0)
        return;
    for (int dy = 0; dy < h; ++dy)
        for (int dx = 0; dx < w; ++dx)
            clk_texture_write_cell(tex, x + dx, y + dy, ch, style_id);
}

void clk_texture_write_string(clk_texture* tex, int x, int y, const char* str, int style_id) {
    if (!tex || !tex->data || !str)
        return;
    int col = 0, i = 0;
    while (str[i] != '\0') {
        unsigned char c = (unsigned char)str[i];
        int byte_len;
        if ((c & 0x80) == 0)
            byte_len = 1;
        else if ((c & 0xE0) == 0xC0)
            byte_len = 2;
        else if ((c & 0xF0) == 0xE0)
            byte_len = 3;
        else if ((c & 0xF8) == 0xF0)
            byte_len = 4;
        else {
            ++i;
            continue;
        }
        char tmp[5] = {0};
        for (int j = 0; j < byte_len && str[i + j] != '\0'; ++j)
            tmp[j] = str[i + j];
        int cw = clk_cell_char_width(tmp);
        if (cw == 2)
            clk_texture_write_wide_cell(tex, x + col, y, tmp, style_id);
        else
            clk_texture_write_cell(tex, x + col, y, tmp, style_id);
        i += byte_len;
        col += cw;
    }
}

void clk_texture_clear_cell(clk_texture* tex, int x, int y) {
    if (!tex || !tex->data || x < 0 || x >= tex->tex_w || y < 0 || y >= tex->tex_h)
        return;
    tex->data[x + y * tex->tex_w].is_empty = true;
}

void clk_texture_clear_all(clk_texture* tex) {
    if (!tex || !tex->data)
        return;
    for (int i = 0; i < tex->tex_w * tex->tex_h; ++i)
        tex->data[i].is_empty = true;
}

const clk_cell* clk_texture_get_cell(const clk_texture* tex, int x, int y) {
    if (!tex || !tex->data || x < 0 || x >= tex->tex_w || y < 0 || y >= tex->tex_h)
        return NULL;
    return &tex->data[x + y * tex->tex_w];
}
