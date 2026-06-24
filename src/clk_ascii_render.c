#include "clk_ascii_render.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "clk_file_util.h"
#include "clk_json.h"
#include "clk_term.h"

/* ================================================================
 *  Font config — JSON → styles + cell templates + glyph textures
 * ================================================================ */

/** Convert a JSON cell definition into a clk_cell using the
 *  style-name → style-id translator. */
static bool trans_json_cell(const clk_json_value* json_cell, clk_cell* out_cell,
                            const clk_json_value* style_translator) {
    if (!json_cell || !out_cell || !style_translator)
        return false;

    memset(out_cell, 0, sizeof(*out_cell));
    out_cell->is_empty = true;

    struct clk_json_value* empty = clk_json_object_get(json_cell, "empty");
    if (empty && clk_json_is_true(empty))
        return true;

    const char* ch_str = NULL;
    struct clk_json_value* ch = clk_json_object_get(json_cell, "char");
    if (!ch || !clk_json_is_string(ch) || clk_json_get_string(ch, &ch_str) != 0)
        return false;
    size_t len = strlen(ch_str);
    if (len > 4)
        return false;
    memcpy(out_cell->cell_tex, ch_str, len);
    out_cell->cell_tex[len] = '\0';

    const char* style_name = NULL;
    struct clk_json_value* style = clk_json_object_get(json_cell, "style");
    if (!style || !clk_json_is_string(style) || clk_json_get_string(style, &style_name) != 0)
        return false;

    struct clk_json_value* style_id_json = clk_json_object_get(style_translator, style_name);
    double style_id_num;
    if (!style_id_json || !clk_json_is_number(style_id_json) ||
        clk_json_get_number(style_id_json, &style_id_num) != 0)
        return false;
    out_cell->style_id = (int)style_id_num;

    struct clk_json_value* wide = clk_json_object_get(json_cell, "wide");
    out_cell->type = (wide && clk_json_is_true(wide)) ? CELL_WIDE_LEAD : CELL_NORMAL;

    out_cell->is_empty = false;
    return true;
}

/** Register every style from the JSON "styles" object with the
 *  term and build a style-name → style-id lookup table. */
static bool register_styles(const clk_json_value* styles_json, clk_json_value* translator) {
    clk_json_object_iterator iter;
    if (clk_json_object_iterator_init(styles_json, &iter) != 0)
        return false;

    clk_json_key_value_pair pair;
    while (clk_json_object_iterator_next(&iter, &pair)) {
        char* name = pair.key;
        struct clk_json_value* style_json = pair.value;

        const char* fg_str = NULL;
        struct clk_json_value* fg = clk_json_object_get(style_json, "fg");
        if (!fg || !clk_json_is_string(fg) || clk_json_get_string(fg, &fg_str) != 0)
            return false;

        const char* bg_str = NULL;
        struct clk_json_value* bg = clk_json_object_get(style_json, "bg");
        if (!bg || !clk_json_is_string(bg) || clk_json_get_string(bg, &bg_str) != 0)
            return false;

        const char* attr_str = NULL;
        struct clk_json_value* attr = clk_json_object_get(style_json, "attr");
        if (!attr || !clk_json_is_string(attr) || clk_json_get_string(attr, &attr_str) != 0)
            return false;

        int id = clk_term_register_style_hex(fg_str, bg_str, attr_str);
        if (id == 0)
            return false;

        if (clk_json_object_set_number(translator, name, id) != 0)
            return false;
    }
    return true;
}

/* ================================================================
 *  Cell / glyph lookup
 * ================================================================ */

typedef struct {
    char key[5];
    clk_cell cell;
} clk_cell_entry;

/** Search for a UTF-8 character in the cell entry table.
 *  Returns a pointer to the matching cell, or a sentinel empty cell
 *  if not found. */
static const clk_cell* cell_lookup(const char* utf8_char, const clk_cell_entry* entries,
                                   int entry_count) {
    for (int i = 0; i < entry_count; ++i)
        if (strcmp(entries[i].key, utf8_char) == 0)
            return &entries[i].cell;
    static clk_cell empty = {.is_empty = true};
    return &empty;
}

/** Search for a character in the glyph table.
 *  Returns a pointer to the matching texture, or NULL if not found. */
