#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "clk_key_io.h"
#include "clk_term.h"
#ifdef _WIN32
#include <windows.h>
#define SLEEP_MS(ms) Sleep(ms)
#else
#include <unistd.h>
#define SLEEP_MS(ms) usleep((ms) * 1000)
#endif
int main(void) {
    if (!clk_term_init()) {
        printf("clk_term_init 失败\n");
        return 1;
    }
    const char* text = "Hello, ANSI!";
    int tex_w = (int)strlen(text);
    int tex_h = 1;
    clk_cell* cells = malloc(tex_w * tex_h * sizeof(clk_cell));
    if (!cells) {
        clk_term_close();
        return 1;
    }
    Color24 colors[] = {
        {{.r = 255, .g = 0, .b = 0, .a = 0}},    // 红
        {{.r = 255, .g = 128, .b = 0, .a = 0}},  // 橙
        {{.r = 255, .g = 255, .b = 0, .a = 0}},  // 黄
        {{.r = 0, .g = 255, .b = 0, .a = 0}},    // 绿
        {{.r = 0, .g = 128, .b = 255, .a = 0}},  // 蓝
        {{.r = 128, .g = 0, .b = 255, .a = 0}},  // 紫
        {{.r = 255, .g = 0, .b = 128, .a = 0}},  // 粉
    };
    int num_colors = sizeof(colors) / sizeof(colors[0]);
    for (int x = 0; x < tex_w; x++) {
        clk_cell c = {0};
        c.cell_tex[0] = text[x];
        c.cell_tex[1] = '\0';
        c.fg_color = colors[x % num_colors];
        c.attrs = ATTR_BOLD;
        c.is_empty = false;
        cells[x] = c;
    }
    clk_texture tex = {
        .posx = 2,
        .posy = 1,
        .tex_w = tex_w,
        .tex_h = tex_h,
        .tex_z_order = 0,
        .data = cells,
        .is_invalid = false,
    };
    clk_add_texture_to_render_list(&tex);
    clk_term_draw();
    printf("\033[%d;1H按 q 键退出...", tex_h + 3);
    fflush(stdout);
    free(cells);
    bool not_exit = true;
    clk_key_event event;
    while (not_exit) {
        event = clk_get_key_event();
        if (event.key == 'q') {
            not_exit = false;
        }
    }
    clk_term_close();
    return 0;
}
