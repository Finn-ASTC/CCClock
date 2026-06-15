/** Clock — terminal-based digital clock application. */

#include <stdbool.h>
#include <stdio.h>

#include "clk_clock.h"
#include "clk_key_io.h"
#include "clk_term.h"

int main(void) {
    if (!clk_term_init()) {
        printf("clk_term_init failed\n");
        return 1;
    }

    clk_clock clk;
    if (!clk_clock_create(&clk, "hh:MM:ss",
                          "assets/config/ascii_fonts/simple_ascii_num.json")) {
        printf("clk_clock_create failed\n");
        clk_term_close();
        return 1;
    }

    clk_clock_set_sprite_pos(&clk, 2, 2);
    clk_clock_add_to_term(&clk);

    printf("\033[10;1HPress q to exit...");
    fflush(stdout);

    bool running = true;
    while (running) {
        clk_key_event ev = clk_get_key_event();
        if (ev.key == 'q' || ev.key == 'Q')
            running = false;

        clk_term_update();
        clk_clock_update(&clk);
        clk_term_draw();
        clk_term_sleep_ms(200);
    }

    clk_clock_destroy(&clk);
    clk_term_close();
    return 0;
}
