#define _GNU_SOURCE
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

static int term_initialized = false;
static bool sprite_list_sorted = false;

static clk_cell* screen_buffer;
static bool* cell_claimed;

static int screen_width, screen_height;
static int cell_count;

static char* ansi_output;
static int ansi_output_length;
static int ansi_output_capacity;

static clk_sprite** sprite_render_list;
static int sprite_count = 0;
static int sprite_capacity = CLK_SPRITE_LIST_DEFAULT_CAPACITY;

static clk_style* style_registry;
static int style_count = 0;
static int style_capacity = 0;

/** Hardware cursor state — consumed by clk_term_draw() at end of frame. */
static struct {
    int x, y;
    clk_cursor_shape shape;
    bool visible;
} cursor = {.shape = 1};

static const clk_style* clk_get_style(int style_id) {
    if (style_id <= 0 || style_id >= style_count)
        return NULL;
    return &style_registry[style_id];
}

/* ================================================================
 *  Terminal lifecycle
 * ================================================================ */

bool clk_term_init(void) {
    if (term_initialized)
        return true;

    clk_key_io_init();

    int detected_width, detected_height;
    if (!clk_term_get_size(&detected_width, &detected_height))
        return false;
    if (detected_width <= 0 || detected_height <= 0)
        return false;
    cell_count = detected_width * detected_height;
    screen_width = detected_width;
    screen_height = detected_height;

    clk_cell empty_cell = {.is_empty = true};

    clk_cell* screen_buf = malloc(cell_count * sizeof(clk_cell));
    if (!screen_buf)
        return false;
    screen_buffer = screen_buf;
    for (int i = 0; i < cell_count; ++i)
        screen_buffer[i] = empty_cell;

    clk_sprite** sprite_buf = malloc(sprite_capacity * sizeof(clk_sprite*));
    if (!sprite_buf) {
        free(screen_buffer);
        return false;
    }
    sprite_render_list = sprite_buf;
    for (int i = 0; i < sprite_capacity; ++i)
        sprite_render_list[i] = NULL;

    /* per-cell "already claimed" flags for z-order masking */
    bool* claimed_buf = calloc(cell_count, sizeof(bool));
    if (!claimed_buf) {
        free(screen_buffer);
        free(sprite_render_list);
        return false;
    }
    cell_claimed = claimed_buf;

    char* ansi_buf = calloc(1, cell_count * CLK_ANSI_OUTPUT_ESTIMATE_PER_CELL);
    if (!ansi_buf) {
        free(screen_buffer);
        free(sprite_render_list);
        free(cell_claimed);
        return false;
    }
    ansi_output = ansi_buf;
    ansi_output_length = 0;
    ansi_output_capacity = cell_count * CLK_ANSI_OUTPUT_ESTIMATE_PER_CELL;

    /* style registry — slot 0 is the "no-style" default */
    clk_style* styles_buf = malloc(CLK_STYLE_DEFAULT_CAPACITY * sizeof(clk_style));
    if (!styles_buf) {
        free(screen_buffer);
        free(sprite_render_list);
        free(cell_claimed);
        free(ansi_output);
        return false;
    }
    style_registry = styles_buf;
    style_registry[0] = (clk_style){{.raw = 0}, {.raw = 0}, CLK_ATTR_NONE};
    style_count = 1;
    style_capacity = CLK_STYLE_DEFAULT_CAPACITY;

    term_initialized = true;

    printf("\033[2J\033[H");
    fflush(stdout);
    clk_term_cursor_hide();
    return true;
}

void clk_term_close(void) {
    if (!term_initialized)
        return;

    clk_key_io_close();

    free(screen_buffer);
    screen_buffer = NULL;
    free(sprite_render_list);
    sprite_render_list = NULL;
    free(ansi_output);
    ansi_output = NULL;
    free(cell_claimed);
    cell_claimed = NULL;
    free(style_registry);
    style_registry = NULL;

    sprite_count = 0;
    sprite_capacity = CLK_SPRITE_LIST_DEFAULT_CAPACITY;
    style_count = 0;
    style_capacity = CLK_STYLE_DEFAULT_CAPACITY;
    ansi_output_length = 0;
    ansi_output_capacity = 0;
    screen_width = screen_height = cell_count = 0;

    term_initialized = false;

    printf("\033[2J\033[H\033[0 q");
    fflush(stdout);
    clk_term_cursor_show();
}

