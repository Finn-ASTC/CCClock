#define _GNU_SOURCE
#include "clk_clock.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "clk_json.h"
#include "clk_term.h"

/* ================================================================
 *  Time format translation
 *
 *  User-friendly tokens → strftime (%… ) tokens.
 *  Unrecognised characters (spaces, colons, newlines) pass through
 *  literally so they appear in the formatted output.
 * ================================================================ */

static bool translate_time_format(const char* fmt, char* out, size_t out_size) {
    int i = 0, o = 0;
    int end = (int)strlen(fmt);
    int out_end = (int)out_size - 1;

    while (i < end && o < out_end) {
        char c = fmt[i];

        switch (c) {
            case 'y': {
                int n = 0;
                while (i + n < end && fmt[i + n] == 'y')
                    n++;
                if (n == 4) {
                    out[o++] = '%';
                    out[o++] = 'Y';
                    i += 4;
                } else if (n == 2) {
                    out[o++] = '%';
                    out[o++] = 'y';
                    i += 2;
                } else
                    return false;
                continue;
            }
            case 'M':
                if (i + 1 >= end || fmt[i + 1] != 'M')
                    return false;
                out[o++] = '%';
                out[o++] = 'm';
                i += 2;
                continue;
            case 'd':
                if (i + 1 >= end || fmt[i + 1] != 'd')
                    return false;
                out[o++] = '%';
                out[o++] = 'd';
                i += 2;
                continue;
            case 'h':
                if (i + 1 >= end || fmt[i + 1] != 'h')
                    return false;
                out[o++] = '%';
                out[o++] = 'H';
                i += 2;
                continue;
            case 'm':
                if (i + 1 >= end || fmt[i + 1] != 'm')
                    return false;
                out[o++] = '%';
                out[o++] = 'M';
                i += 2;
                continue;
            case 's':
                if (i + 1 >= end || fmt[i + 1] != 's')
                    return false;
                out[o++] = '%';
                out[o++] = 'S';
                i += 2;
                continue;
            default:
                out[o++] = fmt[i++];
                break;
        }
    }

    out[o] = '\0';
    return true;
}

static bool clk_clock_format_current_time(const char* time_format, char* buffer,
                                          size_t buffer_size) {
    if (!time_format || !buffer || buffer_size == 0)
        return false;

    char translated[128];
    if (!translate_time_format(time_format, translated, sizeof(translated)))
        return false;

    time_t rawtime;
    struct tm timeinfo;
    time(&rawtime);
#if defined(_WIN32) || defined(_WIN64)
    if (localtime_s(&timeinfo, &rawtime) != 0)
        return false;
#else
    if (!localtime_r(&rawtime, &timeinfo))
        return false;
#endif

    return strftime(buffer, buffer_size, translated, &timeinfo) > 0;
}

/* ================================================================
 *  File I/O helper
 * ================================================================ */

static bool clk_clock_read_file(const char* path, char** out_content, size_t* out_size) {
    *out_content = NULL;
    *out_size = 0;

    FILE* file = fopen(path, "rb");
    if (!file)
        return false;

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return false;
    }

    long file_size = ftell(file);
    if (file_size < 0) {
        fclose(file);
        return false;
    }

    rewind(file);

    char* content = malloc(file_size + 1);
    if (!content) {
        fclose(file);
        return false;
    }

    size_t read_size = fread(content, 1, file_size, file);
    fclose(file);

    if (read_size != (size_t)file_size) {
        free(content);
        return false;
    }

    content[file_size] = '\0';
    *out_content = content;
    *out_size = (size_t)file_size;
    return true;
}

/* ================================================================
 *  Font config — JSON → styles + cell templates + glyph textures
 * ================================================================ */

/** Convert a JSON cell definition into a clk_cell using the
 *  style-name → style-id translator. */
