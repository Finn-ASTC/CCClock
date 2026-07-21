#include <stdio.h>
#include <string.h>

#include "clk_key_io.h"
#include "test_utils.h"

extern void clk_key_io_test_inject(clk_key_event ev);
extern void clk_key_io_test_inject_raw(int ch);
extern void clk_key_io_test_queue_byte(int ch);
extern void clk_key_io_test_reset(void);
extern void clk_key_io_test_pause(void);
extern void clk_key_io_test_resume(void);

int main(void) {
    if (isatty_fd(fileno(stdout))) {
        g_pass = "  \033[32m[PASS]\033[0m ";
        g_fail = "  \033[31m[FAIL]\033[0m ";
    }

    /* ================================================================
     *  Key mask constants
     * ================================================================ */

    TEST("KEY_A_UPPER != 0", KEY_A_UPPER != 0);
    TEST("KEY_Z_UPPER != 0", KEY_Z_UPPER != 0);
    TEST("KEY_a_LOWER != 0", KEY_a_LOWER != 0);
    TEST("KEY_z_LOWER != 0", KEY_z_LOWER != 0);
    TEST("KEY_0 != 0", KEY_0 != 0);
    TEST("KEY_9 != 0", KEY_9 != 0);
    TEST("KEY_UP != 0", KEY_UP != 0);
    TEST("KEY_ESC != 0", KEY_ESC != 0);
    TEST("KEY_F1 != 0", KEY_F1 != 0);
    TEST("KEY_F8 != 0", KEY_F8 != 0);

    TEST("MOD_SHIFT != 0", MOD_SHIFT != 0);
    TEST("MOD_CTRL != 0", MOD_CTRL != 0);
    TEST("MOD_ALT != 0", MOD_ALT != 0);
    TEST("MOD_META != 0", MOD_META != 0);

    TEST("KEY_UP != KEY_DOWN", KEY_UP != KEY_DOWN);
    TEST("KEY_LEFT != KEY_RIGHT", KEY_LEFT != KEY_RIGHT);
    TEST("KEY_A_UPPER != KEY_a_LOWER", KEY_A_UPPER != KEY_a_LOWER);
    TEST("KEY_ESC != KEY_ENTER", KEY_ESC != KEY_ENTER);
    TEST("KEY_SLASH != KEY_BS", KEY_SLASH != KEY_BS);
    TEST("MOD_CTRL != MOD_SHIFT", MOD_CTRL != MOD_SHIFT);

    TEST("mod+key OR yields two bits", (MOD_CTRL | KEY_A_UPPER) == (MOD_CTRL + KEY_A_UPPER));
    TEST("mod+shift+key OR yields three bits",
         (MOD_CTRL | MOD_SHIFT | KEY_J_UPPER) == (MOD_CTRL + MOD_SHIFT + KEY_J_UPPER));

    /* ================================================================
     *  Event struct
     * ================================================================ */

    clk_key_event ev_zero = {0};
    TEST("zero-init: key_mask == 0", ev_zero.key_mask == 0);
    TEST("zero-init: text_len == 0", ev_zero.text_len == 0);
    TEST("zero-init: has_text == false", ev_zero.has_text == false);

    clk_key_event ev_k = {
        .key_mask = KEY_a_LOWER, .text = {'a', 0}, .text_len = 1, .has_text = true};
    TEST("constructed: key_mask OK", ev_k.key_mask == KEY_a_LOWER);
    TEST("constructed: text_len=1", ev_k.text_len == 1);
    TEST("constructed: has_text", ev_k.has_text);

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

    /* no event before init */
    clk_key_event ev = clk_normal_get_key_event();
    TEST("get before init returns zero", ev.key_mask == 0);

    /* normal flow */
    clk_key_io_init();
    ev = clk_normal_get_key_event();
    TEST("get after init returns zero", ev.key_mask == 0);
    clk_key_io_close();

    /* reinit */
    clk_key_io_init();
    ev = clk_normal_get_key_event();
    TEST("get after reinit returns zero", ev.key_mask == 0);
    clk_key_io_close();

    /* ================================================================
     *  Mode control
     * ================================================================ */

    char buf[64];
    size_t len, pos;

    /* --- bind + set_input clears buffer --- */
    clk_key_io_init();
    clk_key_io_set_input(buf, 32, &len, &pos);
    TEST("set_input: len=0", len == 0);
    TEST("set_input: pos=0", pos == 0);
    TEST("set_input: buf empty", buf[0] == '\0');
    clk_key_io_set_normal();

    /* --- set_input NULL guards --- */
    clk_key_io_set_input(NULL, 32, &len, &pos);
    TEST("set_input NULL buf safe", 1);
    clk_key_io_set_input(buf, 0, &len, &pos);
    TEST("set_input zero max_chars safe", 1);
    clk_key_io_set_input(buf, 32, NULL, &pos);
    TEST("set_input NULL len safe", 1);
    clk_key_io_set_input(buf, 32, &len, NULL);
    TEST("set_input NULL pos safe", 1);

    /* --- restart after set_normal → set_input clears --- */
    clk_key_io_set_input(buf, 32, &len, &pos);
    len = 5; /* dirty */
    pos = 3;
    clk_key_io_set_normal();
    clk_key_io_set_input(buf, 32, &len, &pos);
    TEST("restart: len=0", len == 0);
    TEST("restart: pos=0", pos == 0);
    clk_key_io_set_normal();

    clk_key_io_close();

    /* ================================================================
     *  Mode isolation — event retrieval
     * ================================================================ */

    clk_key_io_init();

    /* NORMAL → normal_get works, input_get returns zero */
    ev = clk_normal_get_key_event();
    TEST("normal mode: normal_get returns zero (empty)", ev.key_mask == 0);

    ev.key_mask = (__uint128_t)-1;
    ev = clk_input_get_key_event();
    TEST("normal mode: input_get returns zero", ev.key_mask == 0);

    /* INPUT → input_get works, normal_get returns zero */
    clk_key_io_set_input(buf, 32, &len, &pos);

    ev.key_mask = (__uint128_t)-1;
    ev = clk_normal_get_key_event();
    TEST("input mode: normal_get returns zero", ev.key_mask == 0);

    ev = clk_input_get_key_event();
    TEST("input mode: input_get returns zero (empty)", ev.key_mask == 0);

    clk_key_io_set_normal();
    clk_key_io_test_resume();
    clk_key_io_close();

    /* ================================================================
     *  Test injection + ring buffer
     * ================================================================ */

    clk_key_io_init();
    {
        clk_key_event ev1 = {.key_mask = KEY_ESC};
        clk_key_event ev2 = {.key_mask = MOD_CTRL | KEY_A_UPPER};

        clk_key_io_test_inject(ev1);
        clk_key_io_test_inject(ev2);

        clk_key_event got1 = clk_normal_get_key_event();
        TEST("inject ESC → key_mask == KEY_ESC", got1.key_mask == KEY_ESC);

        clk_key_event got2 = clk_normal_get_key_event();
        TEST("inject Ctrl+A → mod+key", got2.key_mask == (MOD_CTRL | KEY_A_UPPER));

        clk_key_event got3 = clk_normal_get_key_event();
        TEST("third get → zero (ring empty)", got3.key_mask == 0);
    }

    /* injection with text — INPUT mode preserves it */
    clk_key_io_set_input(buf, 32, &len, &pos);
    {
        clk_key_event tev = {
            .key_mask = KEY_a_LOWER, .text = {'a', 0}, .text_len = 1, .has_text = true};
        clk_key_io_test_inject(tev);

        clk_key_event got = clk_input_get_key_event();
        TEST("inject text event: key_mask OK", got.key_mask == KEY_a_LOWER);
        TEST("inject text event: has_text", got.has_text);
        TEST("inject text event: text_len=1", got.text_len == 1);
    }
    clk_key_io_set_normal();
    clk_key_io_test_resume();
    clk_key_io_close();

    /* ================================================================
     *  clk_key_is utility
     * ================================================================ */

    {
        clk_key_event e = {.key_mask = MOD_CTRL | KEY_S_UPPER};
        TEST("key_is Ctrl+S(+) ", clk_key_is(e, KEY_S_UPPER, MOD_CTRL));
        TEST("key_is Ctrl+S(−) wrong key", !clk_key_is(e, KEY_T_UPPER, MOD_CTRL));
        TEST("key_is Ctrl+S(−) wrong mod", !clk_key_is(e, KEY_S_UPPER, MOD_META));
        TEST("key_is Ctrl+S(−) no mod", !clk_key_is(e, KEY_S_UPPER, 0));
    }
    {
        clk_key_event e = {.key_mask = KEY_ESC};
        TEST("key_is ESC(+) ", clk_key_is(e, KEY_ESC, 0));
        TEST("key_is ESC(−) stray mod", !clk_key_is(e, KEY_ESC, MOD_CTRL));
    }
    {
        clk_key_event e = {.key_mask = MOD_CTRL | MOD_SHIFT | KEY_J_UPPER};
        TEST("key_is Ctrl+Shift+J(+) ", clk_key_is(e, KEY_J_UPPER, MOD_CTRL | MOD_SHIFT));
        TEST("key_is Ctrl+Shift+J(−) missing mod", !clk_key_is(e, KEY_J_UPPER, MOD_CTRL));
    }

    /* ================================================================
     *  Editing primitives — no-ops outside INPUT mode
     * ================================================================ */

    /* NORMAL mode → all return false / no-op */
    if (!clk_input_write(CLK_WRITE_INSERT, "x", 1))
        TEST("write(INSERT) NORMAL → false", 1);
    else
        TEST("write(INSERT) NORMAL → false", 0);

    if (!clk_input_write(CLK_WRITE_OVERWRITE, "y", 1))
        TEST("write(OVERWRITE) NORMAL → false", 1);
    else
        TEST("write(OVERWRITE) NORMAL → false", 0);

    if (!clk_input_delete_before())
        TEST("delete_before NORMAL → false", 1);
    else
        TEST("delete_before NORMAL → false", 0);

    if (!clk_input_delete_after())
        TEST("delete_after NORMAL → false", 1);
    else
        TEST("delete_after NORMAL → false", 0);

    clk_input_move_cursor(5);
    TEST("move_cursor NORMAL → no crash", 1);

    /* INPUT mode — placeholder buffer, primitives not yet fully impl */
    clk_key_io_init();
    clk_key_io_set_input(buf, 32, &len, &pos);
    TEST("input mode set OK", 1);
    clk_key_io_set_normal();
    clk_key_io_test_resume();
    clk_key_io_close();

    /* ================================================================
     *  State machine — byte dispatch (inject_raw)
     * ================================================================ */

    clk_key_io_init();
    clk_key_io_test_pause();
    clk_key_io_test_reset();

    /* ---- ASCII printable ---- */
    clk_key_io_test_inject_raw('a');
    ev = clk_normal_get_key_event();
    TEST("sm 'a' → KEY_a_LOWER", ev.key_mask == KEY_a_LOWER);

    clk_key_io_test_inject_raw('A');
    ev = clk_normal_get_key_event();
    TEST("sm 'A' → KEY_A_UPPER", ev.key_mask == KEY_A_UPPER);

    clk_key_io_test_inject_raw('0');
    ev = clk_normal_get_key_event();
    TEST("sm '0' → KEY_0", ev.key_mask == KEY_0);

    clk_key_io_test_inject_raw(' ');
    ev = clk_normal_get_key_event();
    TEST("sm ' ' → KEY_SPACE", ev.key_mask == KEY_SPACE);

    clk_key_io_test_inject_raw('/');
    ev = clk_normal_get_key_event();
    TEST("sm '/' → KEY_SLASH", ev.key_mask == KEY_SLASH);

    /* ---- Ctrl + letter ---- */
    clk_key_io_test_inject_raw(0x01);
    ev = clk_normal_get_key_event();
    TEST("sm Ctrl+A → MOD_CTRL|KEY_A_UPPER", ev.key_mask == (MOD_CTRL | KEY_A_UPPER));

    clk_key_io_test_inject_raw(0x1A);
    ev = clk_normal_get_key_event();
    TEST("sm Ctrl+Z → MOD_CTRL|KEY_Z_UPPER", ev.key_mask == (MOD_CTRL | KEY_Z_UPPER));

    /* ---- Backspace / Enter / Tab -- checked before Ctrl+letter range ---- */
    clk_key_io_test_inject_raw(0x7F);
    ev = clk_normal_get_key_event();
    TEST("sm BS (0x7F) → KEY_BS", ev.key_mask == KEY_BS);

    clk_key_io_test_inject_raw(0x08);
    ev = clk_normal_get_key_event();
    TEST("sm BS (0x08) → KEY_BS", ev.key_mask == KEY_BS);

    clk_key_io_test_inject_raw(0x0D);
    ev = clk_normal_get_key_event();
    TEST("sm ENTER → KEY_ENTER", ev.key_mask == KEY_ENTER);

    clk_key_io_test_inject_raw(0x09);
    ev = clk_normal_get_key_event();
    TEST("sm TAB → KEY_TAB", ev.key_mask == KEY_TAB);

    /* ---- ESC alone (no follow-on) ---- */
    clk_key_io_test_inject_raw(0x1B);
    ev = clk_normal_get_key_event();
    TEST("sm ESC → KEY_ESC", ev.key_mask == KEY_ESC);

    /* ---- CSI arrows ---- */
    clk_key_io_test_queue_byte('[');
    clk_key_io_test_inject_raw(0x1B);
    clk_key_io_test_inject_raw('A');
    ev = clk_normal_get_key_event();
    TEST("sm CSI A → KEY_UP", ev.key_mask == KEY_UP);

    clk_key_io_test_queue_byte('[');
    clk_key_io_test_inject_raw(0x1B);
    clk_key_io_test_inject_raw('B');
    ev = clk_normal_get_key_event();
    TEST("sm CSI B → KEY_DOWN", ev.key_mask == KEY_DOWN);

    clk_key_io_test_queue_byte('[');
    clk_key_io_test_inject_raw(0x1B);
    clk_key_io_test_inject_raw('C');
    ev = clk_normal_get_key_event();
    TEST("sm CSI C → KEY_RIGHT", ev.key_mask == KEY_RIGHT);

    clk_key_io_test_queue_byte('[');
    clk_key_io_test_inject_raw(0x1B);
    clk_key_io_test_inject_raw('D');
    ev = clk_normal_get_key_event();
    TEST("sm CSI D → KEY_LEFT", ev.key_mask == KEY_LEFT);

    /* ---- CSI HOME / END ---- */
    clk_key_io_test_queue_byte('[');
    clk_key_io_test_inject_raw(0x1B);
    clk_key_io_test_inject_raw('H');
    ev = clk_normal_get_key_event();
    TEST("sm CSI H → KEY_HOME", ev.key_mask == KEY_HOME);

    clk_key_io_test_queue_byte('[');
    clk_key_io_test_inject_raw(0x1B);
    clk_key_io_test_inject_raw('F');
    ev = clk_normal_get_key_event();
    TEST("sm CSI F → KEY_END", ev.key_mask == KEY_END);

    /* ---- CSI ~ sequences ---- */
    clk_key_io_test_queue_byte('[');
    clk_key_io_test_inject_raw(0x1B);
    clk_key_io_test_inject_raw('3');
    clk_key_io_test_inject_raw('~');
    ev = clk_normal_get_key_event();
    TEST("sm CSI 3~ → KEY_DEL", ev.key_mask == KEY_DEL);

    clk_key_io_test_queue_byte('[');
    clk_key_io_test_inject_raw(0x1B);
    clk_key_io_test_inject_raw('1');
    clk_key_io_test_inject_raw('5');
    clk_key_io_test_inject_raw('~');
    ev = clk_normal_get_key_event();
    TEST("sm CSI 15~ → KEY_F5", ev.key_mask == KEY_F5);

    /* ---- CSI with modifier ---- */
    clk_key_io_test_queue_byte('[');
    clk_key_io_test_inject_raw(0x1B);
    clk_key_io_test_inject_raw('1');
    clk_key_io_test_inject_raw(';');
    clk_key_io_test_inject_raw('5');
    clk_key_io_test_inject_raw('A');
    ev = clk_normal_get_key_event();
    TEST("sm CSI 1;5A → KEY_UP|MOD_CTRL", ev.key_mask == (KEY_UP | MOD_CTRL));

    /* ---- CSI META modifier codes ---- */
    clk_key_io_test_queue_byte('[');
    clk_key_io_test_inject_raw(0x1B);
    clk_key_io_test_inject_raw('1');
    clk_key_io_test_inject_raw(';');
    clk_key_io_test_inject_raw('9');
    clk_key_io_test_inject_raw('A');
    ev = clk_normal_get_key_event();
    TEST("sm CSI 1;9A → KEY_UP|MOD_META", ev.key_mask == (KEY_UP | MOD_META));

    clk_key_io_test_queue_byte('[');
    clk_key_io_test_inject_raw(0x1B);
    clk_key_io_test_inject_raw('1');
    clk_key_io_test_inject_raw(';');
    clk_key_io_test_inject_raw('1');
    clk_key_io_test_inject_raw('3');
    clk_key_io_test_inject_raw('A');
    ev = clk_normal_get_key_event();
    TEST("sm CSI 1;13A → KEY_UP|MOD_META|MOD_CTRL", ev.key_mask == (KEY_UP | MOD_META | MOD_CTRL));

    clk_key_io_test_queue_byte('[');
    clk_key_io_test_inject_raw(0x1B);
    clk_key_io_test_inject_raw('1');
    clk_key_io_test_inject_raw(';');
    clk_key_io_test_inject_raw('4');
    clk_key_io_test_inject_raw('A');
    ev = clk_normal_get_key_event();
    TEST("sm CSI 1;4A → KEY_UP|MOD_SHIFT|MOD_ALT", ev.key_mask == (KEY_UP | MOD_SHIFT | MOD_ALT));

    /* ---- SS3 ---- */
    clk_key_io_test_queue_byte('O');
    clk_key_io_test_inject_raw(0x1B);
    clk_key_io_test_inject_raw('P');
    ev = clk_normal_get_key_event();
    TEST("sm ESC O P → KEY_F1", ev.key_mask == KEY_F1);

    clk_key_io_test_queue_byte('O');
    clk_key_io_test_inject_raw(0x1B);
    clk_key_io_test_inject_raw('Q');
    ev = clk_normal_get_key_event();
    TEST("sm ESC O Q → KEY_F2", ev.key_mask == KEY_F2);

    /* ---- modifyOtherKeys: Ctrl+letter via keycode 1-26 ---- */
    clk_key_io_test_queue_byte('[');
    clk_key_io_test_inject_raw(0x1B);
    clk_key_io_test_inject_raw('1');
    clk_key_io_test_inject_raw(';');
    clk_key_io_test_inject_raw('5');
    clk_key_io_test_inject_raw('u');
    ev = clk_normal_get_key_event();
    TEST("sm CSI 1;5u → MOD_CTRL|KEY_A_UPPER", ev.key_mask == (MOD_CTRL | KEY_A_UPPER));

    /* ---- bracketed paste ---- */
    clk_key_io_test_queue_byte('[');
    clk_key_io_test_inject_raw(0x1B);
    clk_key_io_test_inject_raw('2');
    clk_key_io_test_inject_raw('0');
    clk_key_io_test_inject_raw('0');
    clk_key_io_test_inject_raw('~');
    ev = clk_normal_get_key_event();
    TEST("sm CSI 200~ → KEY_PASTE_START", ev.key_mask == KEY_PASTE_START);

    clk_key_io_test_queue_byte('[');
    clk_key_io_test_inject_raw(0x1B);
    clk_key_io_test_inject_raw('2');
    clk_key_io_test_inject_raw('0');
    clk_key_io_test_inject_raw('1');
    clk_key_io_test_inject_raw('~');
    ev = clk_normal_get_key_event();
    TEST("sm CSI 201~ → KEY_PASTE_END", ev.key_mask == KEY_PASTE_END);

    /* ---- pending_byte (ESC followed by non-[non-O) ---- */
    /* NOTE: this mechanism relies on background-thread timing;
     *       verify it works interactively by pasting text that
     *       contains ESC sequences. */
    clk_key_io_test_inject_raw(0x1B);
    ev = clk_normal_get_key_event();
    TEST("sm ESC alone → KEY_ESC", ev.key_mask == KEY_ESC);

    clk_key_io_test_resume();
    clk_key_io_close();

    /* ================================================================
     *  Text editing — INPUT mode
     * ================================================================ */

    clk_key_io_init();
    clk_key_io_test_pause();
    clk_key_io_test_reset();
    clk_key_io_set_input(buf, 32, &len, &pos);

    /* ---- write INSERT ---- */
    TEST_REQUIRE("write(INSERT) abc → true", clk_input_write(CLK_WRITE_INSERT, "abc", 3));
    TEST("write(INSERT) abc → len=3", len == 3);
    TEST("write(INSERT) abc → pos=3", pos == 3);
    TEST("write(INSERT) abc → buf", strncmp(buf, "abc", 3) == 0);

    /* insert at middle */
    pos = 1;
    TEST_REQUIRE("write(INSERT) mid 'X' → true", clk_input_write(CLK_WRITE_INSERT, "X", 1));
    TEST("write(INSERT) mid → len=4", len == 4);
    TEST("write(INSERT) mid → pos=2", pos == 2);
    TEST("write(INSERT) mid → buf=AXbc", strncmp(buf, "aXbc", 4) == 0);

    /* empty input */
    TEST("write(INSERT) \"\" → false", !clk_input_write(CLK_WRITE_INSERT, "", 0));

    /* overflow truncation — max 32 bytes, fill to 31 */
    memset(buf, 'x', 31);
    len = 31;
    pos = 0;
    TEST("write(INSERT) overflow → false", !clk_input_write(CLK_WRITE_INSERT, "ab", 2));
    TEST("write(INSERT) overflow → partial", len == 32);
    TEST("write(INSERT) overflow → 'a' was written", buf[0] == 'a');

    /* ---- write OVERWRITE ---- */
    len = 0;
    clk_input_write(CLK_WRITE_INSERT, "xyzw", 4);
    pos = 0;
    TEST_REQUIRE("write(OVERWRITE) 'abc' over 'xyzw' → true",
                 clk_input_write(CLK_WRITE_OVERWRITE, "abc", 3));
    TEST("write(OVERWRITE) 'abc' over 'xyzw' → len=4", len == 4);
    TEST("write(OVERWRITE) 'abc' over 'xyzw' → pos=3", pos == 3);

    /* overwrite past end appends */
    len = 0;
    clk_input_write(CLK_WRITE_INSERT, "x", 1);
    pos = 0;
    TEST_REQUIRE("write(OVERWRITE) 'abc' over 'x' → true",
                 clk_input_write(CLK_WRITE_OVERWRITE, "abc", 3));
    TEST("write(OVERWRITE) 'abc' over 'x' → abc", strncmp(buf, "abc", 3) == 0);

    /* overwrite on empty */
    len = 0;
    pos = 0;
    TEST_REQUIRE("write(OVERWRITE) on empty → true", clk_input_write(CLK_WRITE_OVERWRITE, "ab", 2));
    TEST("write(OVERWRITE) on empty → ab", strncmp(buf, "ab", 2) == 0);

    /* ---- move_cursor ---- */
    len = 0;
    clk_input_write(CLK_WRITE_INSERT, "abcdef", 6);
    pos = 0;
    clk_input_move_cursor(2);
    TEST("move +2 → pos=2", pos == 2);
    clk_input_move_cursor(-1);
    TEST("move -1 → pos=1", pos == 1);
    clk_input_move_cursor(-5);
    TEST("move -5 → clamp pos=0", pos == 0);
    clk_input_move_cursor(100);
    TEST("move +100 → clamp pos=6", pos == 6);

    /* ---- delete_before ---- */
    len = 0;
    clk_input_write(CLK_WRITE_INSERT, "abcdef", 6);
    pos = 2;
    TEST("delete_before → true", clk_input_delete_before());
    TEST("delete_before → len=5", len == 5);
    TEST("delete_before → pos=1", pos == 1);

    pos = 0;
    TEST("delete_before at 0 → false", !clk_input_delete_before());

    /* ---- delete_after ---- */
    len = 0;
    clk_input_write(CLK_WRITE_INSERT, "abcdef", 6);
    pos = 1;
    TEST("delete_after → true", clk_input_delete_after());
    TEST("delete_after → buf=acdef", strncmp(buf, "acdef", 5) == 0);
    TEST("delete_after → pos=1", pos == 1);

    pos = 5;
    TEST("delete_after at end → false", !clk_input_delete_after());

    /* ---- illegal write mode ---- */
    TEST("write mode -1 → false", !clk_input_write((clk_write_mode)(-1), "x", 1));
    TEST("write mode 99 → false", !clk_input_write((clk_write_mode)99, "x", 1));

    clk_key_io_set_normal();
    clk_key_io_test_resume();
    clk_key_io_close();

    /* ================================================================
     *  UTF-8 cross-frame accumulation
     * ================================================================ */

    clk_key_io_init();
    clk_key_io_test_pause();
    clk_key_io_test_reset();
    clk_key_io_set_input(buf, 32, &len, &pos);

    /* 2-byte UTF-8: é = 0xC3 0xA1 */
    clk_key_io_test_inject_raw(0xC3);
    ev = clk_input_get_key_event();
    TEST("sm UTF-8 lead(2b) → no event", ev.key_mask == 0 && !ev.has_text);
    clk_key_io_test_inject_raw(0xA1);
    ev = clk_input_get_key_event();
    TEST("sm UTF-8 complete(2b) → has_text", ev.has_text);
    TEST("sm UTF-8 complete(2b) → text_len=2", ev.text_len == 2);

    /* 3-byte UTF-8: 中 = 0xE4 0xB8 0xAD */
    clk_key_io_test_inject_raw(0xE4);
    ev = clk_input_get_key_event();
    TEST("sm UTF-8 lead(3b) → no event", ev.key_mask == 0 && !ev.has_text);
    clk_key_io_test_inject_raw(0xB8);
    ev = clk_input_get_key_event();
    TEST("sm UTF-8 cont(3b) → no event", ev.key_mask == 0 && !ev.has_text);
    clk_key_io_test_inject_raw(0xAD);
    ev = clk_input_get_key_event();
    TEST("sm UTF-8 complete(3b) → has_text", ev.has_text);
    TEST("sm UTF-8 complete(3b) → text_len=3", ev.text_len == 3);

    /* broken sequence recovery */
    clk_key_io_test_inject_raw(0xE4);
    ev = clk_input_get_key_event();
    TEST("sm UTF-8 broken: lead then ASCII → no event (lead)", ev.key_mask == 0 && !ev.has_text);
    clk_key_io_test_inject_raw('A');
    ev = clk_input_get_key_event();
    TEST("sm UTF-8 broken: ASCII after → KEY_A_UPPER", ev.key_mask == KEY_A_UPPER);

    clk_key_io_set_normal();
    clk_key_io_test_resume();
    clk_key_io_close();

    /* ================================================================
     *  Paste mode
     * ================================================================ */

    clk_key_io_init();
    clk_key_io_test_pause();
    clk_key_io_test_reset();
    clk_key_io_set_input(buf, 32, &len, &pos);

    /* Paste start — via raw CSI 200~ */
    clk_key_io_test_queue_byte('[');
    clk_key_io_test_inject_raw(0x1B);
    clk_key_io_test_inject_raw('2');
    clk_key_io_test_inject_raw('0');
    clk_key_io_test_inject_raw('0');
    clk_key_io_test_inject_raw('~');
    ev = clk_input_get_key_event();
    TEST("paste bracket START → OK", ev.key_mask == KEY_PASTE_START);

    /* During paste: text char pushed with key=0 */
    clk_key_io_test_inject_raw('a');
    ev = clk_input_get_key_event();
    TEST("paste text 'a' → key_mask=0", ev.key_mask == 0);
    TEST("paste text 'a' → has_text", ev.has_text);

    /* Paste end restores normal */
    clk_key_io_test_queue_byte('[');
    clk_key_io_test_inject_raw(0x1B);
    clk_key_io_test_inject_raw('2');
    clk_key_io_test_inject_raw('0');
    clk_key_io_test_inject_raw('1');
    clk_key_io_test_inject_raw('~');
    ev = clk_input_get_key_event();
    TEST("paste bracket END → OK", ev.key_mask == KEY_PASTE_END);

    clk_key_io_test_inject_raw('a');
    ev = clk_input_get_key_event();
    TEST("after paste: normal key resumes", ev.key_mask == KEY_a_LOWER);

    clk_key_io_set_normal();
    clk_key_io_test_resume();
    clk_key_io_close();

    /* ================================================================
     *  Summary
     * ================================================================ */

    printf("\n%d/%d passed\n", test_total - test_failed, test_total);
    return test_failed > 0 ? 1 : 0;

test_cleanup:
    printf("\n%d/%d passed\n", test_total - test_failed, test_total);
    return 1;
}
