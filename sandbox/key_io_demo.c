#include <stdbool.h>
#include <stdio.h>

#include "clk_key_io.h"
#include "clk_term.h"
#include "clk_time.h"

int main(void) {
    clk_term_init();
    clk_key_io_init();

    char buf[64];
    size_t len = 0, pos = 0;
    enum { MODE_NAVIGATE, MODE_TEXT } mode = MODE_NAVIGATE;
    bool running = true;

    while (running) {
        if (mode == MODE_NAVIGATE) {
            clk_key_event event = clk_normal_get_key_event();
            if (event.key_mask == KEY_q_LOWER)
                running = false;
            else if (event.key_mask == KEY_s_LOWER) {
                clk_key_io_set_input(buf, sizeof(buf) - 1, &len, &pos);
                mode = MODE_TEXT;
            } else if (event.key_mask == KEY_UP)
                printf("UP\n");
            else if (event.key_mask == KEY_DOWN)
                printf("DOWN\n");
            else if (event.key_mask == KEY_LEFT)
                printf("LEFT\n");
            else if (event.key_mask == KEY_RIGHT)
                printf("RIGHT\n");
        } else {
            clk_key_event event = clk_input_get_key_event();
            if (event.key_mask == KEY_ENTER) {
                printf("text: \"%s\"\n", buf);
                clk_key_io_set_normal();
                mode = MODE_NAVIGATE;
            } else if (event.key_mask == KEY_ESC) {
                printf("cancelled\n");
                clk_key_io_set_normal();
                mode = MODE_NAVIGATE;
            } else if (event.has_text) {
                clk_input_write(CLK_WRITE_INSERT, event.text, event.text_len);
            }
        }

        clk_time_sleep_ms(16);
    }

    clk_key_io_close();
    clk_term_close();
    return 0;
}
