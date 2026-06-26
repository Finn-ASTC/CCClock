#include <stdio.h>

#include "clk_menu.h"
#include "test_utils.h"

static const char* str_opts[] = {"opt_a", "opt_b", "opt_c"};

int main(void) {
    if (isatty_fd(fileno(stdout))) {
        g_pass = "  \033[32m[PASS]\033[0m ";
        g_fail = "  \033[31m[FAIL]\033[0m ";
    }

    clk_menu* m;

    /* ================================================================
     *  create
     * ================================================================ */
    m = clk_menu_create();
    TEST_REQUIRE("create succeeds", m != NULL);

    TEST("create tab_capacity == 6", m->tab_capacity == 6);
    TEST("create tab_count == 0", m->tab_count == 0);
    TEST("create tabs != NULL", m->tabs != NULL);
    TEST("create active_tab == 0", m->active_tab == 0);

    /* ================================================================
     *  add_tab
     * ================================================================ */
    TEST("add_tab NULL menu", clk_menu_add_tab(NULL, 0, "x") == -1);
    TEST("add_tab NULL name", clk_menu_add_tab(m, 0, NULL) == -1);

    clk_menu_add_tab(m, 10, "display");
    TEST("tab_count == 1", m->tab_count == 1);
    TEST("tab[0].id == 10", m->tabs[0]->id == 10);
    TEST("tab[0].name ok", strcmp(m->tabs[0]->name, "display") == 0);
    TEST("tab[0].item_cap == 6", m->tabs[0]->item_capacity == 6);
    TEST("tab[0].item_cnt == 0", m->tabs[0]->item_count == 0);
    TEST("tab[0].active == 0", m->tabs[0]->active_item == 0);

    /* expansion: add enough to trigger realloc (default cap=6, had 1) */
    for (int i = 0; i < 7; ++i) {
        char name[16];
        snprintf(name, sizeof(name), "t%d", i);
        TEST("expand tab ok", clk_menu_add_tab(m, 100 + i, name) >= 0);
    }
    TEST("tab_count == 8", m->tab_count == 8);

    /* ================================================================
     *  add_item — invalid args  (void-call + check = no crash)
     * ================================================================ */
    clk_menu_add_item_str(NULL, 10, 1, "x", 0, str_opts, 3);
    TEST("add_str NULL m no crash", 1);
    clk_menu_add_item_str(m, 10, 1, NULL, 0, str_opts, 3);
    TEST("add_str NULL label no crash", 1);
    clk_menu_add_item_str(m, 10, 1, "x", 0, NULL, 3);
    TEST("add_str NULL options no crash", 1);
    clk_menu_add_item_str(m, 999, 1, "x", 0, str_opts, 3);
    TEST("add_str bad tab_id no crash", 1);
    clk_menu_add_item_str(m, 10, 1, "x", 0, str_opts, 0);
    TEST("add_str count <= 0 no crash", 1);

    /* ================================================================
     *  add_item_str
     * ================================================================ */
    clk_menu_add_item_str(m, 10, 1, "format", 2, str_opts, 3);
    TEST("str item_count == 1", m->tabs[0]->item_count == 1);
    TEST("str item[0].id == 1", m->tabs[0]->items[0]->id == 1);
    TEST("str item[0].type", m->tabs[0]->items[0]->type == CLK_MENU_TYPE_STR);
    TEST("str item[0].label", strcmp(m->tabs[0]->items[0]->label, "format") == 0);
    TEST("str option_count == 3", m->tabs[0]->items[0]->option_count == 3);
    TEST("str option_idx == 2", m->tabs[0]->items[0]->option_idx == 2);
    TEST("str value.str == opt_c", strcmp(m->tabs[0]->items[0]->value.str, "opt_c") == 0);

    /* default_idx clamp */
    clk_menu_add_item_str(m, 10, 2, "clamp_lo", -5, str_opts, 3);
    TEST("str clamp lo idx == 0", m->tabs[0]->items[1]->option_idx == 0);

    clk_menu_add_item_str(m, 10, 3, "clamp_hi", 99, str_opts, 3);
    TEST("str clamp hi idx == 2", m->tabs[0]->items[2]->option_idx == 2);

    /* ================================================================
     *  add_item_int
     * ================================================================ */
    clk_menu_add_item_int(m, 10, 10, "z order", 0, -2, 2, 1);
    TEST("int item_count == 4", m->tabs[0]->item_count == 4);
    TEST("int item[3].id == 10", m->tabs[0]->items[3]->id == 10);
    TEST("int item[3].type", m->tabs[0]->items[3]->type == CLK_MENU_TYPE_INT);
    TEST("int item[3].value.num == 0", m->tabs[0]->items[3]->value.num == 0.0);
    TEST("int item[3].min == -2", m->tabs[0]->items[3]->min_val == -2);
    TEST("int item[3].max == 2", m->tabs[0]->items[3]->max_val == 2);
    TEST("int item[3].step == 1", m->tabs[0]->items[3]->step_val == 1);

    /* int default clamp */
    clk_menu_add_item_int(m, 10, 11, "clamp", 100, 0, 10, 1);
    TEST("int clamp value == 10", m->tabs[0]->items[4]->value.num == 10);

    clk_menu_add_item_int(m, 10, 12, "clamp2", -100, 0, 10, 1);
    TEST("int clamp2 value == 0", m->tabs[0]->items[5]->value.num == 0);

    /* ================================================================
     *  add_item_bool
     * ================================================================ */
    clk_menu_add_item_bool(m, 10, 20, "dark mode", true);
    TEST("bool item_count == 7", m->tabs[0]->item_count == 7);
    TEST("bool item[6].type", m->tabs[0]->items[6]->type == CLK_MENU_TYPE_BOOL);
    TEST("bool item[6].value.b", m->tabs[0]->items[6]->value.b == true);

    /* ================================================================
     *  add_item_action
     * ================================================================ */
    clk_menu_add_item_action(m, 10, 30, "quit");
    TEST("action item_count == 8", m->tabs[0]->item_count == 8);
    TEST("action item[7].type", m->tabs[0]->items[7]->type == CLK_MENU_TYPE_ACTION);

    /* ================================================================
     *  handle_input — NONE / guard
     * ================================================================ */
    {
        clk_menu_event ev = clk_menu_handle_input(NULL, CLK_MENU_INPUT_NEXT_ITEM);
        TEST("hnd NULL input", ev.type == CLK_MENU_EVENT_NONE);
    }
    {
        /* empty menu → NONE */
        clk_menu* empty_m = clk_menu_create();
        clk_menu_event ev = clk_menu_handle_input(empty_m, CLK_MENU_INPUT_NEXT_ITEM);
        TEST("hnd no tabs", ev.type == CLK_MENU_EVENT_NONE);
        clk_menu_destroy(empty_m);
    }

    /* ================================================================
     *  handle_input — navigation
     * ================================================================ */
    m->active_tab = 0; /* tab "display" with 8 items */

    /* start: active_item == 0 */
    m->tabs[0]->active_item = 0;
    clk_menu_handle_input(m, CLK_MENU_INPUT_NEXT_ITEM);
    TEST("nav down → active=1", m->tabs[0]->active_item == 1);

    clk_menu_handle_input(m, CLK_MENU_INPUT_PREV_ITEM);
    TEST("nav up → active=0", m->tabs[0]->active_item == 0);

    /* clamp top */
    clk_menu_handle_input(m, CLK_MENU_INPUT_PREV_ITEM);
    TEST("nav up clamp → 0", m->tabs[0]->active_item == 0);

    /* clamp bottom */
    m->tabs[0]->active_item = 7; /* last item */
    clk_menu_handle_input(m, CLK_MENU_INPUT_NEXT_ITEM);
    TEST("nav down clamp → 7", m->tabs[0]->active_item == 7);

    /* ================================================================
     *  handle_input — NEXT_TAB
     * ================================================================ */
    m->active_tab = 0;
    clk_menu_handle_input(m, CLK_MENU_INPUT_NEXT_TAB);
    TEST("tab → 1", m->active_tab == 1);

    clk_menu_handle_input(m, CLK_MENU_INPUT_NEXT_TAB);
    TEST("tab → 2", m->active_tab == 2);

    /* wrap: last tab → first */
    m->active_tab = (int)m->tab_count - 1;
    clk_menu_handle_input(m, CLK_MENU_INPUT_NEXT_TAB);
    TEST("tab wrap → 0", m->active_tab == 0);

    m->active_tab = 0; /* back to "display" */

    /* ================================================================
     *  handle_input — value change: INT
     * ================================================================ */
    /* index 3 is z_order (INT, val=0, min=-2, max=2, step=1) */
    m->tabs[0]->active_item = 3;

    /* inc */
    clk_menu_event ev = clk_menu_handle_input(m, CLK_MENU_INPUT_INC_VALUE);
    TEST("int inc event type", ev.type == CLK_MENU_EVENT_VALUE_CHANGED);
    TEST("int inc tab_id=10", ev.tab_id == 10);
    TEST("int inc item_id=10", ev.item_id == 10);
    TEST("int inc val=1.0", ev.value.num == 1.0);
    TEST("int inc item val=1.0", m->tabs[0]->items[3]->value.num == 1.0);

    /* inc again */
    ev = clk_menu_handle_input(m, CLK_MENU_INPUT_INC_VALUE);
    TEST("int inc2 val=2.0", ev.value.num == 2.0);

    /* inc at max → no event */
    ev = clk_menu_handle_input(m, CLK_MENU_INPUT_INC_VALUE);
    TEST("int inc max → NONE", ev.type == CLK_MENU_EVENT_NONE);

    /* dec */
    ev = clk_menu_handle_input(m, CLK_MENU_INPUT_DEC_VALUE);
    TEST("int dec val=1.0", ev.value.num == 1.0);

    /* dec to min */
    clk_menu_handle_input(m, CLK_MENU_INPUT_DEC_VALUE); /* 0 */
    ev = clk_menu_handle_input(m, CLK_MENU_INPUT_DEC_VALUE);
    TEST("int dec val=-1.0", ev.value.num == -1.0);

    ev = clk_menu_handle_input(m, CLK_MENU_INPUT_DEC_VALUE);
    TEST("int dec val=-2.0", ev.value.num == -2.0);

    /* dec at min → no event */
    ev = clk_menu_handle_input(m, CLK_MENU_INPUT_DEC_VALUE);
    TEST("int dec min → NONE", ev.type == CLK_MENU_EVENT_NONE);

    /* ================================================================
     *  handle_input — value change: BOOL
     * ================================================================ */
    m->tabs[0]->active_item = 6; /* dark mode, init true */

    ev = clk_menu_handle_input(m, CLK_MENU_INPUT_DEC_VALUE);
    TEST("bool dec → false", ev.value.b == false);
    TEST("bool item val=false", m->tabs[0]->items[6]->value.b == false);

    ev = clk_menu_handle_input(m, CLK_MENU_INPUT_INC_VALUE);
    TEST("bool inc → true", ev.value.b == true);

    /* ================================================================
     *  handle_input — value change: STR
     * ================================================================ */
    m->tabs[0]->active_item = 0; /* format, opt_c (idx=2 of 3) */

    /* dec: 2 → 1 */
    ev = clk_menu_handle_input(m, CLK_MENU_INPUT_DEC_VALUE);
    TEST("str dec event", ev.type == CLK_MENU_EVENT_VALUE_CHANGED);
    TEST("str dec val=opt_b", strcmp(ev.value.str, "opt_b") == 0);
    TEST("str dec idx=1", m->tabs[0]->items[0]->option_idx == 1);

    /* dec: 1 → 0 */
    clk_menu_handle_input(m, CLK_MENU_INPUT_DEC_VALUE);
    TEST("str dec2 idx=0", m->tabs[0]->items[0]->option_idx == 0);

    /* dec wrap around: 0 → 2 */
    clk_menu_handle_input(m, CLK_MENU_INPUT_DEC_VALUE);
    TEST("str dec wrap idx=2", m->tabs[0]->items[0]->option_idx == 2);

    /* inc: 2 → 0 (wrap) */
    clk_menu_handle_input(m, CLK_MENU_INPUT_INC_VALUE);
    TEST("str inc wrap idx=0", m->tabs[0]->items[0]->option_idx == 0);

    /* inc: 0 → 1 */
    clk_menu_handle_input(m, CLK_MENU_INPUT_INC_VALUE);
    TEST("str inc idx=1", m->tabs[0]->items[0]->option_idx == 1);

    /* ================================================================
     *  handle_input — CONFIRM
     * ================================================================ */
    /* on STR item → NONE */
    m->tabs[0]->active_item = 0;
    ev = clk_menu_handle_input(m, CLK_MENU_INPUT_CONFIRM);
    TEST("confirm STR → NONE", ev.type == CLK_MENU_EVENT_NONE);

    /* on ACTION item → SUBMIT */
    m->tabs[0]->active_item = 7;
    ev = clk_menu_handle_input(m, CLK_MENU_INPUT_CONFIRM);
    TEST("confirm ACTION → SUBMIT", ev.type == CLK_MENU_EVENT_SUBMIT);
    TEST("confirm tab_id", ev.tab_id == 10);
    TEST("confirm item_id", ev.item_id == 30);

    /* ================================================================
     *  set_value
     * ================================================================ */
    TEST("set_str NULL m", !clk_menu_set_value_str(NULL, 10, 1, "x"));
    TEST("set_str NULL val", !clk_menu_set_value_str(m, 10, 1, NULL));
    TEST("set_str bad item", !clk_menu_set_value_str(m, 10, 999, "x"));
    TEST("set_str wrong type", !clk_menu_set_value_str(m, 10, 10, "x"));
    TEST("set_str not found", !clk_menu_set_value_str(m, 10, 1, "nope"));

    TEST("set_str success", clk_menu_set_value_str(m, 10, 1, "opt_a"));
    TEST("set_str idx=0", m->tabs[0]->items[0]->option_idx == 0);

    TEST("set_int success", clk_menu_set_value_int(m, 10, 10, 2.0));
    TEST("set_int val=2.0", m->tabs[0]->items[3]->value.num == 2.0);
    TEST("set_int clamp", clk_menu_set_value_int(m, 10, 10, 999.0));
    TEST("set_int clamped=2.0", m->tabs[0]->items[3]->value.num == 2.0);

    TEST("set_bool success", clk_menu_set_value_bool(m, 10, 20, false));
    TEST("set_bool val=false", m->tabs[0]->items[6]->value.b == false);

    /* ================================================================
     *  dynamic options
     * ================================================================ */
    clk_menu_add_option(m, 10, 1, "opt_d");
    TEST("add_opt count=4", m->tabs[0]->items[0]->option_count == 4);
    TEST("add_opt new val", strcmp(m->tabs[0]->items[0]->options[3], "opt_d") == 0);

    clk_menu_remove_option(m, 10, 1, 0); /* remove "opt_a" */
    TEST("rem_opt count=3", m->tabs[0]->items[0]->option_count == 3);
    TEST("rem_opt shift[0]", strcmp(m->tabs[0]->items[0]->options[0], "opt_b") == 0);
    TEST("rem_opt shift[1]", strcmp(m->tabs[0]->items[0]->options[1], "opt_c") == 0);
    TEST("rem_opt shift[2]", strcmp(m->tabs[0]->items[0]->options[2], "opt_d") == 0);

    /* remove with bad idx */
    clk_menu_remove_option(m, 10, 1, 99);
    TEST("rem_opt bad idx no crash", 1);

    /* clear */
    clk_menu_clear_options(m, 10, 1);
    TEST("clear count=0", m->tabs[0]->items[0]->option_count == 0);
    TEST("clear idx=0", m->tabs[0]->items[0]->option_idx == 0);
    TEST("clear options=NULL", m->tabs[0]->items[0]->options == NULL);

    /* ================================================================
     *  set_item_range
     * ================================================================ */
    clk_menu_set_item_range(m, 10, 10, -10, 10, 2);
    TEST("range min=-10", m->tabs[0]->items[3]->min_val == -10);
    TEST("range max=10", m->tabs[0]->items[3]->max_val == 10);
    TEST("range step=2", m->tabs[0]->items[3]->step_val == 2);
    /* current value 2.0 is still within [-10, 10] */
    TEST("range val kept", m->tabs[0]->items[3]->value.num == 2.0);

    /* shrink range so current value is out → clamp */
    clk_menu_set_item_range(m, 10, 10, 0, 1, 1);
    TEST("range clamp to max", m->tabs[0]->items[3]->value.num == 1.0);

    /* ================================================================
     *  remove_item
     * ================================================================ */
    size_t prev_count = m->tabs[0]->item_count;
    clk_menu_remove_item(m, 10, 30); /* remove "quit" action */
    TEST("remove_item count down", m->tabs[0]->item_count == prev_count - 1);
    TEST("remove_item id gone", !clk_menu_set_value_str(m, 10, 30, "x"));

    /* remove_item bad args */
    clk_menu_remove_item(m, 999, 1);   /* bad tab */
    clk_menu_remove_item(m, 10, 9999); /* bad item */
    TEST("remove bad args no crash", 1);

    /* ================================================================
     *  insert_item_at
     * ================================================================ */
    /* --- insert_str_at (head / middle / tail / -1) --- */
    {
        clk_menu* im = clk_menu_create();
        clk_menu_add_tab(im, 0, "ins");
        clk_menu_add_item_str_at(im, 0, 10, "B", 0, str_opts, 3, 0);
        clk_menu_add_item_str_at(im, 0, 20, "C", 0, str_opts, 3, -1);
        clk_menu_add_item_str_at(im, 0, 15, "A", 0, str_opts, 3, 0);
        TEST("insert: head→3 items", im->tabs[0]->item_count == 3);
        TEST("insert: items[0]=A", im->tabs[0]->items[0]->id == 15);
        TEST("insert: items[1]=B", im->tabs[0]->items[1]->id == 10);
        TEST("insert: items[2]=C", im->tabs[0]->items[2]->id == 20);

        clk_menu_add_item_str_at(im, 0, 30, "M", 0, str_opts, 3, 1);
        TEST("insert: middle→items[1]=M", im->tabs[0]->items[1]->id == 30);

        clk_menu_add_item_str_at(im, 0, 40, "Z", 0, str_opts, 3, -1);
        TEST("insert: -1=append→last=Z", im->tabs[0]->items[4]->id == 40);

        clk_menu_add_item_str_at(im, 0, 50, "X", 0, str_opts, 3, 999);
        TEST("insert: OOB→last=X", im->tabs[0]->items[5]->id == 50);

        clk_menu_add_item_int_at(im, 0, 60, "num", 5, 0, 10, 1, 0);
        TEST("insert_int_at head", im->tabs[0]->items[0]->id == 60);
        TEST("insert_int_at value", clk_menu_set_value_int(im, 0, 60, 7));

        clk_menu_add_item_bool_at(im, 0, 70, "flag", true, 1);
        TEST("insert_bool_at mid", im->tabs[0]->items[1]->id == 70);

        clk_menu_add_item_action_at(im, 0, 80, "act", -1);
        TEST("insert_action_at last=80",
             im->tabs[0]->items[im->tabs[0]->item_count - 1]->id == 80);

        /* bad args no crash */
        clk_menu_add_item_str_at(NULL, 0, 1, "x", 0, str_opts, 3, 0);
        clk_menu_add_item_str_at(im, 0, 1, NULL, 0, str_opts, 3, 0);
        clk_menu_add_item_str_at(im, 99, 1, "x", 0, str_opts, 3, 0);
        TEST("insert bad args no crash", 1);

        clk_menu_destroy(im);
    }

    /* ================================================================
     *  destroy
     * ================================================================ */
    clk_menu_destroy(NULL);
    TEST("destroy NULL safe", 1);

    clk_menu_destroy(m);
    TEST("destroy ok", 1);

    /* re-create → re-destroy to test full item lifecycle */
    m = clk_menu_create();
    clk_menu_add_tab(m, 0, "test");
    clk_menu_add_item_str(m, 0, 1, "x", 0, str_opts, 3);
    clk_menu_add_item_int(m, 0, 2, "y", 0, 0, 10, 1);
    clk_menu_add_item_bool(m, 0, 3, "z", true);
    clk_menu_add_item_action(m, 0, 4, "go");
    clk_menu_add_option(m, 0, 1, "opt_x");
    clk_menu_destroy(m);
    TEST("full lifecycle no crash", 1);

test_cleanup:
    printf("\n%d/%d passed\n", test_total - test_failed, test_total);
    return test_failed > 0 ? 1 : 0;
}