static bool clk_clock_trans_json_cell_to_clk_cell(const clk_json_value* json_cell,
                                                  clk_cell* out_cell,
                                                  const clk_json_value* style_int_translator) {
    if (!json_cell || !out_cell || !style_int_translator)
        return false;

    memset(out_cell, 0, sizeof(*out_cell));
    out_cell->is_empty = true;

    /* "empty": true → nothing to render, return early */
    struct clk_json_value* empty = clk_json_object_get(json_cell, "empty");
    if (empty && clk_json_is_true(empty))
        return true;

    /* "char" → cell_tex (null-terminated UTF-8) */
    const char* ch_str = NULL;
    struct clk_json_value* ch = clk_json_object_get(json_cell, "char");
    if (!ch || !clk_json_is_string(ch) || clk_json_get_string(ch, &ch_str) != 0)
        return false;
    size_t len = strlen(ch_str);
    if (len > 4)
        return false;
    memcpy(out_cell->cell_tex, ch_str, len);
    out_cell->cell_tex[len] = '\0';

    /* "style" → look up in translator → style_id */
    const char* style_name = NULL;
    struct clk_json_value* style = clk_json_object_get(json_cell, "style");
    if (!style || !clk_json_is_string(style) || clk_json_get_string(style, &style_name) != 0)
        return false;

    struct clk_json_value* sid = clk_json_object_get(style_int_translator, style_name);
    double sid_val;
    if (!sid || !clk_json_is_number(sid) || clk_json_get_number(sid, &sid_val) != 0)
        return false;
    out_cell->style_id = (int)sid_val;

    /* "wide": true → CELL_WIDE_LEAD, otherwise CELL_NORMAL */
    struct clk_json_value* wide = clk_json_object_get(json_cell, "wide");
    out_cell->type = (wide && clk_json_is_true(wide)) ? CELL_WIDE_LEAD : CELL_NORMAL;

    out_cell->is_empty = false;
    return true;
}

/** Register every style from the JSON "styles" object with the
 *  term and build a style-name → style-id lookup table. */
static bool clk_clock_register_config_style(const clk_json_value* styles_json,
                                            clk_json_value* translator) {
    clk_json_object_iterator iter;
    if (clk_json_object_iterator_init(styles_json, &iter) != 0)
        return false;

    clk_json_key_value_pair pair;
    while (clk_json_object_iterator_next(&iter, &pair)) {
        char* name = pair.key;
        struct clk_json_value* s = pair.value;

        const char* fg_str = NULL;
        struct clk_json_value* fg = clk_json_object_get(s, "fg");
        if (!fg || !clk_json_is_string(fg) || clk_json_get_string(fg, &fg_str) != 0)
            return false;

        const char* bg_str = NULL;
        struct clk_json_value* bg = clk_json_object_get(s, "bg");
        if (!bg || !clk_json_is_string(bg) || clk_json_get_string(bg, &bg_str) != 0)
            return false;

        const char* attr_str = NULL;
        struct clk_json_value* attr = clk_json_object_get(s, "attr");
        if (!attr || !clk_json_is_string(attr) || clk_json_get_string(attr, &attr_str) != 0)
            return false;

        int id = clk_term_register_style_hex(fg_str, bg_str, attr_str);

        if (clk_json_object_set_number(translator, name, id) != 0)
            return false;
    }
    return true;
}

/** Build the 11 glyph textures from the parsed JSON.
 *
 *  Two-phase: first iterate the "cells" object to build a
 *  char → clk_cell lookup table (one JSON parse per unique cell);
 *  then iterate "glyphs" and fill each texture by indexing the
 *  table by character. */
