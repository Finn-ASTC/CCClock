#include "clk_term.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include "clk_key_io.h"

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#else
#include <sys/ioctl.h>
#include <unistd.h>
#endif

#define CLK_TEXTURE_DEFAULT_LENGTH (16)

static int clk_is_term_init = false;

static clk_cell* screen_buffer;

static int screen_w, screen_h;
static int screen_buffer_size;

static const clk_texture** texture_render_list;

static int texture_list_count = 0;
static int texture_list_capacity = CLK_TEXTURE_DEFAULT_LENGTH;

bool clk_term_init(void) {
    clk_key_io_init();

    int sw, sh;
    if (!clk_get_term_size(&sw, &sh))
        return false;
    if (sw <= 0 || sh <= 0)
        return false;
    screen_buffer_size = sw * sh;
    screen_w = sw;
    screen_h = sh;

    clk_cell* temp_s = malloc(screen_buffer_size * sizeof(clk_cell));
    if (!temp_s)
        return false;
    screen_buffer = temp_s;

    clk_cell empty_cell = {.is_empty = true};

    for (int i = 0; i < screen_buffer_size; ++i)
        screen_buffer[i] = empty_cell;

    const clk_texture** temp_l = malloc(texture_list_capacity * sizeof(const clk_texture*));
    if (!temp_l) {
        free(screen_buffer);
        return false;
    }

    texture_render_list = temp_l;

    for (int i = 0; i < texture_list_capacity; ++i)
        texture_render_list[i] = NULL;

    clk_is_term_init = true;

    printf("\033[?25l");

    return true;
}

void clk_term_close(void) {
    if (!clk_is_term_init)
        return;

    clk_key_io_close();

    free(screen_buffer);
    screen_buffer = NULL;

    free(texture_render_list);
    texture_render_list = NULL;
    // texture_list 里面存储的 texture 生命周期应该由其创建者管理

    texture_list_count = 0;
    texture_list_capacity = CLK_TEXTURE_DEFAULT_LENGTH;

    screen_w = 0;
    screen_h = 0;
    screen_buffer_size = 0;

    printf("\033[?25h");
}

static int cmp_texture_zorder(const void* tex1, const void* tex2) {
    const clk_texture* t1 = *(const clk_texture**)tex1;
    const clk_texture* t2 = *(const clk_texture**)tex2;

    return (t2->tex_z_order > t1->tex_z_order) - (t2->tex_z_order < t1->tex_z_order);
}

bool clk_add_texture_to_render_list(const clk_texture* texture) {
    if (!texture || !clk_is_term_init)
        return false;

    int count = texture_list_count + 1;

    if (count > texture_list_capacity) {
        int new_capacity = texture_list_capacity * 2;
        const clk_texture** temp_l =
            realloc(texture_render_list, new_capacity * sizeof(const clk_texture*));
        if (!temp_l)
            return false;

        texture_render_list = temp_l;

        for (int i = texture_list_capacity; i < new_capacity; ++i) {
            texture_render_list[i] = NULL;
        }

        texture_list_capacity = new_capacity;
    }

    texture_render_list[texture_list_count] = texture;

    texture_list_count = count;

    // 降序排列
    qsort(texture_render_list, texture_list_count, sizeof(const clk_texture*), cmp_texture_zorder);

    return true;
}

bool clk_add_all_textures_to_screen_buffer(void) {
    // todo
    return true;
}

void clk_term_draw(void) {
    if (!clk_is_term_init)
        return;

    if (!clk_add_all_textures_to_screen_buffer())
        return;

    // todo
}

void clk_update_term(void) {
    // todo
}

bool clk_get_term_size(int* term_w, int* term_h) {
    if (!term_w || !term_h)
        return false;

#if defined(_WIN32) || defined(_WIN64)

    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hConsole == INVALID_HANDLE_VALUE) {
        return false;
    }

    CONSOLE_SCREEN_BUFFER_INFO csbi;

    if (GetConsoleScreenBufferInfo(hConsole, &csbi)) {
        *term_w = csbi.srWindow.Right - csbi.srWindow.Left + 1;
        *term_h = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
        return true;
    }

    return false;

#else  // linux / MacOS / Unix

    struct winsize w;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0) {
        *term_w = w.ws_col;
        *term_h = w.ws_row;
        return true;
    }

    return false;

#endif
}