static clk_texture* glyph_lookup(const clk_glyph* glyphs, int glyph_count, char character) {
    char key[5] = {character, '\0'};
    for (int i = 0; i < glyph_count; ++i)
        if (strcmp(glyphs[i].key, key) == 0)
            return (clk_texture*)&glyphs[i].texture;
    return NULL;
}

/* ================================================================
 *  Glyph texture loading
 * ================================================================ */

/** Build glyph textures from the parsed JSON.
 *
 *  Iterates the "glyphs" object — each key is a character (or UTF-8
 *  sequence), each value is a 2D array of cell keys.  A separate
 *  "cells" object maps cell keys to styled clk_cell definitions. */
static bool load_glyph_textures(const clk_json_value* json, clk_ascii_render* render, int glyph_w,
                                int glyph_h) {
    struct clk_json_value* json_glyphs = clk_json_object_get(json, "glyphs");
    if (!json_glyphs || !clk_json_is_object(json_glyphs))
        return false;

    struct clk_json_value* styles_json = clk_json_object_get(json, "styles");

    struct clk_json_value* translator = clk_json_create_object();
    if (!register_styles(styles_json, translator)) {
        clk_json_free(translator);
        return false;
    }

    clk_cell_entry* cell_entries = NULL;
    int cell_entry_count = 0;

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
    clk_json_key_value_pair cell_pair;
    while (clk_json_object_iterator_next(&cell_iter, &cell_pair)) {
        cell_entries = realloc(cell_entries, (cell_entry_count + 1) * sizeof(*cell_entries));
        if (!cell_entries) {
            clk_json_free(translator);
            return false;
        }
        memset(cell_entries[cell_entry_count].key, 0, 5);
        strncpy(cell_entries[cell_entry_count].key, cell_pair.key, 4);
        if (!trans_json_cell(cell_pair.value, &cell_entries[cell_entry_count].cell, translator)) {
            free(cell_entries);
            clk_json_free(translator);
            return false;
        }
        cell_entry_count++;
    }

    int glyph_count = (int)clk_json_object_count(json_glyphs);
    clk_glyph* glyphs = calloc(glyph_count, sizeof(clk_glyph));
    if (!glyphs) {
        free(cell_entries);
        clk_json_free(translator);
        return false;
    }

    clk_json_object_iterator iter;
    if (clk_json_object_iterator_init(json_glyphs, &iter) != 0) {
        free(glyphs);
        free(cell_entries);
        clk_json_free(translator);
        return false;
    }

    clk_json_key_value_pair pair;
    int slot = 0;
    while (clk_json_object_iterator_next(&iter, &pair)) {
        memset(glyphs[slot].key, 0, 5);
        strncpy(glyphs[slot].key, pair.key, 4);
        glyphs[slot].texture = clk_texture_create(glyph_w, glyph_h);

        struct clk_json_value* glyph_value = pair.value;
        if (clk_json_get_type(glyph_value) != JSON_ARRAY) {
            for (int i = 0; i <= slot; ++i)
                clk_texture_destroy(&glyphs[i].texture);
            free(glyphs);
            free(cell_entries);
            clk_json_free(translator);
            return false;
        }

        for (int y = 0; y < glyph_h; ++y) {
            struct clk_json_value* row_value = clk_json_array_get(glyph_value, y);
            const char* str = NULL;
            if (clk_json_get_string(row_value, &str) != 0) {
                for (int i = 0; i <= slot; ++i)
                    clk_texture_destroy(&glyphs[i].texture);
                free(glyphs);
                free(cell_entries);
                clk_json_free(translator);
                return false;
            }
            int byte_pos = 0, cell_x = 0;
            while (cell_x < glyph_w && str[byte_pos]) {
                unsigned char first_byte = (unsigned char)str[byte_pos];
                int byte_count = 1;
                if ((first_byte & 0x80) == 0)
                    byte_count = 1;
                else if ((first_byte & 0xE0) == 0xC0)
                    byte_count = 2;
                else if ((first_byte & 0xF0) == 0xE0)
                    byte_count = 3;
                else if ((first_byte & 0xF8) == 0xF0)
                    byte_count = 4;

                char ch_buf[5] = {0};
                memcpy(ch_buf, str + byte_pos, byte_count);
                const clk_cell* cell = cell_lookup(ch_buf, cell_entries, cell_entry_count);
                if (!cell->is_empty)
                    clk_texture_set_cell(&glyphs[slot].texture, cell_x, y, cell);
                if (cell->type == CELL_WIDE_LEAD)
                    cell_x++;
                byte_pos += byte_count;
                cell_x++;
            }
        }
        slot++;
    }

    render->glyphs = glyphs;
    render->glyph_count = glyph_count;

    free(cell_entries);
    clk_json_free(translator);
    return true;
}

