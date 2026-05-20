#include <stdbool.h>
#include <stdio.h>
#include <windows.h>

#include "clk_clock.h"
#include "clk_key_io.h"
#include "clk_menu.h"
#include "clk_term.h"

int main(void) {
    if (!clk_term_init()) {
        printf("初始化错误");
        return 1;
    }
    clk_clock the_clock = {};
    clk_texture clock_texture;
    if (!clk_add_texture_to_render_list(&clock_texture)) {
        printf("clock_texture添加失败");
        return 1;
    }
    clk_menu settings_menu = {};
    clk_texture menu_texture;
    if (!clk_add_texture_to_render_list(&menu_texture)) {
        printf("menu_texture添加失败");
        return 1;
    }
    bool running = true;
    while (running) {
        clk_update_clock(&the_clock);
        clk_update_term();
        clock_texture = clk_clock_to_texture(&the_clock);
        clk_key_event event = clk_get_key_event();
        menu_texture = clk_menu_to_texture(&settings_menu);
        switch (event.key) {
            case 'q':
            case 'Q':
                // todo
                break;
            case 's':
            case 'S':
                // todo
                break;
            default:
                break;
        }
        clk_term_draw();
        Sleep(200);
    }
    clk_destroy_menu(&settings_menu);
    clk_term_close();
}
