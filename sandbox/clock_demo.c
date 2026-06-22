#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "clk_ascii_render.h"
#include "clk_clock.h"
#include "clk_key_io.h"
#include "clk_term.h"

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

static void recenter_clock(clk_ascii_render* render, const char* time_format, int term_w,
                            int term_h) {
    int cw = 0, ch = 0;
    if (!clk_ascii_render_get_size(render, time_format, &cw, &ch))
        return;
    clk_ascii_render_set_pos(render, (term_w - cw) / 2, (term_h - ch) / 2);
}

int main() {
    clk_term_init();

    clk_texture red_tex = {0}, green_tex = {0};
    clk_sprite* red_sp = NULL;
    clk_sprite* green_sp = NULL;

    const char* formats[] = {"yyyy:mm:dd\n hh:MM:ss", "hh:MM:ss", "hh:MM"};
    int fmt_idx = 0;

    const char* font_files[] = {"assets/config/ascii_fonts/simple_ascii_num.json",
                                "assets/config/ascii_fonts/single_color_num.json",
                                "assets/config/ascii_fonts/ascii_num.json"};
    int font_idx = 0;

    char time_format[64];
    size_t len = strlen(formats[fmt_idx]);
    if (len >= sizeof(time_format))
        len = sizeof(time_format) - 1;
    memcpy(time_format, formats[fmt_idx], len);
    time_format[len] = '\0';

    clk_ascii_render render;
    if (!clk_ascii_render_create(&render, font_files[font_idx])) {
        clk_term_close();
        return -1;
    }

    int term_w = 0, term_h = 0;
    if (!clk_term_get_size(&term_w, &term_h))
        goto fail;

    recenter_clock(&render, time_format, term_w, term_h);

    if (!clk_ascii_render_add_to_term(&render))
        goto fail;

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

    while (clk_get_key_event().key != CLK_KEY_NONE)
        ;
    while (clk_get_key_event().key == CLK_KEY_NONE)
        clk_term_sleep_ms(16);

    printf("\033[2J\033[H");

    for (;;) {
        clk_key_event event = clk_get_key_event();
        if (event.key == 'q' || event.key == 'Q')
            break;

        switch (event.key) {
            case 'f':
            case 'F':
                fmt_idx = (fmt_idx + 1) % ARRAY_SIZE(formats);
                {
                    size_t l = strlen(formats[fmt_idx]);
                    if (l >= sizeof(time_format))
                        l = sizeof(time_format) - 1;
                    memcpy(time_format, formats[fmt_idx], l);
                    time_format[l] = '\0';
                }
                recenter_clock(&render, time_format, term_w, term_h);
                break;

            case 'r':
            case 'R':
                font_idx = (font_idx + 1) % ARRAY_SIZE(font_files);
                clk_ascii_render_change_font(&render, font_files[font_idx]);
                recenter_clock(&render, time_format, term_w, term_h);
                break;

            case CLK_KEY_UP:
                clk_ascii_render_set_z_order(&render, clk_ascii_render_get_z_order(&render) + 1);
                break;

            case CLK_KEY_DOWN:
                clk_ascii_render_set_z_order(&render, clk_ascii_render_get_z_order(&render) - 1);
                break;
        }

        if (clk_term_size_changed()) {
            clk_term_resize();
            if (!clk_term_get_size(&term_w, &term_h))
                break;
            recenter_clock(&render, time_format, term_w, term_h);
        }

        {
            char translated[128];
            char time_str[64];
            if (clk_clock_translate_format(time_format, translated, sizeof(translated)) &&
                clk_clock_format_now(translated, time_str, sizeof(time_str)))
                clk_ascii_render_update(&render, time_str);
        }

        clk_term_update();
        clk_term_draw();
        clk_term_sleep_ms(16);
    }

    clk_ascii_render_destroy(&render);
    clk_sprite_destroy(red_sp);
    clk_sprite_destroy(green_sp);
    clk_texture_destroy(&red_tex);
    clk_texture_destroy(&green_tex);
    clk_term_close();
    return 0;

fail:
    clk_ascii_render_destroy(&render);
    clk_sprite_destroy(red_sp);
    clk_sprite_destroy(green_sp);
    clk_texture_destroy(&red_tex);
    clk_texture_destroy(&green_tex);
    clk_term_close();
    return -1;
}
