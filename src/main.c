/** Clock — terminal-based digital clock application. */

#include <stdbool.h>
#include <stdio.h>
#include <windows.h>

#include "clk_clock.h"
#include "clk_key_io.h"
#include "clk_menu.h"
#include "clk_term.h"

int main(void) {
    if (!clk_term_init()) {
        printf("init error\n");
        return 1;
    }

    clk_clock the_clock        = {};
    clk_texture clock_texture  = {0};
    clk_texture menu_texture   = {0};
    clk_menu settings_menu     = {};

    clk_term_add_texture(&clock_texture);
    clk_term_add_texture(&menu_texture);

    bool running = true;
    while (running) {
        clk_update_clock(&the_clock);
        clk_term_update();

        clock_texture = clk_clock_to_texture(&the_clock);
        clk_key_event event = clk_get_key_event();
        menu_texture = clk_menu_to_texture(&settings_menu);

        switch (event.key) {
            case 'q':
            case 'Q':
                /* TODO — exit */
                break;
            case 's':
            case 'S':
                /* TODO — settings menu */
                break;
            default:
                break;
        }

        clk_term_draw();
        Sleep(200);
    }

    clk_destroy_menu(&settings_menu);
    clk_texture_destroy(&clock_texture);
    clk_texture_destroy(&menu_texture);
    clk_term_close();
    return 0;
}
