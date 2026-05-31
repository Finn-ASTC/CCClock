#include "clk_key_io.h"
#include "test_utils.h"

int main(void) {
    if (isatty_fd(fileno(stdout))) {
        g_pass = "  \033[32m[PASS]\033[0m ";
        g_fail = "  \033[31m[FAIL]\033[0m ";
    }

    /* 常量值 */
    TEST("CLK_KEY_NONE == 0xFFFFFFFF", CLK_KEY_NONE == 0xFFFFFFFF);
    TEST("CLK_KEY_ESC == 27", CLK_KEY_ESC == 27);
    TEST("CLK_KEY_UP != CLK_KEY_DOWN", CLK_KEY_UP != CLK_KEY_DOWN);
    TEST("CLK_KEY_LEFT != CLK_KEY_RIGHT", CLK_KEY_LEFT != CLK_KEY_RIGHT);

    /* init / close 可以反复调用不崩溃 */
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

    /* 未 init 时 get_key_event 不应崩溃 */
    clk_key_event ev = clk_get_key_event();
    TEST("get_event before init returns NONE", ev.key == CLK_KEY_NONE);

    /* 正常流程 */
    clk_key_io_init();
    ev = clk_get_key_event();
    TEST("get_event after init returns NONE when no key pressed", ev.key == CLK_KEY_NONE);
    clk_key_io_close();

test_cleanup:
    printf("\n%d/%d passed\n", test_total - test_failed, test_total);
    return test_failed > 0 ? 1 : 0;
}
