#include <stdio.h>

#include "clk_menu.h"
#include "test_utils.h"

int main(void) {
    if (isatty_fd(fileno(stdout))) {
        g_pass = "  \033[32m[PASS]\033[0m ";
        g_fail = "  \033[31m[FAIL]\033[0m ";
    }

    /* === clk_menu_create === */
    clk_menu* m = clk_menu_create();
    TEST_REQUIRE("create succeeds", m != NULL);

    TEST("create tab_capacity == 6", m->tab_capacity == 6);
    TEST("create tab_count == 0",    m->tab_count == 0);
    TEST("create tabs != NULL",      m->tabs != NULL);
    TEST("create active_tab == 0",   m->active_tab == 0);
    TEST("create scroll_offset == 0", m->scroll_offset == 0);
    TEST("create visible == false",   !m->visible);

    /* === clk_menu_add_tab — invalid args === */
    TEST("add_tab NULL menu fails",   clk_menu_add_tab(NULL, "test") == -1);
    TEST("add_tab NULL name fails",   clk_menu_add_tab(m, NULL) == -1);

    /* === clk_menu_add_tab — success === */
    int tab0 = clk_menu_add_tab(m, "display");
    TEST("add_tab first returns 0",   tab0 == 0);
    TEST("tab_count == 1",            m->tab_count == 1);
    TEST("first tab name",            strcmp(m->tabs[0]->name, "display") == 0);
    TEST("first tab id == 0",         m->tabs[0]->id == 0);
    TEST("first tab items prealloc",  m->tabs[0]->items != NULL);
    TEST("first tab item_cap == 6",   m->tabs[0]->item_capacity == 6);
    TEST("first tab item_count == 0", m->tabs[0]->item_count == 0);

    int tab1 = clk_menu_add_tab(m, "color");
    TEST("add_tab second returns 1",  tab1 == 1);
    TEST("tab_count == 2",            m->tab_count == 2);
    TEST("second tab name",           strcmp(m->tabs[1]->name, "color") == 0);
    TEST("second tab id == 1",        m->tabs[1]->id == 1);

    /* === clk_menu_add_tab — capacity expansion === */
    /* default capacity is 6; add enough tabs to trigger expansion */
    for (int i = 0; i < 6; ++i) {
        char name[16];
        snprintf(name, sizeof(name), "tab%d", i);
        int id = clk_menu_add_tab(m, name);
        TEST("expand tab no crash", id >= 0);
    }
    TEST("tab_count == 8",    m->tab_count == 8);
    TEST("tab_capacity >= 8", m->tab_capacity >= 8);

    /* === clk_menu_destroy — NULL safe === */
    clk_menu_destroy(NULL);
    TEST("destroy NULL no crash", 1);

    /* === clk_menu_destroy — normal cleanup === */
    clk_menu_destroy(m);
    TEST("destroy no crash", 1);

    /* === re-create after destroy === */
    m = clk_menu_create();
    TEST("re-create succeeds", m != NULL);
    int tab2 = clk_menu_add_tab(m, "settings");
    TEST("re-create add_tab returns 0", tab2 == 0);
    TEST("re-create tab name", strcmp(m->tabs[0]->name, "settings") == 0);
    clk_menu_destroy(m);

test_cleanup:
    printf("\n%d/%d passed\n", test_total - test_failed, test_total);
    return test_failed > 0 ? 1 : 0;
}
