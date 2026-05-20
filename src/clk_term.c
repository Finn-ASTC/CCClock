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

static clk_cell* screen;

static int screen_w, screen_h;
static int screen_size;

static clk_texture** texture_list;

static int texture_list_count = 0;
static int texture_list_capacity = CLK_TEXTURE_DEFAULT_LENGTH;

bool clk_term_init(void) {
    clk_key_io_init();

    int sw, sh;
    if (!clk_get_term_size(&sw, &sh))
        return false;
    if (sw <= 0 || sh <= 0)
        return false;
    screen_size = sw * sh;
    screen_w = sw;
    screen_h = sh;

    clk_cell* temp_s = malloc(screen_size * sizeof(clk_cell));
    if (!temp_s)
        return false;
    screen = temp_s;

    clk_cell empty_cell = {.is_empty = true};

    for (int i = 0; i < screen_size; ++i)
        screen[i] = empty_cell;

    clk_texture** temp_l = malloc(texture_list_capacity * sizeof(clk_texture*));
    if (!temp_l) {
        free(screen);
        return false;
    }

    texture_list = temp_l;

    for (int i = 0; i < CLK_TEXTURE_DEFAULT_LENGTH; ++i)
        texture_list[i] = NULL;

    printf("\033[?25l");

    return true;
}

void clk_term_close(void) {
    clk_key_io_close();
    free(screen);
    free(texture_list);
    texture_list_count = 0;
    texture_list_capacity = CLK_TEXTURE_DEFAULT_LENGTH;
    printf("\033[?25h");
}

void clk_add_texture_to_term(const clk_texture* texture) {
    // todo
}

void clk_term_draw(void) {
    // todo
}

void clk_update_term(void) {
    // todo
}

bool clk_get_term_size(int* term_w, int* term_h) {
    if (term_w == NULL || term_h == NULL)
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
