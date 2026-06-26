#include <stdio.h>
#include <string.h>

#include "clk_clock_menu.h"
#include "test_utils.h"

int main(void) {
    clk_menu* menu = NULL;
    char* tf0 = NULL, *tf1 = NULL, *fn0 = NULL, *fn1 = NULL, *tn0 = NULL;
    const char** wrapped = NULL;
    char* paths0 = NULL, *paths1 = NULL;
    const char** names = NULL;

    if (isatty_fd(fileno(stdout))) {
        g_pass = "  \033[32m[PASS]\033[0m ";
        g_fail = "  \033[31m[FAIL]\033[0m ";
    }

    menu = clk_menu_create();
    TEST_REQUIRE("menu create", menu != NULL);

    tf0 = strdup("hh:MM:ss");
    tf1 = strdup("hh:MM");
    char* tf_arr[2] = {tf0, tf1};
    const char* tf_opts[2] = {tf0, tf1};

    fn0 = strdup("ascii_num");
    fn1 = strdup("simple");
    char* fn_arr[2] = {fn0, fn1};
    const char* fnames[2] = {fn0, fn1};

    tn0 = strdup("default");
    char* tn_arr[1] = {tn0};
    const char* tnames[1] = {tn0};

    /* --- build --- */
    clk_clock_menu_build(menu, tf_opts, 2, 0, fnames, 2, 1, tnames, 1, 0);
    TEST("build: tab_count == 1", menu->tab_count == 1);
    TEST("build: tab name clock", strcmp(menu->tabs[0]->name, "clock") == 0);
    TEST("build: 4 items", menu->tabs[0]->item_count == 4);

    /* --- rebuild_item --- */
    clk_clock_menu_rebuild_item(menu, 0, CLK_MENU_CLOCK_TFMT, tf_opts, 2, 1);
    TEST("rebuild: tfmt option_idx=1", menu->tabs[0]->items[0]->option_idx == 1);
    TEST("rebuild: tfmt value==hh:MM",
         strcmp(menu->tabs[0]->items[0]->value.str, "hh:MM") == 0);

    /* --- handle_event --- */
    int a = 0, b = 0, c = 0;
    clk_menu_event event;
    memset(&event, 0, sizeof(event));

    event.type = CLK_MENU_EVENT_SUBMIT;
    event.item_id = CLK_MENU_CLOCK_QUIT;
    TEST("handle_event: quit → true", clk_clock_menu_handle_event(&event, &a, &b, &c));

    event.type = CLK_MENU_EVENT_VALUE_CHANGED;
    event.item_id = CLK_MENU_CLOCK_TFMT;
    TEST("handle_event: tfmt change → false", !clk_clock_menu_handle_event(&event, &a, &b, &c));

    event.type = CLK_MENU_EVENT_NONE;
    TEST("handle_event: NONE → false", !clk_clock_menu_handle_event(&event, &a, &b, &c));

    TEST("handle_event: NULL → false", !clk_clock_menu_handle_event(NULL, &a, &b, &c));

    /* --- translate_key --- */
    clk_key_event ke = {.key = CLK_KEY_UP};
    TEST("translate_key: UP → PREV_ITEM",
         clk_clock_menu_translate_key(ke) == CLK_MENU_INPUT_PREV_ITEM);
    ke.key = CLK_KEY_DOWN;
    TEST("translate_key: DOWN → NEXT_ITEM",
         clk_clock_menu_translate_key(ke) == CLK_MENU_INPUT_NEXT_ITEM);
    ke.key = '\t';
    TEST("translate_key: TAB → NEXT_TAB",
         clk_clock_menu_translate_key(ke) == CLK_MENU_INPUT_NEXT_TAB);
    ke.key = '\r';
    TEST("translate_key: ENTER → CONFIRM",
         clk_clock_menu_translate_key(ke) == CLK_MENU_INPUT_CONFIRM);
    ke.key = 'x';
    TEST("translate_key: unknown → NONE",
         clk_clock_menu_translate_key(ke) == CLK_MENU_INPUT_NONE);

    /* --- wrap_strings --- */
    wrapped = clk_clock_menu_wrap_strings(tf_arr, 2);
    TEST("wrap: returns non-null", wrapped != NULL);
    TEST("wrap: [0] == tf[0]", strcmp(wrapped[0], tf_arr[0]) == 0);
    TEST("wrap: [1] == tf[1]", strcmp(wrapped[1], tf_arr[1]) == 0);

    /* --- build_names --- */
    paths0 = strdup("dir/font.json");
    paths1 = strdup("dir/theme.json");
    char* paths_arr[2] = {paths0, paths1};
    names = clk_clock_menu_build_names(paths_arr, 2);
    TEST("build_names: [0]=font", strcmp(names[0], "font") == 0);
    TEST("build_names: [1]=theme", strcmp(names[1], "theme") == 0);

    free((void*)names[0]);
    free((void*)names[1]);
    free(names);
    names = NULL;
    free(wrapped);
    wrapped = NULL;
    clk_menu_destroy(menu);
    menu = NULL;

    free(tf0); free(tf1);
    free(fn0); free(fn1); free(tn0);
    free(paths0); free(paths1);

    printf("\n%d/%d passed\n", test_total - test_failed, test_total);
    return test_failed > 0 ? 1 : 0;

test_cleanup:
    if (wrapped) free(wrapped);
    if (names) {
        if (names[0]) free((void*)names[0]);
        if (names[1]) free((void*)names[1]);
        free(names);
    }
    if (menu) clk_menu_destroy(menu);
    free(tf0); free(tf1); free(fn0); free(fn1); free(tn0);
    free(paths0); free(paths1);
    printf("\n%d/%d passed\n", test_total - test_failed, test_total);
    return 1;
}
