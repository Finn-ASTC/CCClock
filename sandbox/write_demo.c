#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#include "clk_key_io.h"
#include "clk_term.h"
#include "clk_time.h"

#define INPUT_FIELD_HEIGHT 5
#define INPUT_FIELD_WIDTH 46
/** Display columns before hitting right border.  "> " occupies x=2..3,
 *  text starts at x=4, right border pad at x=WIDTH-4.
 *  → usable = (WIDTH-4) - 4 + 1 = WIDTH - 7 = 39. */
#define MAX_COLUMNS (INPUT_FIELD_WIDTH - 7)

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

    clk_key_io_close(); /* restart keyboard engine on fresh thread */
    clk_key_io_init();

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

        switch (mod) {
            case NORMAL: {
                clk_key_event ev = clk_normal_get_key_event();
                switch (ev.key_mask) {
                    case KEY_q_LOWER:
                    case KEY_Q_UPPER:
                        running = false;
                        break;
                    case KEY_w_LOWER:
                    case KEY_W_UPPER:
                        clk_key_io_set_input(buf, sizeof(buf) - 1, &len, &pos);
                        is_typing = true;
                        mod = INPUT;
                        break;
                    default:
                        break;
                }
                break;
            }
            case INPUT: {
                clk_key_event ev2 = clk_input_get_key_event();
                switch (ev2.key_mask) {
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
                    case KEY_ENTER:
                        clk_key_io_set_normal();
                        is_typing = false;
                        mod = NORMAL;
                        break;
                    default:
                        if (ev2.has_text) {
                            int cur_cols = clk_term_utf8_display_width(buf, len);
                            int add_cols = clk_term_utf8_display_width(ev2.text, ev2.text_len);
                            if (cur_cols + add_cols <= MAX_COLUMNS)
                                clk_input_write(CLK_WRITE_INSERT, ev2.text, ev2.text_len);
                        }
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
            {
                int col = clk_term_utf8_display_width(buf, pos);
                clk_term_cursor_set_pos(input_field->posx + 4 + col, input_field->posy + 2);
            }
            clk_term_cursor_show();
        } else {
            clk_term_cursor_hide();
            clk_texture_write_string(&input_field_tex, 4, 2, "Press 'w' to type, 'q' to quit",
                                     text_style);
        }

        set_field_pos(input_field);

        /* ---- Flush ---- */
        clk_term_update();
        clk_term_draw();
        clk_time_sleep_ms(16);
    }

    /* ================================================================
     *  Cleanup
     * ================================================================ */

    clk_key_io_close();
    clk_sprite_destroy(input_field);
    clk_texture_destroy(&input_field_tex);
    clk_term_close();
}
