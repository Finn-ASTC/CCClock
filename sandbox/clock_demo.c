#include <stddef.h>
#include <stdio.h>

#include "clk_clock.h"
#include "clk_key_io.h"
#include "clk_term.h"

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

static void recenter_clock(clk_clock* clk, int term_w, int term_h) {
    int cw = 0, ch = 0;
    if (!clk_clock_get_clock_size(clk, &cw, &ch))
        return;
    int px = (term_w - cw) / 2;
    int py = (term_h - ch) / 2;
    clk_clock_set_sprite_pos(clk, px, py);
}

int main() {
    clk_term_init();

    clk_texture red_tex = {0}, green_tex = {0};
    clk_sprite* red_sp = NULL;
    clk_sprite* green_sp = NULL;

    const char* formats[] = {"yyyy:mm:dd\n hh:MM:ss", "hh:MM:ss", "hh:MM"};
    int fmt_idx = 0;

    const char* font_files[] = {"assets/config/ascii_fonts/simple_ascii_num.json",
                                "assets/config/ascii_fonts/single_color_num.json"};
    int font_idx = 0;

    clk_clock clk;
    if (!clk_clock_create(&clk, formats[fmt_idx], font_files[font_idx])) {
        clk_term_close();
        return -1;
    }

    int term_w = 0, term_h = 0;
    if (!clk_term_get_size(&term_w, &term_h))
        goto fail;

    recenter_clock(&clk, term_w, term_h);

    if (!clk_clock_add_to_term(&clk))
        goto fail;

    /* two overlapping coloured blocks for z-order demo */
    int red_style = clk_term_register_style_rgb(255, 0, 0, 0, 0, 0, "bold");
    int green_style = clk_term_register_style_rgb(0, 255, 0, 0, 0, 0, "bold");

    int bw = 10, bh = 5;
    red_tex = clk_texture_create(bw, bh);
    green_tex = clk_texture_create(bw, bh);
    clk_texture_fill_rect(&red_tex, 0, 0, bw, bh, "█", red_style);
    clk_texture_fill_rect(&green_tex, 0, 0, bw, bh, "█", green_style);

    int red_px = term_w / 2 - bw - 2;
    int red_py = term_h / 2 - bh / 2;
    int green_px = term_w / 2 + 2;
    int green_py = term_h / 2 - bh / 2;

    red_sp = clk_sprite_create_with_texture(&red_tex, red_px, red_py, -1);
    green_sp = clk_sprite_create_with_texture(&green_tex, green_px, green_py, 1);
    clk_term_add_sprite(red_sp);
    clk_term_add_sprite(green_sp);

    printf("\033[2J\033[H");
    printf(
        "=== Clock Demo ===\n"
        "  f  : toggle time format\n"
        "  r  : switch font (colour / white)\n"
        "  UP / DOWN : clock z-order +/-1\n"
        "  q  : quit\n\n"
        "Press any key to start...\n");

    /* drain any stale input (e.g. leftover Enter from command line),
     * then wait for a fresh key press */
    while (clk_get_key_event().key != CLK_KEY_NONE)
        ;
    while (clk_get_key_event().key == CLK_KEY_NONE)
        clk_term_sleep_ms(16);

    printf("\033[2J\033[H");

    /* main loop */
    for (;;) {
        clk_key_event ev = clk_get_key_event();
        if (ev.key == 'q' || ev.key == 'Q')
            break;

        switch (ev.key) {
            case 'f':
            case 'F':
                fmt_idx = (fmt_idx + 1) % ARRAY_SIZE(formats);
                clk_clock_change_time_format(&clk, formats[fmt_idx]);
                recenter_clock(&clk, term_w, term_h);
                break;

            case 'r':
            case 'R':
                font_idx = (font_idx + 1) % ARRAY_SIZE(font_files);
                clk_clock_change_font_path(&clk, font_files[font_idx]);
                recenter_clock(&clk, term_w, term_h);
                break;

            case CLK_KEY_UP:
                clk_clock_set_z_order(&clk, clk_clock_get_z_order(&clk) + 1);
                break;

            case CLK_KEY_DOWN:
                clk_clock_set_z_order(&clk, clk_clock_get_z_order(&clk) - 1);
                break;
        }

        if (clk_term_size_changed()) {
            clk_term_resize();
            if (!clk_term_get_size(&term_w, &term_h))
                break;
            recenter_clock(&clk, term_w, term_h);
        }

        clk_clock_update(&clk);
        clk_term_update();
        clk_term_draw();
        clk_term_sleep_ms(16);
    }

    clk_clock_destroy(&clk);
    clk_sprite_destroy(red_sp);
    clk_sprite_destroy(green_sp);
    clk_texture_destroy(&red_tex);
    clk_texture_destroy(&green_tex);
    clk_term_close();
    return 0;

fail:
    clk_clock_destroy(&clk);
    clk_sprite_destroy(red_sp);
    clk_sprite_destroy(green_sp);
    clk_texture_destroy(&red_tex);
    clk_texture_destroy(&green_tex);
    clk_term_close();
    return -1;
}