static bool clk_clock_load_font_texture(const clk_json_value* json, clk_clock* clk, int glyph_w,
                                        int glyph_h) {
    struct clk_json_value* json_glyphs = clk_json_object_get(json, "glyphs");
    if (!json_glyphs || !clk_json_is_object(json_glyphs) ||
        clk_json_object_count(json_glyphs) != 11)
        return false;

    struct clk_json_value* styles_json = clk_json_object_get(json, "styles");

    /* --- build style-name → style-id translator --- */
    struct clk_json_value* translator = clk_json_create_object();
    if (!clk_clock_register_config_style(styles_json, translator)) {
        clk_json_free(translator);
        return false;
    }

    /* --- build cell lookup table: char → clk_cell --- */
    clk_cell cell_table[256] = {0};
    for (int i = 0; i < 256; i++)
        cell_table[i].is_empty = true;

    struct clk_json_value* json_cells = clk_json_object_get(json, "cells");
    if (!json_cells) {
        clk_json_free(translator);
        return false;
    }

    clk_json_object_iterator cell_iter;
    if (clk_json_object_iterator_init(json_cells, &cell_iter) != 0) {
        clk_json_free(translator);
        return false;
    }
    clk_json_key_value_pair cp;
    while (clk_json_object_iterator_next(&cell_iter, &cp)) {
        unsigned char idx = (unsigned char)cp.key[0];
        if (!clk_clock_trans_json_cell_to_clk_cell(cp.value, &cell_table[idx], translator)) {
            clk_json_free(translator);
            return false;
        }
    }

    /* --- populate glyph textures --- */
    clk_json_object_iterator iter;
    if (clk_json_object_iterator_init(json_glyphs, &iter) != 0) {
        clk_json_free(translator);
        return false;
    }

    clk_json_key_value_pair pair;
    while (clk_json_object_iterator_next(&iter, &pair)) {
        char key = pair.key[0];
        int slot = (key >= '0' && key <= '9') ? (key - '0') : (key == ':' ? 10 : -1);
        if (slot == -1) {
            clk_json_free(translator);
            return false;
        }

        clk->clk_clock_num_font_texture[slot] = clk_texture_create(glyph_w, glyph_h);

        struct clk_json_value* gv = pair.value;
        if (clk_json_get_type(gv) != JSON_ARRAY) {
            clk_json_free(translator);
            return false;
        }

        for (int y = 0; y < glyph_h; ++y) {
            struct clk_json_value* row = clk_json_array_get(gv, y);
            const char* str = NULL;
            if (clk_json_get_string(row, &str) != 0) {
                clk_json_free(translator);
                return false;
            }
            for (int x = 0; x < glyph_w; ++x) {
                const clk_cell* c = &cell_table[(unsigned char)str[x]];
                if (!c->is_empty)
                    clk_texture_set_cell(&clk->clk_clock_num_font_texture[slot], x, y, c);
                /* wide pair: skip the trailing marker */
                if (c->type == CELL_WIDE_LEAD)
                    ++x;
            }
        }
    }

    clk_json_free(translator);
    return true;
}

/** Top-level font loader: read file, parse JSON, extract metrics,
 *  then delegate to clk_clock_load_font_texture. */
static bool clk_clock_load_font_config(clk_clock* clk) {
    char* file_content = NULL;
    size_t file_size = 0;
    if (!clk_clock_read_file(clk->clk_clock_font_path, &file_content, &file_size))
        return false;

    struct clk_json_value* json = clk_json_parse(file_content);
    if (!json) {
        free(file_content);
        return false;
    }

    double spacing = 0;
    double fw = 0, fh = 0;

    struct clk_json_value* js = clk_json_object_get(json, "glyph_spacing");
    struct clk_json_value* jw = clk_json_object_get(json, "font_w");
    struct clk_json_value* jh = clk_json_object_get(json, "font_h");

    if (!js || !clk_json_is_number(js) || !jw || !clk_json_is_number(jw) || !jh ||
        !clk_json_is_number(jh) || clk_json_get_number(js, &spacing) != 0 ||
        clk_json_get_number(jw, &fw) != 0 || clk_json_get_number(jh, &fh) != 0) {
        clk_json_free(json);
        free(file_content);
        return false;
    }

    clk->clk_clock_glyph_spacing = (int)spacing;

    if (!clk_clock_load_font_texture(json, clk, (int)fw, (int)fh)) {
        clk_json_free(json);
        free(file_content);
        return false;
    }

    free(file_content);
    clk_json_free(json);
    return true;
}

/* ================================================================
 *  Public API
 * ================================================================ */

