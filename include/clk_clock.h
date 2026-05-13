#ifndef CLK_CLOCK_H
#define CLK_CLOCK_H

#include "clk_render.h"
#include <stdbool.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

// 定义时钟结构，包含时间信息、格式化字符串和字体路径
typedef struct {
    struct tm time_info;
    char* format;
    char* font_path;
} clk_clock;

clk_clock* clk_clock_create(const char* format, const char* font_path);
void clk_clock_destroy(clk_clock* clk);

bool clk_clock_update(clk_clock* clk);
bool clk_clock_set_format(clk_clock* clk, const char* format);
bool clk_clock_set_font(clk_clock* clk, const char* font_path);

clk_render_texture clk_clock_to_texture(const clk_clock* clk, clk_render_texture* texture);

#ifdef __cplusplus
}
#endif

#endif  // CLK_CLOCK_H