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

#define CLK_TEXTURE_DEFAULT_LENGTH (16)
#define CLK_ANSI_OUTPUT_ESTIMATE_PER_CELL (100)
#define CLK_STYLE_DEFAULT_CAPACITY (16)

/* ------------------------------------------------------------------
 *  ANSI buffer formatting macros
 *
 *  APPENDF — append formatted text to a fixed-size char array.
 *            Bail out (return from enclosing function) on overflow.
 *  APPENDC — append a single character.
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

static int  clk_is_term_init          = false;
static bool clk_is_texture_list_sorted = false;

static clk_cell* screen_buffer;
static bool*     if_rendered_sign;

static int screen_w, screen_h;
static int screen_size;

static char* ansi_output;
static int   ansi_output_length;
static int   ansi_output_capacity;

static const clk_texture** texture_render_list;
static int texture_list_count    = 0;
static int texture_list_capacity = CLK_TEXTURE_DEFAULT_LENGTH;

static clk_style* style_registry;
static int style_count    = 0;
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

    /* texture render list */
    const clk_texture** temp_l =
        malloc(texture_list_capacity * sizeof(const clk_texture*));
    if (!temp_l) {
        free(screen_buffer);
        return false;
    }
    texture_render_list = temp_l;
    for (int i = 0; i < texture_list_capacity; ++i)
        texture_render_list[i] = NULL;

    /* per-cell "already claimed" flags for z-order masking */
    bool* temp_sign = calloc(screen_size, sizeof(bool));
    if (!temp_sign) {
        free(screen_buffer);
        free(texture_render_list);
        return false;
    }
    if_rendered_sign = temp_sign;

    /* ANSI output buffer */
    char* temp_a = calloc(1, screen_size * CLK_ANSI_OUTPUT_ESTIMATE_PER_CELL);
    if (!temp_a) {
        free(screen_buffer);
        free(texture_render_list);
        free(if_rendered_sign);
        return false;
    }
    ansi_output = temp_a;
    ansi_output_length = 0;
    ansi_output_capacity = screen_size * CLK_ANSI_OUTPUT_ESTIMATE_PER_CELL;

    /* style registry — slot 0 is the "no-style" default */
    clk_style* temp_st =
        malloc(CLK_STYLE_DEFAULT_CAPACITY * sizeof(clk_style));
    if (!temp_st) {
        free(screen_buffer);
        free(texture_render_list);
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

    free(screen_buffer);          screen_buffer       = NULL;
    free(texture_render_list);    texture_render_list = NULL;
    free(ansi_output);            ansi_output         = NULL;
    free(if_rendered_sign);       if_rendered_sign    = NULL;
    free(style_registry);         style_registry      = NULL;

    texture_list_count    = 0;
    texture_list_capacity = CLK_TEXTURE_DEFAULT_LENGTH;
    style_count           = 0;
    style_capacity        = CLK_STYLE_DEFAULT_CAPACITY;
    ansi_output_length    = 0;
    ansi_output_capacity  = 0;
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

    /* deduplicate — return existing ID if matching */
    for (int i = 1; i < style_count; ++i) {
        if (style_registry[i].fg_color.raw == fg.raw &&
            style_registry[i].bg_color.raw == bg.raw &&
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

/* ================================================================
 *  Render list management
 * ================================================================ */

static int cmp_texture_zorder(const void* tex1, const void* tex2) {
    const clk_texture* t1 = *(const clk_texture**)tex1;
    const clk_texture* t2 = *(const clk_texture**)tex2;

    if (t1->tex_z_order != t2->tex_z_order)
        /* descending z-order */
        return (t2->tex_z_order > t1->tex_z_order) -
               (t2->tex_z_order < t1->tex_z_order);

    return 0;
}

bool clk_term_add_texture(const clk_texture* texture) {
    if (!clk_is_term_init)
        return false;

    if (!texture)
        return false;

    int count = texture_list_count + 1;

    if (count > texture_list_capacity) {
        int new_capacity = texture_list_capacity * 2;
        const clk_texture** temp_l =
            realloc(texture_render_list,
                    new_capacity * sizeof(const clk_texture*));
        if (!temp_l)
            return false;

        texture_render_list = temp_l;
        for (int i = texture_list_capacity; i < new_capacity; ++i)
            texture_render_list[i] = NULL;
        texture_list_capacity = new_capacity;
    }

    texture_render_list[texture_list_count] = texture;
    texture_list_count = count;

    /* mark as needing re-sort */
    clk_is_texture_list_sorted = false;

    return true;
}

/* ================================================================
 *  Rendering helpers
 * ================================================================ */

static bool if_cell_equal(const clk_cell* c1, const clk_cell* c2) {
    if (c1 == c2) return true;
    if (!c1 || !c2) return false;

    return memcmp(c1->cell_tex, c2->cell_tex, sizeof(c1->cell_tex)) == 0 &&
           c1->style_id == c2->style_id &&
           c1->is_empty == c2->is_empty;
}

static void clk_add_cell_to_ansi_output(const clk_cell* cell, int x, int y) {
    if (!clk_is_term_init)
        return;

    if (!cell || cell->is_empty)
        return;

    const clk_style* style = clk_get_style(cell->style_id);

    char buf[256];
    int len = 0;

    /* ANSI cursor position (1-indexed) */
    APPENDF(buf, sizeof(buf), len, "\033[%d;%dH", y + 1, x + 1);

    if (style) {
        int params[16];
        int pcount = 0;

        if (style->attrs & ATTR_BOLD)      params[pcount++] = 1;
        if (style->attrs & ATTR_DIM)       params[pcount++] = 2;
        if (style->attrs & ATTR_ITALIC)    params[pcount++] = 3;
        if (style->attrs & ATTR_UNDERLINE) params[pcount++] = 4;
        if (style->attrs & ATTR_BLINK)     params[pcount++] = 5;
        if (style->attrs & ATTR_REVERSE)   params[pcount++] = 7;
        if (style->attrs & ATTR_HIDDEN)    params[pcount++] = 8;
        if (style->attrs & ATTR_STRIKE)    params[pcount++] = 9;

        bool has_fg = style->fg_color.raw != 0;
        bool has_bg = style->bg_color.raw != 0;

        if (pcount > 0 || has_fg || has_bg) {
            APPENDF(buf, sizeof(buf), len, "\033[");
            for (int i = 0; i < pcount; i++)
                APPENDF(buf, sizeof(buf), len, "%s%d",
                        i > 0 ? ";" : "", params[i]);

            if (has_fg) {
                APPENDF(buf, sizeof(buf), len, "%s38;2;%d;%d;%d",
                        pcount > 0 ? ";" : "",
                        style->fg_color.rgb.r,
                        style->fg_color.rgb.g,
                        style->fg_color.rgb.b);
                pcount++;
            }
            if (has_bg) {
                APPENDF(buf, sizeof(buf), len, "%s48;2;%d;%d;%d",
                        pcount > 0 ? ";" : "",
                        style->bg_color.rgb.r,
                        style->bg_color.rgb.g,
                        style->bg_color.rgb.b);
            }
            APPENDF(buf, sizeof(buf), len, "m");
        }
    }

    for (int i = 0; i < 5 && cell->cell_tex[i] != '\0'; i++)
        APPENDC(buf, sizeof(buf), len, cell->cell_tex[i]);

    /* SGR reset */
    APPENDF(buf, sizeof(buf), len, "\033[0m");

    /* grow global ANSI buffer if needed */
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

    /* reset per-frame mask */
    for (int i = 0; i < screen_size; ++i)
        if_rendered_sign[i] = 0;

    /* lazy sort — only when textures have been added or re-ordered */
    if (!clk_is_texture_list_sorted) {
        qsort(texture_render_list, texture_list_count,
              sizeof(const clk_texture*), cmp_texture_zorder);
        clk_is_texture_list_sorted = true;
    }

    /* first pass: composite textures into screen_buffer */
    for (int i = 0; i < texture_list_count; ++i) {
        const clk_texture* tex = texture_render_list[i];
        if (!tex || tex->is_invalid || !tex->data)
            continue;

        int pos_x = tex->posx, pos_y = tex->posy;
        int width = tex->tex_w, height = tex->tex_h;

        for (int ty = 0; ty < height; ++ty) {
            for (int tx = 0; tx < width; ++tx) {
                int x = tx + pos_x;
                int y = ty + pos_y;
                if (x < 0 || x >= screen_w || y < 0 || y >= screen_h)
                    continue;

                int idx = x + y * screen_w;
                const clk_cell* cell = &tex->data[tx + ty * width];

                if (if_rendered_sign[idx] || cell->is_empty)
                    continue;

                /* diff: only output cells that changed */
                if (!if_cell_equal(cell, &screen_buffer[idx])) {
                    clk_add_cell_to_ansi_output(cell, x, y);
                    screen_buffer[idx] = *cell;
                }
                if_rendered_sign[idx] = 1;
            }
        }
    }

    /* second pass: erase cells that were present last frame but
     * are not rendered this frame */
    for (int i = 0; i < screen_size; ++i) {
        if (!if_rendered_sign[i] && !screen_buffer[i].is_empty) {
            int x = i % screen_w;
            int y = i / screen_w;

            clk_cell clear = {
                .cell_tex = {' ', '\0', 0, 0, 0},
                .style_id = 0,
                .is_empty = false
            };
            clk_add_cell_to_ansi_output(&clear, x, y);
            screen_buffer[i] = (clk_cell){.is_empty = true};
        }
    }

    /* flush accumulated ANSI to stdout */
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
    int new_cap  = new_size * CLK_ANSI_OUTPUT_ESTIMATE_PER_CELL;

    /* allocate all three new buffers before touching the old ones */
    clk_cell* new_buf  = malloc(new_size * sizeof(clk_cell));
    bool*     new_sign = malloc(new_size * sizeof(bool));
    char*     new_ansi = malloc(new_cap);

    if (!new_buf || !new_sign || !new_ansi) {
        free(new_buf); free(new_sign); free(new_ansi);
        return false;
    }

    memset(new_sign, 0, new_size * sizeof(bool));

    int copy_count = screen_size < new_size ? screen_size : new_size;
    memcpy(new_buf, screen_buffer, copy_count * sizeof(clk_cell));

    clk_cell empty = {.is_empty = true};
    for (int i = screen_size; i < new_size; ++i)
        new_buf[i] = empty;

    /* swap — atomic from here on */
    free(screen_buffer);
    free(if_rendered_sign);
    free(ansi_output);

    screen_buffer      = new_buf;
    if_rendered_sign   = new_sign;
    ansi_output        = new_ansi;

    screen_w           = new_w;
    screen_h           = new_h;
    screen_size        = new_size;
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

    /* compact render list — remove invalid textures */
    int write = 0;
    for (int read = 0; read < texture_list_count; ++read) {
        const clk_texture* tex = texture_render_list[read];
        if (tex && !tex->is_invalid)
            texture_render_list[write++] = tex;
    }
    for (int i = write; i < texture_list_count; ++i)
        texture_render_list[i] = NULL;
    texture_list_count = write;

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
        *term_w = csbi.srWindow.Right  - csbi.srWindow.Left + 1;
        *term_h = csbi.srWindow.Bottom - csbi.srWindow.Top  + 1;
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

    clk_cell empty = {.is_empty = true, .style_id = 0};
    for (int i = 0; i < w * h; ++i)
        data[i] = empty;

    tex.tex_w = w;
    tex.tex_h = h;
    tex.data  = data;
    return tex;
}

void clk_texture_destroy(clk_texture* tex) {
    if (!tex || !tex->data)
        return;
    free(tex->data);
    tex->data  = NULL;
    tex->tex_w = 0;
    tex->tex_h = 0;
}

/* ================================================================
 *  Texture — cell manipulation
 * ================================================================ */

void clk_texture_set_cell(clk_texture* tex, int x, int y,
                          const char* ch, int style_id) {
    if (!tex || !tex->data ||
        x < 0 || x >= tex->tex_w || y < 0 || y >= tex->tex_h)
        return;

    clk_cell* cell = &tex->data[x + y * tex->tex_w];

    int i = 0;
    while (i < 4 && ch && ch[i]) {
        cell->cell_tex[i] = ch[i];
        ++i;
    }
    cell->cell_tex[i] = '\0';
    cell->style_id = style_id;
    cell->is_empty = false;
}

void clk_texture_fill_rect(clk_texture* tex, int x, int y,
                           int w, int h, const char* ch, int style_id) {
    if (!tex || !tex->data || w <= 0 || h <= 0)
        return;

    for (int dy = 0; dy < h; ++dy)
        for (int dx = 0; dx < w; ++dx)
            clk_texture_set_cell(tex, x + dx, y + dy, ch, style_id);
}

void clk_texture_write_string(clk_texture* tex, int x, int y,
                              const char* str, int style_id) {
    if (!tex || !tex->data || !str)
        return;

    int col = 0;
    int i = 0;
    while (str[i] != '\0') {
        unsigned char c = (unsigned char)str[i];
        int byte_len;

        /* determine UTF-8 character length from leading byte */
        if ((c & 0x80) == 0)
            byte_len = 1;
        else if ((c & 0xE0) == 0xC0)
            byte_len = 2;
        else if ((c & 0xF0) == 0xE0)
            byte_len = 3;
        else if ((c & 0xF8) == 0xF0)
            byte_len = 4;
        else {
            /* skip unexpected continuation bytes */
            ++i;
            continue;
        }

        char tmp[5] = {0};
        for (int j = 0; j < byte_len && str[i + j] != '\0'; ++j)
            tmp[j] = str[i + j];
        clk_texture_set_cell(tex, x + col, y, tmp, style_id);
        i += byte_len;
        ++col;
    }
}

void clk_texture_clear_cell(clk_texture* tex, int x, int y) {
    if (!tex || !tex->data ||
        x < 0 || x >= tex->tex_w || y < 0 || y >= tex->tex_h)
        return;
    tex->data[x + y * tex->tex_w].is_empty = true;
}

void clk_texture_clear_all(clk_texture* tex) {
    if (!tex || !tex->data)
        return;
    for (int i = 0; i < tex->tex_w * tex->tex_h; ++i)
        tex->data[i].is_empty = true;
}

/* ================================================================
 *  Texture — layout setters
 * ================================================================ */

void clk_texture_set_pos(clk_texture* tex, int x, int y) {
    if (!tex) return;
    tex->posx = x;
    tex->posy = y;
}

void clk_texture_set_z_order(clk_texture* tex, int z) {
    if (!tex) return;
    tex->tex_z_order = z;
    clk_is_texture_list_sorted = false;   /* re-sort needed */
}

void clk_texture_set_invalid(clk_texture* tex, bool invalid) {
    if (!tex) return;
    tex->is_invalid = invalid;
}

void clk_texture_set_pos_z(clk_texture* tex, int x, int y, int z) {
    if (!tex) return;
    tex->posx = x;
    tex->posy = y;
    tex->tex_z_order = z;
    clk_is_texture_list_sorted = false;
}