bool clk_clock_create(clk_clock* clk, const char* time_format, const char* font_path) {
    if (!clk || !time_format || !font_path)
        return false;

    /* zero-initialise so destroy() is safe on partial construction */
    memset(clk, 0, sizeof(*clk));

    size_t fmt_len = strlen(time_format);
    if (fmt_len >= CLK_CLOCK_TIME_FORMAT_MAX_LENGTH)
        return false;
    memcpy(clk->clk_clock_time_format, time_format, fmt_len + 1);

    clk->clk_clock_font_path = strdup(font_path);
    if (!clk->clk_clock_font_path) {
        clk_clock_destroy(clk);
        return false;
    }

    if (!clk_clock_load_font_config(clk)) {
        clk_clock_destroy(clk);
        return false;
    }

    clk->clk_clock_sprite_count = strlen(clk->clk_clock_time_format);
    clk->clk_clock_sprite_capacity = clk->clk_clock_sprite_count * 2;
    clk_sprite** sprites = malloc(clk->clk_clock_sprite_capacity * sizeof(clk_sprite*));
    if (!sprites) {
        clk_clock_destroy(clk);
        return false;
    }
    memset(sprites, 0, clk->clk_clock_sprite_capacity * sizeof(clk_sprite*));
    clk->clk_clock_sprites = sprites;

    for (size_t i = 0; i < clk->clk_clock_sprite_count; ++i) {
        clk_sprite* s = malloc(sizeof(clk_sprite));
        if (!s) {
            clk_clock_destroy(clk);
            return false;
        }
        memset(s, 0, sizeof(clk_sprite));
        s->is_invalid = true;
        clk->clk_clock_sprites[i] = s;
    }

    clk->posx = 0;
    clk->posy = 0;
    clk->clk_clock_z_order = 0;
    clk_clock_update(clk);
    return true;
}

void clk_clock_destroy(clk_clock* clk) {
    if (!clk)
        return;

    free(clk->clk_clock_font_path);
    clk->clk_clock_font_path = NULL;

    for (int i = 0; i < CLK_CLOCK_NUM_TEXTURE_COUNT; ++i)
        clk_texture_destroy(&clk->clk_clock_num_font_texture[i]);

    for (size_t i = 0; i < clk->clk_clock_sprite_count; ++i) {
        clk_term_remove_sprite(clk->clk_clock_sprites[i]);
        free(clk->clk_clock_sprites[i]);
    }
    free(clk->clk_clock_sprites);

    clk->clk_clock_sprites = NULL;
    clk->clk_clock_sprite_count = 0;
    clk->clk_clock_sprite_capacity = 0;
}

bool clk_clock_reload_config(clk_clock* clk) {
    if (!clk)
        return false;

    for (int i = 0; i < CLK_CLOCK_NUM_TEXTURE_COUNT; ++i)
        clk_texture_destroy(&clk->clk_clock_num_font_texture[i]);

    if (!clk_clock_load_font_config(clk))
        return false;

    clk_clock_update(clk);
    return true;
}

bool clk_clock_change_time_format(clk_clock* clk, const char* new_time_format) {
    if (!clk || !new_time_format)
        return false;

    size_t new_len = strlen(new_time_format);
    if (new_len >= CLK_CLOCK_TIME_FORMAT_MAX_LENGTH)
        return false;

    /* copy format string */
    memset(clk->clk_clock_time_format, 0, sizeof(clk->clk_clock_time_format));
    memcpy(clk->clk_clock_time_format, new_time_format, new_len + 1);

    /* expand sprite array if the new format is longer */
    if (new_len > clk->clk_clock_sprite_count) {
        if (new_len > clk->clk_clock_sprite_capacity) {
            size_t new_cap = new_len * 2;
            clk_sprite** sp = realloc(clk->clk_clock_sprites, new_cap * sizeof(clk_sprite*));
            if (!sp)
                return false;
            memset(sp + clk->clk_clock_sprite_capacity, 0,
                   (new_cap - clk->clk_clock_sprite_capacity) * sizeof(clk_sprite*));
            clk->clk_clock_sprites = sp;
            clk->clk_clock_sprite_capacity = new_cap;
        }

        for (size_t i = clk->clk_clock_sprite_count; i < new_len; ++i) {
            clk_sprite* s = malloc(sizeof(clk_sprite));
            if (!s)
                return false;
            memset(s, 0, sizeof(clk_sprite));
            s->is_invalid = true;
            s->z_order = clk->clk_clock_z_order;
            clk->clk_clock_sprites[i] = s;
            clk_term_add_sprite(s);
        }
    }

    clk->clk_clock_sprite_count = new_len;
    return true;
}

