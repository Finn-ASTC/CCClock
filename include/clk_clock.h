#ifndef CLK_CLOCK_H
#define CLK_CLOCK_H

#include <stdbool.h>
#include <time.h>

#include "clk_term.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CLK_CLOCK_TIME_FORMAT_LENGTH (64)

typedef struct {
    struct tm clk_clock_time;
    char clk_clock_time_format[CLK_CLOCK_TIME_FORMAT_LENGTH];
    char* clk_clock_font_path;
    clk_texture* clk_clock_texture;
} clk_clock;

bool clk_clock_create(clk_clock* clk, const char* time_format, const char* font_path);

void clk_clock_destroy(clk_clock* clk);

bool clk_clock_change_time_format(clk_clock* clk, const char* new_time_format);

bool clk_clock_change_font_path(clk_clock* clk, const char* new_font_path);

void clk_clock_update(clk_clock* clk);

bool clk_clock_get_texture_size(const clk_clock* clk, int* width, int* height);

bool clk_clock_get_texture_pos(const clk_clock* clk, int* posx, int* posy);

bool clk_clock_set_texture_pos(clk_clock* clk, int posx, int posy);

const clk_texture* clk_clock_get_texture(const clk_clock* clk);

#ifdef __cplusplus
}
#endif

#endif  // CLK_CLOCK_H