bool clk_term_is_init(void) {
    return term_initialized;
}

/* ================================================================
 *  Hardware cursor
 * ================================================================ */

void clk_term_cursor_set_pos(int x, int y) {
    cursor.x = x;
    cursor.y = y;
}

void clk_term_cursor_set_shape(clk_cursor_shape shape) {
    cursor.shape = shape;
}

void clk_term_cursor_show(void) {
    cursor.visible = true;
    printf("\033[?25h");
    fflush(stdout);
}

void clk_term_cursor_hide(void) {
    cursor.visible = false;
    printf("\033[?25l");
    fflush(stdout);
}

/* ================================================================
 *  UTF-8 display width
 * ================================================================ */

static int clk_cell_char_width(const char* utf8);

int clk_term_utf8_display_width(const char* str, size_t byte_len) {
    int width = 0;
    size_t i = 0;

    while (i < byte_len && str[i] != '\0') {
        unsigned char c = (unsigned char)str[i];
        int ch_bytes;

        if ((c & 0x80) == 0)
            ch_bytes = 1;
        else if ((c & 0xE0) == 0xC0)
            ch_bytes = 2;
        else if ((c & 0xF0) == 0xE0)
            ch_bytes = 3;
        else if ((c & 0xF8) == 0xF0)
            ch_bytes = 4;
        else {
            i++;
            width++;
            continue;
        }

        if (i + ch_bytes > byte_len)
            break;

        width += clk_cell_char_width(str + i);
        i += ch_bytes;
    }
    return width;
}

/* ================================================================
 *  Style registry
 * ================================================================ */

