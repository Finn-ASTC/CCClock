/** Sandbox — interactive test harness for term + key-io modules.
 *  Compile: cmake --build build --target sandbox
 *  Run:     ./build/sandbox.exe
 */

#include <stdbool.h>
#include <stdio.h>

#include "clk_key_io.h"
#include "clk_term.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

int main(void) {
    if (!clk_term_init()) {
        printf("clk_term_init failed\n");
        return 1;
    }

    int red   = clk_term_register_style((Color24){.rgb = {255, 50, 50}},
                                    (Color24){0}, ATTR_BOLD);
    int green = clk_term_register_style((Color24){.rgb = {50, 255, 50}},
                                    (Color24){0}, ATTR_NONE);
    int blue  = clk_term_register_style((Color24){.rgb = {100, 150, 255}},
                                    (Color24){0}, ATTR_BOLD);
    int dim   = clk_term_register_style((Color24){.rgb = {128, 128, 128}},
                                    (Color24){0}, ATTR_DIM);
    int cyan_bg = clk_term_register_style((Color24){.rgb = {255, 255, 255}},
                                     (Color24){.rgb = {0, 100, 100}}, ATTR_NONE);
    int cyan_fg = clk_term_register_style((Color24){.rgb = {0, 255, 255}},
                                     (Color24){0}, ATTR_BOLD);

    /* rainbow "Hello ANSI!" */
    clk_texture tex_hello = clk_texture_create(11, 1);
    clk_texture_write_string(&tex_hello, 0, 0, "Hello ANSI!", red);
    tex_hello.posx = 2; tex_hello.posy = 1; tex_hello.tex_z_order = 0;

    /* dotted grid with coloured markers */
    clk_texture tex_info = clk_texture_create(10, 3);
    clk_texture_fill_rect(&tex_info, 0, 0, 10, 3, ".", dim);
    clk_texture_set_cell(&tex_info, 1, 1, "G", green);
    clk_texture_set_cell(&tex_info, 4, 1, "B", blue);
    clk_texture_set_cell(&tex_info, 7, 1, "R", red);
    tex_info.posx = 2; tex_info.posy = 3; tex_info.tex_z_order = 1;

    /* block with cyan background */
    clk_texture tex_block = clk_texture_create(5, 2);
    clk_texture_fill_rect(&tex_block, 0, 0, 5, 2, " ", cyan_bg);
    clk_texture_write_string(&tex_block, 0, 0, "BLOCK", cyan_fg);
    tex_block.posx = 2; tex_block.posy = 7; tex_block.tex_z_order = 0;

    clk_term_add_texture(&tex_hello);
    clk_term_add_texture(&tex_info);
    clk_term_add_texture(&tex_block);

    clk_term_draw();

    printf("\033[%d;1HPress q to exit...", 10);
    fflush(stdout);

    clk_key_event ev;
    bool running = true;
    while (running) {
        ev = clk_get_key_event();
        if (ev.key == 'q')
            running = false;
    }

    clk_texture_destroy(&tex_hello);
    clk_texture_destroy(&tex_info);
    clk_texture_destroy(&tex_block);
    clk_term_close();
    return 0;
}