static bool load_font_config(clk_ascii_render* render) {
    size_t file_size = 0;
    char* file_content = clk_file_read_all(render->font_path, &file_size);
    if (!file_content)
        return false;

    struct clk_json_value* json = clk_json_parse(file_content);
    free(file_content);
    if (!json)
        return false;

    double spacing = 0, glyph_w = 0, glyph_h = 0, line_spacing = 0;
    struct clk_json_value* field;
    bool ok = true;

    field = clk_json_object_get(json, "glyph_spacing");
    ok = ok && field && clk_json_is_number(field) && clk_json_get_number(field, &spacing) == 0;

    field = clk_json_object_get(json, "font_w");
    ok = ok && field && clk_json_is_number(field) && clk_json_get_number(field, &glyph_w) == 0;

    field = clk_json_object_get(json, "font_h");
    ok = ok && field && clk_json_is_number(field) && clk_json_get_number(field, &glyph_h) == 0;

    field = clk_json_object_get(json, "line_spacing");
    ok = ok && field && clk_json_is_number(field) && clk_json_get_number(field, &line_spacing) == 0;

    if (!ok) {
        clk_json_free(json);
        return false;
    }

    render->glyph_spacing = (int)spacing;
    render->line_spacing = (int)line_spacing;

    if (!load_glyph_textures(json, render, (int)glyph_w, (int)glyph_h)) {
        clk_json_free(json);
        return false;
    }

    clk_json_free(json);
    return true;
}

/* ================================================================
 *  Public API
 * ================================================================ */

bool clk_ascii_render_create(clk_ascii_render* render, const char* font_path) {
    if (!render || !font_path)
        return false;

    memset(render, 0, sizeof(*render));

    render->font_path = strdup(font_path);
    if (!render->font_path) {
        clk_ascii_render_destroy(render);
        return false;
    }

    if (!load_font_config(render)) {
        clk_ascii_render_destroy(render);
        return false;
    }

    return true;
}

void clk_ascii_render_destroy(clk_ascii_render* render) {
    if (!render)
        return;

    free(render->font_path);
    render->font_path = NULL;

    for (int i = 0; i < render->glyph_count; ++i)
        clk_texture_destroy(&render->glyphs[i].texture);
    free(render->glyphs);
    render->glyphs = NULL;
    render->glyph_count = 0;

    for (size_t i = 0; i < render->sprite_capacity; ++i) {
        if (render->sprites[i])
            clk_sprite_destroy(render->sprites[i]);
    }
    free(render->sprites);
    render->sprites = NULL;
    render->sprite_count = 0;
    render->sprite_capacity = 0;
}

bool clk_ascii_render_reload(clk_ascii_render* render) {
    if (!render)
        return false;

    for (int i = 0; i < render->glyph_count; ++i)
        clk_texture_destroy(&render->glyphs[i].texture);
    free(render->glyphs);
    render->glyphs = NULL;
    render->glyph_count = 0;

    return load_font_config(render);
}

/* ================================================================
 *  Sprite array management (internal)
 * ================================================================ */

static bool ensure_sprite_count(clk_ascii_render* render, size_t needed) {
    if (needed <= render->sprite_count)
        return true;

    while (needed > render->sprite_capacity) {
        size_t new_cap = render->sprite_capacity ? render->sprite_capacity * 2 : needed * 2;
        clk_sprite** new_sprites = realloc(render->sprites, new_cap * sizeof(clk_sprite*));
        if (!new_sprites)
            return false;
        memset(new_sprites + render->sprite_capacity, 0,
               (new_cap - render->sprite_capacity) * sizeof(clk_sprite*));
        render->sprites = new_sprites;
        render->sprite_capacity = new_cap;
    }

    for (size_t i = render->sprite_count; i < needed; ++i) {
        clk_sprite* sprite = clk_sprite_create();
        if (!sprite)
            return false;
        sprite->is_hidden = true;
        sprite->z_order = render->z_order;
        render->sprites[i] = sprite;
        clk_term_add_sprite(sprite);
    }

    render->sprite_count = needed;
    return true;
}

