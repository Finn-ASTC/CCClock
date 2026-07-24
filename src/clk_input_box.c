#include "clk_input_box.h"

#include <stdlib.h>
#include <string.h>

#include "clk_term.h"

#define CLK_INPUT_BOX_COL_MAX (CLK_INPUT_BOX_WIDTH - 7)

#define CLK_INPUT_BOX_FG_R 255
#define CLK_INPUT_BOX_FG_G 255
#define CLK_INPUT_BOX_FG_B 255
#define CLK_INPUT_BOX_BG_R 30
#define CLK_INPUT_BOX_BG_G 30
#define CLK_INPUT_BOX_BG_B 30

#define CLK_INPUT_BOX_PROMPT_X 2
#define CLK_INPUT_BOX_PROMPT_Y 2
#define CLK_INPUT_BOX_TEXT_X 4
#define CLK_INPUT_BOX_TEXT_Y 2

struct clk_input_box {
    clk_texture tex;
    clk_sprite* sprite;
    bool sprite_added;
    char* buffer;
    size_t max_length;
    size_t length;
    size_t cursor_position;
    bool finished;
    bool confirmed;
    int border_style;
    int text_style;
};

static void draw_border(clk_texture* tex, int style_id) {
    int w = tex->tex_w;
    int h = tex->tex_h;

    clk_texture_write_cell(tex, 0, 0, "┌", style_id);
    for (int i = 1; i < w - 1; ++i)
        clk_texture_write_cell(tex, i, 0, "─", style_id);
    clk_texture_write_cell(tex, w - 1, 0, "┐", style_id);

    for (int i = 1; i < h - 1; ++i) {
        clk_texture_write_cell(tex, 0, i, "│", style_id);
        clk_texture_write_cell(tex, w - 1, i, "│", style_id);
    }

    clk_texture_write_cell(tex, 0, h - 1, "└", style_id);
    for (int i = 1; i < w - 1; ++i)
        clk_texture_write_cell(tex, i, h - 1, "─", style_id);
    clk_texture_write_cell(tex, w - 1, h - 1, "┘", style_id);
}

clk_input_box* clk_input_box_create(const char* initial, size_t max_length) {
    clk_input_box* box = calloc(1, sizeof(clk_input_box));
    if (!box)
        return NULL;

    box->max_length = max_length;
    box->buffer = malloc(max_length + 1);
    if (!box->buffer) {
        free(box);
        return NULL;
    }
    memset(box->buffer, 0, max_length + 1);

    if (initial)
        strncpy(box->buffer, initial, max_length);
    box->length = strlen(box->buffer);
    box->cursor_position = box->length;

    box->tex = clk_texture_create(CLK_INPUT_BOX_WIDTH, CLK_INPUT_BOX_HEIGHT);
    box->sprite = clk_sprite_create_with_texture(&box->tex, 0, 0, 0);
    box->border_style = clk_term_register_style_rgb(
        CLK_INPUT_BOX_FG_R, CLK_INPUT_BOX_FG_G, CLK_INPUT_BOX_FG_B, CLK_INPUT_BOX_BG_R,
        CLK_INPUT_BOX_BG_G, CLK_INPUT_BOX_BG_B, CLK_ATTR_NONE);
    box->text_style = box->border_style;

    return box;
}

void clk_input_box_destroy(clk_input_box* box) {
    if (!box)
        return;
    clk_input_box_remove_from_term(box);
    clk_texture_destroy(&box->tex);
    clk_sprite_destroy(box->sprite);
    free(box->buffer);
    free(box);
}

void clk_input_box_get_buffer(clk_input_box* box, char** buf, size_t* max, size_t** len,
                              size_t** pos) {
    *buf = box->buffer;
    *max = box->max_length;
    *len = &box->length;
    *pos = &box->cursor_position;
}

bool clk_input_box_handle_input(clk_input_box* box, clk_key_event event) {
    switch (event.key_mask) {
        case KEY_LEFT:
            clk_input_move_cursor(-1);
            break;
        case KEY_RIGHT:
            clk_input_move_cursor(1);
            break;
        case KEY_BS:
            clk_input_delete_before();
            break;
        case KEY_DEL:
            clk_input_delete_after();
            break;
        case KEY_HOME:
        case KEY_UP:
            clk_input_move_cursor(-9999);
            break;
        case KEY_END:
        case KEY_DOWN:
            clk_input_move_cursor(9999);
            break;
        case KEY_ESC:
            box->finished = true;
            box->confirmed = false;
            return true;
        case KEY_ENTER:
            box->finished = true;
            box->confirmed = true;
            return true;
        default:
            if (event.has_text) {
                int cur_cols = clk_term_utf8_display_width(box->buffer, box->length);
                int add_cols = clk_term_utf8_display_width(event.text, event.text_len);
                if (cur_cols + add_cols <= CLK_INPUT_BOX_COL_MAX)
                    clk_input_write(CLK_WRITE_INSERT, event.text, event.text_len);
            }
            break;
    }
    return false;
}

bool clk_input_box_is_confirmed(const clk_input_box* box) {
    return box ? box->confirmed : false;
}

const char* clk_input_box_get_result(const clk_input_box* box) {
    return box ? box->buffer : NULL;
}

void clk_input_box_render(clk_input_box* box) {
    if (!box)
        return;

    clk_texture_clear_all(&box->tex);
    clk_texture_fill_rect(&box->tex, 0, 0, CLK_INPUT_BOX_WIDTH, CLK_INPUT_BOX_HEIGHT, " ",
                          box->border_style);
    draw_border(&box->tex, box->border_style);
    clk_texture_write_string(&box->tex, CLK_INPUT_BOX_PROMPT_X, CLK_INPUT_BOX_PROMPT_Y, "> ",
                             box->text_style);
    clk_texture_write_string(&box->tex, CLK_INPUT_BOX_TEXT_X, CLK_INPUT_BOX_TEXT_Y, box->buffer,
                             box->text_style);

    int col = clk_term_utf8_display_width(box->buffer, box->cursor_position);
    clk_term_cursor_set_pos(box->sprite->posx + CLK_INPUT_BOX_TEXT_X + col,
                            box->sprite->posy + CLK_INPUT_BOX_TEXT_Y);
    clk_term_cursor_show();
}

void clk_input_box_set_position(clk_input_box* box, int x, int y) {
    if (!box || !box->sprite)
        return;
    box->sprite->posx = x;
    box->sprite->posy = y;
}

void clk_input_box_set_z_order(clk_input_box* box, int z) {
    if (!box || !box->sprite)
        return;
    clk_sprite_set_z(box->sprite, z);
}

void clk_input_box_add_to_term(clk_input_box* box) {
    if (!box || box->sprite_added)
        return;
    clk_term_add_sprite(box->sprite);
    box->sprite_added = true;
}

void clk_input_box_remove_from_term(clk_input_box* box) {
    if (!box || !box->sprite_added)
        return;
    clk_term_remove_sprite(box->sprite);
    box->sprite_added = false;
}
