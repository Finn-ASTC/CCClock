#include <stdbool.h>
#include <stdio.h>

#include "clk_key_io.h"
#include "clk_term.h"

int main(void) {
    clk_term_init();

    char buf[64];
    size_t len = 0, pos = 0;
    enum { MODE_NAVIGATE, MODE_TEXT } mode = MODE_NAVIGATE;
    bool running = true;

    while (running) {
        clk_key_event ev = clk_get_key_event();

        if (mode == MODE_NAVIGATE) {
            if (ev.key == 'q')
                running = false;
            else if (ev.key == 's') {
                clk_key_io_text_start(buf, sizeof(buf), &len, &pos);
                mode = MODE_TEXT;
            } else if (ev.key == CLK_KEY_UP)
                printf("UP\n");
            else if (ev.key == CLK_KEY_DOWN)
                printf("DOWN\n");
            else if (ev.key == CLK_KEY_LEFT)
                printf("LEFT\n");
            else if (ev.key == CLK_KEY_RIGHT)
                printf("RIGHT\n");
        } else {
            uint32_t result = clk_key_io_text_poll();
            if (result == '\r') {
                printf("text: \"%s\"\n", buf);
                mode = MODE_NAVIGATE;
            } else if (result == CLK_KEY_ESC) {
                printf("cancelled\n");
                mode = MODE_NAVIGATE;
            }
        }

        clk_term_sleep_ms(16);
    }

    clk_term_close();
    return 0;
}
