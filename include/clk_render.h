#ifndef CLK_RENDER_H
#define CLK_RENDER_H

#include <ncursesw/ncurses.h>
#include <stdbool.h>
#include <stdint.h>
#include <wchar.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CLK_STYLE_INVALID (-1)
#define CLK_STYLE_DEFAULT_QUANTITY (16)
#define CLK_TEXTURE_DEFAULT_CAPACITY (8)

// 定义样式结构
typedef struct {
    uint32_t fg_rgb;
    uint32_t bg_rgb;
    attr_t attrs;
} clk_style;

// 定义渲染单元格结构，包含字符、颜色和样式信息
typedef struct {
    wchar_t ch;
    int style_id;
} clk_cell;

// 定义渲染纹理结构，包含宽高、单元格数组和层级信息
typedef struct {
    int x, y;
    int width, height;
    clk_cell* cells;
    int z_index;
} clk_render_texture;

clk_render_texture* clk_render_texture_create(int x, int y, int width, int height, int z_index);
void clk_render_texture_destroy(clk_render_texture* texture);

bool clk_render_texture_set_cell(clk_render_texture* texture, int x, int y, clk_cell cell);
bool clk_render_texture_fill(clk_render_texture* texture, clk_cell cell);

// 定义渲染器
typedef struct clk_render clk_render;

clk_render* clk_render_create(void);
void clk_render_destroy(clk_render* render);

int clk_render_register_style(clk_render* render, const clk_style* style);

bool clk_render_add_texture(clk_render* render, clk_render_texture* texture);

void clk_render_present(clk_render* render);
// 清除的是当前帧的渲染 texture 列表，而非清屏
void clk_render_clear(clk_render* render);

#ifdef __cplusplus
}
#endif

#endif  // CLK_RENDER_H