/* ================================================================
 *  Runtime
 * ================================================================ */

void clk_ascii_render_update(clk_ascii_render* render, const char* string) {
    if (!render || !string)
        return;

    size_t len = strlen(string);
    if (!ensure_sprite_count(render, len))
        return;

    if (render->glyph_count == 0)
        return;

    int font_w = render->glyphs[0].texture.tex_w;
    int font_h = render->glyphs[0].texture.tex_h;
    int spacing = render->glyph_spacing;
    int line_spacing = render->line_spacing;

    int col = 0, row = 0;

    for (size_t i = 0; i < len; ++i) {
        clk_sprite* sprite = render->sprites[i];
        char character = string[i];

        if (character == '\n') {
            sprite->is_hidden = true;
            sprite->tex = NULL;
            row++;
            col = 0;
            continue;
        }

        clk_texture* tex = glyph_lookup(render->glyphs, render->glyph_count, character);
        if (tex) {
            sprite->tex = tex;
            sprite->posx = render->pos_x + col * (font_w + spacing);
            sprite->posy = render->pos_y + row * (font_h + line_spacing);
            clk_sprite_set_z(sprite, render->z_order);
            sprite->is_hidden = false;
            sprite->is_invalid = false;
        } else {
            sprite->is_hidden = true;
            sprite->tex = NULL;
        }
        col++;
    }

    for (size_t i = len; i < render->sprite_capacity; ++i) {
        if (render->sprites[i]) {
            render->sprites[i]->is_hidden = true;
            render->sprites[i]->tex = NULL;
        }
    }
}

/* ================================================================
 *  Configuration
 * ================================================================ */

bool clk_ascii_render_change_font(clk_ascii_render* render, const char* new_font_path) {
    if (!render || !new_font_path)
        return false;

    free(render->font_path);
    render->font_path = strdup(new_font_path);
    if (!render->font_path)
        return false;

    return clk_ascii_render_reload(render);
}

/* ================================================================
 *  Query
 * ================================================================ */

bool clk_ascii_render_get_glyph_size(const clk_ascii_render* render, int* width, int* height) {
    if (!render || !width || !height || render->glyph_count == 0)
        return false;
    *width = render->glyphs[0].texture.tex_w;
    *height = render->glyphs[0].texture.tex_h;
    return true;
}

bool clk_ascii_render_get_size(const clk_ascii_render* render, const char* string, int* width,
                               int* height) {
    int glyph_w, glyph_h;
    if (!clk_ascii_render_get_glyph_size(render, &glyph_w, &glyph_h))
        return false;

    int spacing = render->glyph_spacing;
    int max_cols = 0, cur_cols = 0, rows = 1;
    const char* p = string;
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

    *width = max_cols * glyph_w + (max_cols > 0 ? max_cols - 1 : 0) * spacing;
    *height = rows * glyph_h + (rows > 1 ? rows - 1 : 0) * render->line_spacing;
    return true;
}

bool clk_ascii_render_get_pos(const clk_ascii_render* render, int* px, int* py) {
    if (!render || !px || !py)
        return false;
    *px = render->pos_x;
    *py = render->pos_y;
    return true;
}

void clk_ascii_render_set_pos(clk_ascii_render* render, int px, int py) {
    if (!render)
        return;
    render->pos_x = px;
    render->pos_y = py;
}

void clk_ascii_render_set_z_order(clk_ascii_render* render, int z) {
    if (!render)
        return;
    render->z_order = z;
    for (size_t i = 0; i < render->sprite_count; ++i)
        clk_sprite_set_z(render->sprites[i], z);
}

int clk_ascii_render_get_z_order(const clk_ascii_render* render) {
    return render ? render->z_order : 0;
}

/* ================================================================
 *  Rendering
 * ================================================================ */

bool clk_ascii_render_add_to_term(clk_ascii_render* render) {
    if (!render)
        return false;
    for (size_t i = 0; i < render->sprite_count; ++i)
        clk_term_add_sprite(render->sprites[i]);
    return true;
}
