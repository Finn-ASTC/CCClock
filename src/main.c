#include <ncursesw/ncurses.h>

#include "clk_render.h"

int main(void) {
    clk_render* render = clk_render_create();
    if (!render)
        return 1;
    // 注册一个彩色样式：绿色前景
    clk_style style_green = {.fg_rgb = 0x00FF00, .bg_rgb = 0x000000, .attrs = A_NORMAL};
    int green_id = clk_render_register_style(render, &style_green);
    // 再注册一个：青色前景
    clk_style style_cyan = {.fg_rgb = 0x00FFFF, .bg_rgb = 0x000000, .attrs = A_NORMAL};
    int cyan_id = clk_render_register_style(render, &style_cyan);
    // 11x5 纹理
    clk_render_texture* tex = clk_render_texture_create(5, 5, 11, 5, 0);
    if (!tex) {
        clk_render_destroy(render);
        return 1;
    }
    clk_cell off = {L' ', 0};
    // 每行 11 列的 HI 图案，不同行用不同颜色
    const char* lines[] = {
        "#   # #####", "#   #   #  ", "#####   #  ", "#   #   #  ", "#   # #####",
    };
    int colors[] = {green_id, green_id, cyan_id, cyan_id, green_id};
    for (int y = 0; y < 5; y++) {
        for (int x = 0; x < 11; x++) {
            if (lines[y][x] == '#') {
                clk_cell cell = {L'#', colors[y]};
                clk_render_texture_set_cell(tex, x, y, cell);
            } else {
                clk_render_texture_set_cell(tex, x, y, off);
            }
        }
    }
    clk_render_add_texture(render, tex);
    clk_render_present(render);
    napms(30000);
    clk_render_texture_destroy(tex);
    clk_render_destroy(render);
    return 0;
}