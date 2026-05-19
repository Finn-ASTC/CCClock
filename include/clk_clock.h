#ifndef CLK_CLOCK_H
#define CLK_CLOCK_H

#include <time.h>

#include "clk_term.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CLK_TIME_FORMAT_LENGTH (64)
#define CLK_FONT_PATH_MAX_LENGTH (256)

typedef struct {
    struct tm clk_time;
    char clk_time_format[CLK_TIME_FORMAT_LENGTH];
    char clk_font_path[CLK_FONT_PATH_MAX_LENGTH];
} clk_clock;

void clk_update_clock(clk_clock* clk);

clk_texture clk_clock_to_texture(const clk_clock* clk);

#ifdef __cplusplus
}
#endif

#endif  // CLK_CLOCK_H