int clk_term_register_style(clk_color fg, clk_color bg, uint8_t attrs) {
    if (!term_initialized)
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

uint8_t clk_term_parse_attrs(const char* str) {
    if (!str)
        return CLK_ATTR_NONE;
    uint8_t attrs = CLK_ATTR_NONE;
    if (strstr(str, "bold"))
        attrs |= CLK_ATTR_BOLD;
    if (strstr(str, "dim"))
        attrs |= CLK_ATTR_DIM;
    if (strstr(str, "italic"))
        attrs |= CLK_ATTR_ITALIC;
    if (strstr(str, "underline"))
        attrs |= CLK_ATTR_UNDERLINE;
    if (strstr(str, "blink"))
        attrs |= CLK_ATTR_BLINK;
    if (strstr(str, "reverse"))
        attrs |= CLK_ATTR_REVERSE;
    if (strstr(str, "hidden"))
        attrs |= CLK_ATTR_HIDDEN;
    if (strstr(str, "strike"))
        attrs |= CLK_ATTR_STRIKE;
    return attrs;
}

int clk_term_register_style_rgb(int fg_r, int fg_g, int fg_b, int bg_r, int bg_g, int bg_b,
                                uint8_t attrs) {
    if (fg_r < 0 || fg_r > 255 || fg_g < 0 || fg_g > 255 || fg_b < 0 || fg_b > 255 || bg_r < 0 ||
        bg_r > 255 || bg_g < 0 || bg_g > 255 || bg_b < 0 || bg_b > 255)
        return 0;

    clk_color fg = {.rgb = {(uint8_t)fg_r, (uint8_t)fg_g, (uint8_t)fg_b}};
    clk_color bg = {.rgb = {(uint8_t)bg_r, (uint8_t)bg_g, (uint8_t)bg_b}};
    return clk_term_register_style(fg, bg, attrs);
}

int clk_term_register_style_hex(const char* fg_hex, const char* bg_hex, uint8_t attrs) {
    int fg_r, fg_g, fg_b, bg_r, bg_g, bg_b;
    if (!clk_term_parse_hex_color(fg_hex, &fg_r, &fg_g, &fg_b) ||
        !clk_term_parse_hex_color(bg_hex, &bg_r, &bg_g, &bg_b))
        return 0;
    return clk_term_register_style_rgb(fg_r, fg_g, fg_b, bg_r, bg_g, bg_b, attrs);
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

static int cmp_sprite_z_order(const void* s1, const void* s2) {
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
    sprite_list_sorted = false;
}

clk_sprite* clk_sprite_create(void) {
    clk_sprite* s = malloc(sizeof(clk_sprite));
    if (!s)
        return NULL;
    memset(s, 0, sizeof(clk_sprite));
    return s;
}

clk_sprite* clk_sprite_create_with_texture(clk_texture* tex, int x, int y, int z) {
    clk_sprite* s = clk_sprite_create();
    if (!s)
        return NULL;
    s->tex = tex;
    s->posx = x;
    s->posy = y;
    s->z_order = z;
    return s;
}

void clk_sprite_destroy(clk_sprite* s) {
    if (!s)
        return;
    clk_term_remove_sprite(s);
    free(s);
}

void clk_sprite_set_texture(clk_sprite* s, clk_texture* tex) {
    if (!s)
        return;
    s->tex = tex;
}

void clk_sprite_remove_texture(clk_sprite* s) {
    if (!s)
        return;
    s->tex = NULL;
}

void clk_term_add_sprite(clk_sprite* sprite) {
    if (!term_initialized || !sprite)
        return;

    /* dedup — ignore if already registered */
    for (int i = 0; i < sprite_count; ++i) {
        if (sprite_render_list[i] == sprite)
            return;
    }

    int count = sprite_count + 1;
    if (count > sprite_capacity) {
        int new_capacity = sprite_capacity * 2;
        clk_sprite** temp = realloc(sprite_render_list, new_capacity * sizeof(clk_sprite*));
        if (!temp)
            return;
        sprite_render_list = temp;
        for (int i = sprite_capacity; i < new_capacity; ++i)
            sprite_render_list[i] = NULL;
        sprite_capacity = new_capacity;
    }

    sprite_render_list[sprite_count] = sprite;
    sprite_count = count;
    sprite_list_sorted = false;
}

void clk_term_remove_sprite(const clk_sprite* sprite) {
    if (!sprite || !term_initialized)
        return;

    for (int i = 0; i < sprite_count; ++i) {
        if (sprite_render_list[i] == sprite) {
            for (int j = i; j < sprite_count - 1; ++j)
                sprite_render_list[j] = sprite_render_list[j + 1];
            sprite_render_list[sprite_count - 1] = NULL;
            sprite_count--;
            return;
        }
    }
}

void clk_term_clear_sprites(void) {
    if (!term_initialized)
        return;
    for (int i = 0; i < sprite_count; ++i)
        sprite_render_list[i] = NULL;
    sprite_count = 0;
    sprite_list_sorted = true;
}

/* ================================================================
 *  Rendering helpers
 * ================================================================ */

static bool cell_equals(const clk_cell* c1, const clk_cell* c2) {
    if (c1 == c2)
        return true;
    if (!c1 || !c2)
        return false;
    return strcmp(c1->cell_tex, c2->cell_tex) == 0 && c1->style_id == c2->style_id &&
           c1->type == c2->type && c1->is_empty == c2->is_empty;
}

static void clk_add_cell_to_ansi_output(const clk_cell* cell, int x, int y) {
    if (!term_initialized)
        return;
    if (!cell || cell->is_empty)
        return;

    const clk_style* style = clk_get_style(cell->style_id);
    char buf[CLK_ANSI_CELL_BUF_SIZE];
    int len = 0;

    APPENDF(buf, sizeof(buf), len, "\033[%d;%dH", y + 1, x + 1);

    if (style) {
        int params[16], param_count = 0;
        if (style->attrs & CLK_ATTR_BOLD)
            params[param_count++] = 1;
        if (style->attrs & CLK_ATTR_DIM)
            params[param_count++] = 2;
        if (style->attrs & CLK_ATTR_ITALIC)
            params[param_count++] = 3;
        if (style->attrs & CLK_ATTR_UNDERLINE)
            params[param_count++] = 4;
        if (style->attrs & CLK_ATTR_BLINK)
            params[param_count++] = 5;
        if (style->attrs & CLK_ATTR_REVERSE)
            params[param_count++] = 7;
        if (style->attrs & CLK_ATTR_HIDDEN)
            params[param_count++] = 8;
        if (style->attrs & CLK_ATTR_STRIKE)
            params[param_count++] = 9;

        bool has_foreground = style->fg_color.raw != 0;
        bool has_background = style->bg_color.raw != 0;

        if (param_count > 0 || has_foreground || has_background) {
            APPENDF(buf, sizeof(buf), len, "\033[");
            for (int i = 0; i < param_count; i++)
                APPENDF(buf, sizeof(buf), len, "%s%d", i > 0 ? ";" : "", params[i]);
            if (has_foreground) {
                APPENDF(buf, sizeof(buf), len, "%s38;2;%d;%d;%d", param_count > 0 ? ";" : "",
                        style->fg_color.rgb.r, style->fg_color.rgb.g, style->fg_color.rgb.b);
                param_count++;
            }
            if (has_background) {
                APPENDF(buf, sizeof(buf), len, "%s48;2;%d;%d;%d", param_count > 0 ? ";" : "",
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
    if (!term_initialized)
        return;

    for (int i = 0; i < cell_count; ++i)
        cell_claimed[i] = 0;

    if (!sprite_list_sorted) {
        qsort(sprite_render_list, sprite_count, sizeof(clk_sprite*), cmp_sprite_z_order);
        sprite_list_sorted = true;
    }

    for (int i = 0; i < sprite_count; ++i) {
        const clk_sprite* s = sprite_render_list[i];
        if (!s || s->is_invalid || s->is_hidden || !s->tex || !s->tex->data)
            continue;

        int pos_x = s->posx, pos_y = s->posy;
        int tex_w = s->tex->tex_w, tex_h = s->tex->tex_h;

        for (int tex_y = 0; tex_y < tex_h; ++tex_y) {
            for (int tex_x = 0; tex_x < tex_w; ++tex_x) {
                int x = tex_x + pos_x;
                int y = tex_y + pos_y;
                if (x < 0 || x >= screen_width || y < 0 || y >= screen_height)
                    continue;

                int idx = x + y * screen_width;
                const clk_cell* cell = &s->tex->data[tex_x + tex_y * tex_w];

                if (cell->type == CELL_WIDE_TRAIL)
                    continue;
                if (cell->type == CELL_WIDE_LEAD) {
                    const clk_cell* next =
                        (tex_x + 1 < tex_w) ? &s->tex->data[tex_x + 1 + tex_y * tex_w] : NULL;
                    if (!next || next->type != CELL_WIDE_TRAIL)
                        continue;
                }

                bool blocked = cell_claimed[idx];
                if (cell->type == CELL_WIDE_LEAD && x + 1 < screen_width)
                    blocked = blocked || cell_claimed[idx + 1];
                if (blocked || cell->is_empty)
                    continue;

                if (!cell_equals(cell, &screen_buffer[idx])) {
                    clk_add_cell_to_ansi_output(cell, x, y);
                    screen_buffer[idx] = *cell;
                }

                cell_claimed[idx] = 1;
                if (cell->type == CELL_WIDE_LEAD && x + 1 < screen_width) {
                    cell_claimed[idx + 1] = 1;
                    screen_buffer[idx + 1] = (clk_cell){.type = CELL_WIDE_TRAIL, .is_empty = false};
                }
            }
        }
    }

    for (int i = 0; i < cell_count; ++i) {
        if (!cell_claimed[i] && !screen_buffer[i].is_empty) {
            int x = i % screen_width, y = i / screen_width;
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

    if (cursor.visible) {
        fprintf(stdout, "\033[%d;%dH\033[%d q\033[?25h", cursor.y + 1, cursor.x + 1,
                (int)cursor.shape);
        fflush(stdout);
    }
}

/* ================================================================
 *  Terminal resize
 * ================================================================ */

bool clk_resize_term(int new_w, int new_h) {
    if (!term_initialized)
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

    clk_cell empty = {.is_empty = true};
    for (int i = 0; i < new_size; ++i)
        new_buf[i] = empty;

    free(screen_buffer);
    screen_buffer = new_buf;
    free(cell_claimed);
    cell_claimed = new_sign;
    free(ansi_output);
    ansi_output = new_ansi;

    screen_width = new_w;
    screen_height = new_h;
    cell_count = new_size;
    ansi_output_capacity = new_cap;

    printf("\033[2J\033[H");
    fflush(stdout);
    return true;
}

void clk_term_resize(void) {
    if (!term_initialized)
        return;

    int new_w, new_h;
    if (!clk_term_get_size(&new_w, &new_h))
        return;
    if (new_w <= 0 || new_h <= 0)
        return;

    if (new_w != screen_width || new_h != screen_height) {
        clk_resize_term(new_w, new_h);
    }
}

bool clk_term_update(void) {
    if (!term_initialized)
        return false;

    clk_term_resize();
    clk_term_compact();
    return true;
}

void clk_term_compact(void) {
    if (!term_initialized)
        return;

    int write = 0;
    for (int read = 0; read < sprite_count; ++read) {
        clk_sprite* s = sprite_render_list[read];
        if (s && !s->is_invalid)
            sprite_render_list[write++] = s;
    }
    for (int i = write; i < sprite_count; ++i)
        sprite_render_list[i] = NULL;
    sprite_count = write;
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
    return (w != screen_width || h != screen_height);
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
    if (!tex || !tex->data || !ch || x < 0 || x + 1 >= tex->tex_w || y < 0 || y >= tex->tex_h)
        return;

    clk_cell cell = {.style_id = style_id, .type = CELL_WIDE_LEAD, .is_empty = false};
    int i = 0;
    while (i < 4 && ch[i])
        cell.cell_tex[i] = ch[i], ++i;
    cell.cell_tex[i] = '\0';
    clk_texture_set_cell(tex, x, y, &cell);
}

void clk_texture_set_cell(clk_texture* tex, int x, int y, const clk_cell* cell) {
    if (!tex || !tex->data || !cell || x < 0 || x >= tex->tex_w || y < 0 || y >= tex->tex_h)
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

/* ================================================================
 *  Unicode character width — Markus Kuhn mk_wcwidth-style tables
 *  Public Domain, based on Unicode 6.3 East Asian Width data
 * ================================================================ */

struct clk_wcwidth_interval {
    unsigned int first;
    unsigned int last;
};

static const struct clk_wcwidth_interval clk_combining[] = {
    {0x0300, 0x036F},   {0x0483, 0x0489},   {0x0591, 0x05BD},   {0x05BF, 0x05BF},
    {0x05C1, 0x05C2},   {0x05C4, 0x05C5},   {0x05C7, 0x05C7},   {0x0610, 0x061A},
    {0x064B, 0x065F},   {0x0670, 0x0670},   {0x06D6, 0x06DC},   {0x06DF, 0x06E4},
    {0x06E7, 0x06E8},   {0x06EA, 0x06ED},   {0x0711, 0x0711},   {0x0730, 0x074A},
    {0x07A6, 0x07B0},   {0x07EB, 0x07F3},   {0x0816, 0x0819},   {0x081B, 0x0823},
    {0x0825, 0x0827},   {0x0829, 0x082D},   {0x0859, 0x085B},   {0x08E3, 0x0902},
    {0x093A, 0x093A},   {0x093C, 0x093C},   {0x0941, 0x0948},   {0x094D, 0x094D},
    {0x0951, 0x0957},   {0x0962, 0x0963},   {0x0981, 0x0981},   {0x09BC, 0x09BC},
    {0x09C1, 0x09C4},   {0x09CD, 0x09CD},   {0x09E2, 0x09E3},   {0x0A01, 0x0A02},
    {0x0A3C, 0x0A3C},   {0x0A41, 0x0A42},   {0x0A47, 0x0A48},   {0x0A4B, 0x0A4D},
    {0x0A51, 0x0A51},   {0x0A70, 0x0A71},   {0x0A75, 0x0A75},   {0x0A81, 0x0A82},
    {0x0ABC, 0x0ABC},   {0x0AC1, 0x0AC5},   {0x0AC7, 0x0AC8},   {0x0ACD, 0x0ACD},
    {0x0AE2, 0x0AE3},   {0x0B01, 0x0B01},   {0x0B3C, 0x0B3C},   {0x0B3F, 0x0B3F},
    {0x0B41, 0x0B44},   {0x0B4D, 0x0B4D},   {0x0B56, 0x0B56},   {0x0B62, 0x0B63},
    {0x0B82, 0x0B82},   {0x0BC0, 0x0BC0},   {0x0BCD, 0x0BCD},   {0x0C00, 0x0C00},
    {0x0C3E, 0x0C40},   {0x0C46, 0x0C48},   {0x0C4A, 0x0C4D},   {0x0C55, 0x0C56},
    {0x0C62, 0x0C63},   {0x0C81, 0x0C81},   {0x0CBC, 0x0CBC},   {0x0CBF, 0x0CBF},
    {0x0CC6, 0x0CC6},   {0x0CCC, 0x0CCD},   {0x0CE2, 0x0CE3},   {0x0D01, 0x0D01},
    {0x0D41, 0x0D44},   {0x0D4D, 0x0D4D},   {0x0D62, 0x0D63},   {0x0DCA, 0x0DCA},
    {0x0DD2, 0x0DD4},   {0x0DD6, 0x0DD6},   {0x0E31, 0x0E31},   {0x0E34, 0x0E3A},
    {0x0E47, 0x0E4E},   {0x0EB1, 0x0EB1},   {0x0EB4, 0x0EB9},   {0x0EBB, 0x0EBC},
    {0x0EC8, 0x0ECD},   {0x0F18, 0x0F19},   {0x0F35, 0x0F35},   {0x0F37, 0x0F37},
    {0x0F39, 0x0F39},   {0x0F71, 0x0F7E},   {0x0F80, 0x0F84},   {0x0F86, 0x0F87},
    {0x0F8D, 0x0F97},   {0x0F99, 0x0FBC},   {0x0FC6, 0x0FC6},   {0x102D, 0x1030},
    {0x1032, 0x1037},   {0x1039, 0x103A},   {0x103D, 0x103E},   {0x1058, 0x1059},
    {0x105E, 0x1060},   {0x1071, 0x1074},   {0x1082, 0x1082},   {0x1085, 0x1086},
    {0x108D, 0x108D},   {0x109D, 0x109D},   {0x135D, 0x135F},   {0x1712, 0x1714},
    {0x1732, 0x1734},   {0x1752, 0x1753},   {0x1772, 0x1773},   {0x17B4, 0x17B5},
    {0x17B7, 0x17BD},   {0x17C6, 0x17C6},   {0x17C9, 0x17D3},   {0x17DD, 0x17DD},
    {0x180B, 0x180D},   {0x18A9, 0x18A9},   {0x1920, 0x1922},   {0x1927, 0x1928},
    {0x1932, 0x1932},   {0x1939, 0x193B},   {0x1A17, 0x1A18},   {0x1A56, 0x1A56},
    {0x1A58, 0x1A5E},   {0x1A60, 0x1A60},   {0x1A62, 0x1A62},   {0x1A65, 0x1A6C},
    {0x1A73, 0x1A7C},   {0x1A7F, 0x1A7F},   {0x1AB0, 0x1ABE},   {0x1B00, 0x1B03},
    {0x1B34, 0x1B34},   {0x1B36, 0x1B3A},   {0x1B3C, 0x1B3C},   {0x1B42, 0x1B42},
    {0x1B6B, 0x1B73},   {0x1B80, 0x1B81},   {0x1BA2, 0x1BA5},   {0x1BA8, 0x1BA9},
    {0x1BAB, 0x1BAD},   {0x1BE6, 0x1BE6},   {0x1BE8, 0x1BE9},   {0x1BED, 0x1BED},
    {0x1BEF, 0x1BF1},   {0x1C2C, 0x1C33},   {0x1C36, 0x1C37},   {0x1CD0, 0x1CD2},
    {0x1CD4, 0x1CE0},   {0x1CE2, 0x1CE8},   {0x1CED, 0x1CED},   {0x1CF4, 0x1CF4},
    {0x1CF8, 0x1CF9},   {0x1DC0, 0x1DF5},   {0x1DFC, 0x1DFF},   {0x20D0, 0x20F0},
    {0x2CEF, 0x2CF1},   {0x2D7F, 0x2D7F},   {0x2DE0, 0x2DFF},   {0xA66F, 0xA672},
    {0xA674, 0xA67D},   {0xA69E, 0xA69F},   {0xA6F0, 0xA6F1},   {0xA802, 0xA802},
    {0xA806, 0xA806},   {0xA80B, 0xA80B},   {0xA825, 0xA826},   {0xA8C4, 0xA8C4},
    {0xA8E0, 0xA8F1},   {0xA926, 0xA92D},   {0xA947, 0xA951},   {0xA980, 0xA982},
    {0xA9B3, 0xA9B3},   {0xA9B6, 0xA9B9},   {0xA9BC, 0xA9BC},   {0xA9E5, 0xA9E5},
    {0xAA29, 0xAA2E},   {0xAA31, 0xAA32},   {0xAA35, 0xAA36},   {0xAA43, 0xAA43},
    {0xAA4C, 0xAA4C},   {0xAA7C, 0xAA7C},   {0xAAB0, 0xAAB0},   {0xAAB2, 0xAAB4},
    {0xAAB7, 0xAAB8},   {0xAABE, 0xAABF},   {0xAAC1, 0xAAC1},   {0xAAEC, 0xAAED},
    {0xAAF6, 0xAAF6},   {0xABE5, 0xABE5},   {0xABE8, 0xABE8},   {0xABED, 0xABED},
    {0xFB1E, 0xFB1E},   {0xFE00, 0xFE0F},   {0xFE20, 0xFE2F},   {0x101FD, 0x101FD},
    {0x102E0, 0x102E0}, {0x10376, 0x1037A}, {0x10A01, 0x10A03}, {0x10A05, 0x10A06},
    {0x10A0C, 0x10A0F}, {0x10A38, 0x10A3A}, {0x10A3F, 0x10A3F}, {0x10AE5, 0x10AE6},
    {0x11001, 0x11001}, {0x11038, 0x11046}, {0x1107F, 0x11081}, {0x110B3, 0x110B6},
    {0x110B9, 0x110BA}, {0x11100, 0x11102}, {0x11127, 0x1112B}, {0x1112D, 0x11134},
    {0x11173, 0x11173}, {0x11180, 0x11181}, {0x111B6, 0x111BE}, {0x111CA, 0x111CC},
    {0x1122F, 0x11231}, {0x11234, 0x11234}, {0x11236, 0x11237}, {0x112DF, 0x112DF},
    {0x112E3, 0x112EA}, {0x11300, 0x11301}, {0x1133C, 0x1133C}, {0x11340, 0x11340},
    {0x11366, 0x1136C}, {0x11370, 0x11374}, {0x114B3, 0x114B8}, {0x114BA, 0x114BA},
    {0x114BF, 0x114C0}, {0x114C2, 0x114C3}, {0x115B2, 0x115B5}, {0x115BC, 0x115BD},
    {0x115BF, 0x115C0}, {0x115DC, 0x115DD}, {0x11633, 0x1163A}, {0x1163D, 0x1163D},
    {0x1163F, 0x11640}, {0x116AB, 0x116AB}, {0x116AD, 0x116AD}, {0x116B0, 0x116B5},
    {0x116B7, 0x116B7}, {0x1171D, 0x1171F}, {0x11722, 0x11725}, {0x11727, 0x1172B},
    {0x11A01, 0x11A06}, {0x11A09, 0x11A0A}, {0x11A33, 0x11A38}, {0x11A3B, 0x11A3E},
    {0x11A47, 0x11A47}, {0x11A51, 0x11A56}, {0x11A59, 0x11A5B}, {0x11A8A, 0x11A96},
    {0x11A98, 0x11A99}, {0x11C30, 0x11C36}, {0x11C38, 0x11C3D}, {0x11C3F, 0x11C3F},
    {0x11C92, 0x11CA7}, {0x11CAA, 0x11CB0}, {0x11CB2, 0x11CB3}, {0x11CB5, 0x11CB6},
    {0x11D31, 0x11D36}, {0x11D3A, 0x11D3A}, {0x11D3C, 0x11D3D}, {0x11D3F, 0x11D45},
    {0x11D47, 0x11D47}, {0x16AF0, 0x16AF4}, {0x16B30, 0x16B36}, {0x16F8F, 0x16F92},
    {0x1BC9D, 0x1BC9E}, {0x1D167, 0x1D169}, {0x1D17B, 0x1D182}, {0x1D185, 0x1D18B},
    {0x1D1AA, 0x1D1AD}, {0x1D242, 0x1D244}, {0x1DA00, 0x1DA36}, {0x1DA3B, 0x1DA6C},
    {0x1DA75, 0x1DA75}, {0x1DA84, 0x1DA84}, {0x1DA9B, 0x1DA9F}, {0x1DAA1, 0x1DAAF},
    {0x1E8D0, 0x1E8D6}, {0xE0100, 0xE01EF},
};

static const struct clk_wcwidth_interval clk_wide[] = {
    {0x1100, 0x115F},   {0x2300, 0x23FF},   {0x2329, 0x232A},   {0x2460, 0x24FF},
    {0x25A0, 0x27BF},   {0x2E80, 0x303E},   {0x3040, 0x33BF},   {0x3400, 0x4DBF},
    {0x4E00, 0x9FFF},   {0xA000, 0xA4CF},   {0xAC00, 0xD7AF},   {0xF900, 0xFAFF},
    {0xFE10, 0xFE6F},   {0xFF01, 0xFFE6},   {0x1B000, 0x1B2FF}, {0x1F000, 0x1F9FF},
    {0x1F200, 0x1F2FF}, {0x20000, 0x2FFFD}, {0x30000, 0x3FFFD},
};

static int clk_wcwidth_bisearch(unsigned int codepoint, const struct clk_wcwidth_interval* table,
                                int count) {
    int lo = 0;
    int hi = count - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        if (codepoint < table[mid].first)
            hi = mid - 1;
        else if (codepoint > table[mid].last)
            lo = mid + 1;
        else
            return 1;
    }
    return 0;
}

static int clk_wcwidth(unsigned int codepoint) {
    if (codepoint == 0)
        return 0;

    if (clk_wcwidth_bisearch(codepoint, clk_combining,
                             (int)(sizeof(clk_combining) / sizeof(clk_combining[0]))))
        return 0;

    if (clk_wcwidth_bisearch(codepoint, clk_wide, (int)(sizeof(clk_wide) / sizeof(clk_wide[0]))))
        return 2;

    return 1;
}

static int clk_cell_char_width(const char* utf8) {
    if (!utf8 || !utf8[0])
        return 0;

    unsigned char c0 = (unsigned char)utf8[0];
    unsigned int codepoint;
    int len;
    if ((c0 & 0x80) == 0) {
        codepoint = c0;
        len = 1;
    } else if ((c0 & 0xE0) == 0xC0) {
        codepoint = c0 & 0x1F;
        len = 2;
    } else if ((c0 & 0xF0) == 0xE0) {
        codepoint = c0 & 0x0F;
        len = 3;
    } else if ((c0 & 0xF8) == 0xF0) {
        codepoint = c0 & 0x07;
        len = 4;
    } else
        return 1;

    for (int i = 1; i < len; i++) {
        unsigned char cb = (unsigned char)utf8[i];
        if ((cb & 0xC0) != 0x80)
            return 1;
        codepoint = (codepoint << 6) | (cb & 0x3F);
    }
    return clk_wcwidth(codepoint);
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
        int char_width = clk_cell_char_width(tmp);
        if (char_width == 2)
            clk_texture_write_wide_cell(tex, x + col, y, tmp, style_id);
        else
            clk_texture_write_cell(tex, x + col, y, tmp, style_id);
        i += byte_len;
        col += char_width;
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
