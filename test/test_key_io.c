#include "clk_key_io.h"
#include "test_utils.h"

int main(void) {
    if (isatty_fd(fileno(stdout))) {
        g_pass = "  \033[32m[PASS]\033[0m ";
        g_fail = "  \033[31m[FAIL]\033[0m ";
    }

    /* Constant values */
    TEST("CLK_KEY_NONE == 0xFFFFFFFF", CLK_KEY_NONE == 0xFFFFFFFF);
    TEST("CLK_KEY_ESC == 27", CLK_KEY_ESC == 27);
    TEST("CLK_KEY_UP != CLK_KEY_DOWN", CLK_KEY_UP != CLK_KEY_DOWN);
    TEST("CLK_KEY_LEFT != CLK_KEY_RIGHT", CLK_KEY_LEFT != CLK_KEY_RIGHT);

    /* init / close can be called repeatedly without crashing */
    clk_key_io_init();
    TEST("clk_key_io_init() no crash", 1);

    clk_key_io_close();
    TEST("clk_key_io_close() no crash", 1);

    /* double init / double close */
    clk_key_io_init();
    clk_key_io_init();
    TEST("double init no crash", 1);

    clk_key_io_close();
    clk_key_io_close();
    TEST("double close no crash", 1);

    /* get_key_event should not crash when not initialized */
    clk_key_event ev = clk_get_key_event();
    TEST("get_event before init returns NONE", ev.key == CLK_KEY_NONE);

    /* Normal flow */
    clk_key_io_init();
    ev = clk_get_key_event();
    TEST("get_event after init returns NONE when no key pressed", ev.key == CLK_KEY_NONE);
    clk_key_io_close();

    /* ring is cleared after close */
    clk_key_io_init();
    ev = clk_get_key_event();
    TEST("get_event after reinit returns NONE", ev.key == CLK_KEY_NONE);
    clk_key_io_close();

    /* init → close → init → close repeated cycle */
    for (int i = 0; i < 3; ++i) {
        clk_key_io_init();
        ev = clk_get_key_event();
        clk_key_io_close();
    }
    TEST("init/close cycle ×3 no crash", 1);

test_cleanup:
    printf("\n%d/%d passed\n", test_total - test_failed, test_total);
    return test_failed > 0 ? 1 : 0;
}
