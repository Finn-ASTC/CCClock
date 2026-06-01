#include "clk_clock.h"

#include <stdlib.h>

bool clk_clock_create(clk_clock* clk, const char* time_format, const char* font_path) {
    (void)clk;
    (void)time_format;
    (void)font_path;
    return false;
}

void clk_clock_destroy(clk_clock* clk) {
    (void)clk;
}

bool clk_clock_change_time_format(clk_clock* clk, const char* new_time_format) {
    (void)clk;
    (void)new_time_format;
    return false;
}

bool clk_clock_change_font_path(clk_clock* clk, const char* new_font_path) {
    (void)clk;
    (void)new_font_path;
    return false;
}

void clk_clock_update(clk_clock* clk) {
    (void)clk;
}

bool clk_clock_get_texture_size(const clk_clock* clk, int* width, int* height) {
    (void)clk;
    (void)width;
    (void)height;
    return false;
}

bool clk_clock_get_texture_pos(const clk_clock* clk, int* posx, int* posy) {
    (void)clk;
    (void)posx;
    (void)posy;
    return false;
}

bool clk_clock_set_texture_pos(clk_clock* clk, int posx, int posy) {
    (void)clk;
    (void)posx;
    (void)posy;
    return false;
}

const clk_texture* clk_clock_get_texture(const clk_clock* clk) {
    (void)clk;
    return NULL;
}