bool clk_clock_change_font_path(clk_clock* clk, const char* new_font_path) {
    if (!clk || !new_font_path)
        return false;

    free(clk->clk_clock_font_path);
    clk->clk_clock_font_path = strdup(new_font_path);
    if (!clk->clk_clock_font_path)
        return false;

    return clk_clock_reload_config(clk);
}

/* ================================================================
 *  Update
 * ================================================================ */

void clk_clock_update(clk_clock* clk) {
    if (!clk)
        return;

    char time_str[CLK_CLOCK_TIME_FORMAT_MAX_LENGTH];
    if (!clk_clock_format_current_time(clk->clk_clock_time_format, time_str, sizeof(time_str)))
        return;

    /* all glyph textures share the same dimensions */
    int font_w = clk->clk_clock_num_font_texture[0].tex_w;
    int font_h = clk->clk_clock_num_font_texture[0].tex_h;
    int spacing = (int)clk->clk_clock_glyph_spacing;

    size_t len = strlen(time_str);
    int col = 0, row = 0;

    for (size_t i = 0; i < len; ++i) {
        clk_sprite* s = clk->clk_clock_sprites[i];
        char ch = time_str[i];

        /* newline → next row */
        if (ch == '\n') {
            s->is_invalid = true;
            row++;
            col = 0;
            continue;
        }

        if ((ch >= '0' && ch <= '9') || ch == ':') {
            int index = (ch == ':') ? 10 : (ch - '0');
            s->tex = &clk->clk_clock_num_font_texture[index];
            s->posx = clk->posx + col * (font_w + spacing);
            s->posy = clk->posy + row * font_h;
            s->z_order = clk->clk_clock_z_order;
            s->is_invalid = false;
        } else {
            /* space / literal — keep gap but render nothing */
            s->is_invalid = true;
        }
        col++;
    }

    /* hide sprites beyond the current time string */
    for (size_t i = len; i < clk->clk_clock_sprite_count; ++i)
        clk->clk_clock_sprites[i]->is_invalid = true;
}

/* ================================================================
 *  Query
 * ================================================================ */

bool clk_clock_get_font_texture_size(const clk_clock* clk, int* width, int* height) {
    if (!clk || !width || !height)
        return false;
    const clk_texture* tex = &clk->clk_clock_num_font_texture[0];
    *width = tex->tex_w;
    *height = tex->tex_h;
    return true;
}

bool clk_clock_get_sprite_size(const clk_clock* clk, int* width, int* height) {
    int fw, fh;
    if (!clk_clock_get_font_texture_size(clk, &fw, &fh))
        return false;

    int spacing = (int)clk->clk_clock_glyph_spacing;
    int max_cols = 0, cur_cols = 0, rows = 1;
    const char* p = clk->clk_clock_time_format;
    for (; *p; ++p) {
        if (*p == '\n') {
            if (cur_cols > max_cols)
                max_cols = cur_cols;
            cur_cols = 0;
            rows++;
        } else {
            cur_cols++;
        }
    }
    if (cur_cols > max_cols)
        max_cols = cur_cols;

    *width = max_cols * fw + (max_cols > 0 ? max_cols - 1 : 0) * spacing;
    *height = rows * fh;
    return true;
}

bool clk_clock_get_sprite_pos(const clk_clock* clk, int* px, int* py) {
    if (!clk || !px || !py)
        return false;
    *px = clk->posx;
    *py = clk->posy;
    return true;
}

bool clk_clock_set_sprite_pos(clk_clock* clk, int px, int py) {
    if (!clk)
        return false;
    clk->posx = px;
    clk->posy = py;
    return true;
}

void clk_clock_set_z_order(clk_clock* clk, int z) {
    if (!clk)
        return;
    clk->clk_clock_z_order = z;
    for (size_t i = 0; i < clk->clk_clock_sprite_count; ++i)
        clk_sprite_set_z(clk->clk_clock_sprites[i], z);
}

/* ================================================================
 *  Render list
 * ================================================================ */

bool clk_clock_add_to_term(clk_clock* clk) {
    if (!clk)
        return false;
    for (size_t i = 0; i < clk->clk_clock_sprite_count; ++i)
        clk_term_add_sprite(clk->clk_clock_sprites[i]);
    return true;
}
