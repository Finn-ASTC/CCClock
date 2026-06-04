#ifndef CLK_CLOCK_H
#define CLK_CLOCK_H

#include <stdbool.h>
#include <stddef.h>

#include "clk_term.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CLK_CLOCK_TIME_FORMAT_MAX_LENGTH (64)
#define CLK_CLOCK_NUM_TEXTURE_COUNT (11)  // 0-9 + colon

typedef struct {
    char clk_clock_time_format[CLK_CLOCK_TIME_FORMAT_MAX_LENGTH];
    char* clk_clock_font_path;
    clk_texture clk_clock_num_font_texture[CLK_CLOCK_NUM_TEXTURE_COUNT];
    clk_sprite** clk_clock_sprites;
    size_t clk_clock_sprite_count;
    size_t clk_clock_sprite_capacity;
    size_t clk_clock_glyph_spacing;
} clk_clock;

bool clk_clock_create(clk_clock* clk, const char* time_format, const char* font_path);

void clk_clock_destroy(clk_clock* clk);

bool clk_clock_change_time_format(clk_clock* clk, const char* new_time_format);

bool clk_clock_change_font_path(clk_clock* clk, const char* new_font_path);

void clk_clock_update(clk_clock* clk);

bool clk_clock_get_sprite_size(const clk_clock* clk, int* width, int* height);

bool clk_clock_get_font_texture_size(const clk_clock* clk, int* width, int* height);

bool clk_clock_get_sprite_pos(const clk_clock* clk, int* posx, int* posy);

bool clk_clock_set_sprite_pos(clk_clock* clk, int posx, int posy);

#ifdef __cplusplus
}
#endif

#endif  // CLK_CLOCK_H
