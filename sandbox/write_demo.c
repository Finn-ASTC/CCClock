#include <stdbool.h>
#include <stddef.h>

#include "clk_key_io.h"
#include "clk_term.h"

#define INPUT_FIELD_HEIGHT 5
#define INPUT_FIELD_WIDTH 46
/** Characters before hitting right border: "> " at x=2..3, text at
 *  x=4, right margin at WIDTH-4 → usable = (WIDTH-4) - 4 + 1. */
#define MAX_INPUT_LEN (INPUT_FIELD_WIDTH - 7)

/* ------------------------------------------------------------------
 *  Layout helpers
 * ------------------------------------------------------------------ */

/** Draw a box-drawing border around the entire texture perimeter. */
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

/** Re-centre the input field sprite on the terminal window. */
static void set_field_pos(clk_sprite* input_field) {
    int term_w = 0, term_h = 0;

    if (!clk_term_get_size(&term_w, &term_h))
        return;

    input_field->posx = (term_w - input_field->tex->tex_w) / 2;
    input_field->posy = (term_h - input_field->tex->tex_h) / 2;
}

int main() {
    /* ================================================================
     *  Init
     * ================================================================ */

    if (!clk_term_init())
        return -1;

    clk_texture input_field_tex = clk_texture_create(INPUT_FIELD_WIDTH, INPUT_FIELD_HEIGHT);
    clk_sprite* input_field = clk_sprite_create_with_texture(&input_field_tex, 0, 0, 0);

    set_field_pos(input_field);

    int text_style = clk_term_register_style_rgb(255, 255, 255, 0, 0, 0, ATTR_NONE);
    draw_border(&input_field_tex, text_style);

    clk_term_cursor_set_shape(CLK_CURSOR_BAR_BLINK);

    clk_term_add_sprite(input_field);

    char buf[64];
    size_t len = 0, pos = 0;
    enum { NORMAL, INPUT } mod = NORMAL;

    bool is_typing = false;

    bool running = true;

    /* ================================================================
     *  Main loop
     * ================================================================ */

    while (running) {
        /* ---- Input ---- */
        clk_key_event event;

        switch (mod) {
            case NORMAL:
                event = clk_get_key_event();
                switch (event.key) {
                    case 'q':
                        running = false;
                        break;
                    case 'w':
                        clk_key_io_text_start(buf, MAX_INPUT_LEN, &len, &pos);
                        is_typing = true;
                        mod = INPUT;
                        break;
                    default:
                        break;
                }
                break;
            case INPUT: {
                uint32_t result = clk_key_io_text_poll();
                switch (result) {
                    case CLK_KEY_ESC:
                    case '\r':
                        is_typing = false;
                        mod = NORMAL;
                        break;
                    default:
                        break;
                }
                break;
            }
        }

        /* ---- Render ---- */
        clk_texture_clear_all(&input_field_tex);

        draw_border(&input_field_tex, text_style);

        if (is_typing) {
            clk_texture_write_string(&input_field_tex, 2, 2, "> ", text_style);
            clk_texture_write_string(&input_field_tex, 4, 2, buf, text_style);
            clk_term_cursor_set_pos(input_field->posx + 4 + (int)pos, input_field->posy + 2);
            clk_term_cursor_show();
        } else {
            clk_term_cursor_hide();
            clk_texture_write_string(&input_field_tex, 4, 2, "Press 'w' to type, 'q' to quit",
                                     text_style);
        }

        /* ---- Flush ---- */
        set_field_pos(input_field);
        clk_term_update();
        clk_term_draw();
        clk_term_sleep_ms(16);
    }

    /* ================================================================
     *  Cleanup
     * ================================================================ */

    clk_sprite_destroy(input_field);
    clk_texture_destroy(&input_field_tex);
    clk_term_close();
}
