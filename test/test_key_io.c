#include <stdio.h>
#include <string.h>

#include "clk_key_io.h"
#include "test_utils.h"

extern void clk_key_io_test_inject(uint32_t key);

int main(void) {
    if (isatty_fd(fileno(stdout))) {
        g_pass = "  \033[32m[PASS]\033[0m ";
        g_fail = "  \033[31m[FAIL]\033[0m ";
    }

    /* ================================================================
     *  Constants
     * ================================================================ */
    TEST("CLK_KEY_NONE == 0xFFFFFFFF", CLK_KEY_NONE == 0xFFFFFFFF);
    TEST("CLK_KEY_ESC == 27", CLK_KEY_ESC == 27);
    TEST("CLK_KEY_UP != CLK_KEY_DOWN", CLK_KEY_UP != CLK_KEY_DOWN);
    TEST("CLK_KEY_LEFT != CLK_KEY_RIGHT", CLK_KEY_LEFT != CLK_KEY_RIGHT);

    /* ================================================================
     *  Lifecycle (init / close / reinit)
     * ================================================================ */
    clk_key_io_init();
    TEST("clk_key_io_init() no crash", 1);
    clk_key_io_close();
    TEST("clk_key_io_close() no crash", 1);

    /* double init / close */
    clk_key_io_init();
    clk_key_io_init();
    TEST("double init no crash", 1);
    clk_key_io_close();
    clk_key_io_close();
    TEST("double close no crash", 1);

    /* no key pressed before init */
    clk_key_event ev = clk_get_key_event();
    TEST("get_event before init returns NONE", ev.key == CLK_KEY_NONE);

    /* normal flow */
    clk_key_io_init();
    ev = clk_get_key_event();
    TEST("get_event after init returns NONE", ev.key == CLK_KEY_NONE);
    clk_key_io_close();

    /* reinit */
    clk_key_io_init();
    ev = clk_get_key_event();
    TEST("get_event after reinit returns NONE", ev.key == CLK_KEY_NONE);
    clk_key_io_close();

    /* ================================================================
     *  String input — pure logic
     * ================================================================ */

    char buf[64];
    size_t len, pos;
    uint32_t r;

    /* --- begin_input NULL guards --- */
    clk_key_io_text_start(NULL, 64, &len, &pos);
    TEST("begin_input NULL buf safe", 1);
    clk_key_io_text_start(buf, 0, &len, &pos);
    TEST("begin_input zero size safe", 1);
    clk_key_io_text_start(buf, 64, NULL, &pos);
    TEST("begin_input NULL len safe", 1);
    clk_key_io_text_start(buf, 64, &len, NULL);
    TEST("begin_input NULL pos safe", 1);

    /* --- append keys --- */
    clk_key_io_init();
    clk_key_io_text_start(buf, 64, &len, &pos);
    clk_key_io_test_inject('A');
    r = clk_key_io_text_poll();
    TEST("append 'A' → NONE", r == CLK_KEY_NONE);
    TEST("append 'A' → len=1,pos=1", len == 1 && pos == 1);
    TEST("append 'A' → buf[0]='A'", buf[0] == 'A');

    clk_key_io_test_inject('B');
    r = clk_key_io_text_poll();
    TEST("append 'B' → len=2,pos=2", len == 2 && pos == 2);
    TEST("append 'B' → buf[1]='B'", buf[1] == 'B');

    clk_key_io_test_inject('C');
    clk_key_io_text_poll();
    TEST("append 'C' → buf=ABC", len == 3 && strncmp(buf, "ABC", 3) == 0);
    clk_key_io_text_stop();

    /* --- left + insert --- */
    clk_key_io_text_start(buf, 64, &len, &pos);
    clk_key_io_test_inject('A');
    clk_key_io_text_poll();
    clk_key_io_test_inject('B');
    clk_key_io_text_poll();
    clk_key_io_test_inject(CLK_KEY_LEFT);
    clk_key_io_text_poll();
    clk_key_io_test_inject('X');
    r = clk_key_io_text_poll();
    TEST("left+insert → NONE", r == CLK_KEY_NONE);
    TEST("left+insert → buf=AXB", strncmp(buf, "AXB", 3) == 0 && len == 3);
    clk_key_io_text_stop();

    /* --- left at 0 → no-op --- */
    clk_key_io_text_start(buf, 64, &len, &pos);
    clk_key_io_test_inject('H');
    clk_key_io_text_poll();
    clk_key_io_test_inject(CLK_KEY_LEFT);
    clk_key_io_text_poll();
    TEST("left at 0 → pos=0", pos == 0);
    clk_key_io_test_inject(CLK_KEY_LEFT);
    clk_key_io_text_poll();
    TEST("left at 0 double → pos=0", pos == 0);
    clk_key_io_text_stop();

    /* --- right at end → no-op --- */
    clk_key_io_text_start(buf, 64, &len, &pos);
    clk_key_io_test_inject('H');
    clk_key_io_text_poll();
    TEST("right at end → pos=1", pos == 1);
    clk_key_io_test_inject(CLK_KEY_RIGHT);
    clk_key_io_text_poll();
    TEST("right at end → pos still 1", pos == 1);
    clk_key_io_text_stop();

    /* --- backspace --- */
    clk_key_io_text_start(buf, 64, &len, &pos);
    clk_key_io_test_inject('A');
    clk_key_io_text_poll();
    clk_key_io_test_inject('B');
    clk_key_io_text_poll();
    clk_key_io_test_inject(CLK_KEY_LEFT);
    clk_key_io_text_poll();
    clk_key_io_test_inject('\b');
    r = clk_key_io_text_poll();
    TEST("backspace: after BS → NONE", r == CLK_KEY_NONE);
    TEST("backspace: len=1", len == 1);
    TEST("backspace: buf='B'", buf[0] == 'B');
    TEST("backspace: pos=0", pos == 0);
    clk_key_io_text_stop();

    /* --- backspace at 0 → no-op --- */
    clk_key_io_text_start(buf, 64, &len, &pos);
    clk_key_io_test_inject('\b');
    r = clk_key_io_text_poll();
    TEST("backspace at 0 → NONE", r == CLK_KEY_NONE);
    TEST("backspace at 0 → len=0", len == 0);
    clk_key_io_text_stop();

    /* --- Enter confirm --- */
    clk_key_io_text_start(buf, 64, &len, &pos);
    clk_key_io_test_inject('A');
    clk_key_io_text_poll();
    clk_key_io_test_inject('B');
    clk_key_io_text_poll();
    clk_key_io_test_inject('\r');
    r = clk_key_io_text_poll();
    TEST("Enter → returns \\r", r == '\r');
    TEST("Enter → buf='AB'", strcmp(buf, "AB") == 0);
    TEST("Enter → auto-exits input mode", clk_key_io_text_poll() == CLK_KEY_NONE);

    /* --- ESC cancel --- */
    clk_key_io_text_start(buf, 64, &len, &pos);
    clk_key_io_test_inject('A');
    clk_key_io_text_poll();
    clk_key_io_test_inject(CLK_KEY_ESC);
    r = clk_key_io_text_poll();
    TEST("ESC → returns CLK_KEY_ESC", r == CLK_KEY_ESC);
    TEST("ESC → len=0", len == 0);
    TEST("ESC → buf[0]='\\0'", buf[0] == '\0');

    /* --- full buffer --- */
    clk_key_io_text_start(buf, 4, &len, &pos);
    clk_key_io_test_inject('1');
    clk_key_io_text_poll();
    clk_key_io_test_inject('2');
    clk_key_io_text_poll();
    clk_key_io_test_inject('3');
    clk_key_io_text_poll();
    clk_key_io_test_inject('4');
    r = clk_key_io_text_poll();
    TEST("full buf: 4th char ignored → NONE", r == CLK_KEY_NONE);
    TEST("full buf: len=3", len == 3);
    clk_key_io_text_stop();

    /* --- non-printable ignored --- */
    clk_key_io_text_start(buf, 64, &len, &pos);
    clk_key_io_test_inject(CLK_KEY_UP);
    r = clk_key_io_text_poll();
    TEST("UP arrow ignored → NONE", r == CLK_KEY_NONE);
    TEST("UP arrow → len unchanged", len == 0);
    clk_key_io_text_stop();

    /* --- mode isolation --- */
    clk_key_io_text_start(buf, 64, &len, &pos);
    ev = clk_get_key_event();
    TEST("get_key_event → NONE during input", ev.key == CLK_KEY_NONE);
    clk_key_io_text_stop();

    /* --- end + restart fresh state --- */
    clk_key_io_text_start(buf, 64, &len, &pos);
    clk_key_io_test_inject('X');
    clk_key_io_text_poll();
    clk_key_io_text_stop();
    clk_key_io_text_start(buf, 64, &len, &pos);
    TEST("restart: len=0", len == 0);
    TEST("restart: buf empty", buf[0] == '\0');
    clk_key_io_text_stop();

    clk_key_io_close();

    printf("\n%d/%d passed\n", test_total - test_failed, test_total);
    return test_failed > 0 ? 1 : 0;

test_cleanup:
    printf("\n%d/%d passed\n", test_total - test_failed, test_total);
    return 1;
}
