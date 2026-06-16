#include <stdio.h>

#include "clk_menu_theme.h"
#include "clk_term.h"
#include "test_utils.h"

#define THEME_JSON "../assets/config/menu_config/menu_theme_config.json"

int main(void) {
    if (isatty_fd(fileno(stdout))) {
        g_pass = "  \033[32m[PASS]\033[0m ";
        g_fail = "  \033[31m[FAIL]\033[0m ";
    }

    /* term must be initialised for style registration */
    if (!clk_term_init()) {
        printf("  [SKIP] No terminal — all tests skipped\n");
        printf("\n0/0 passed\n");
        return 0;
    }

    /* === load — invalid args === */
    TEST("load NULL path",      !clk_menu_theme_load(NULL, &(clk_menu_theme){0}));
    TEST("load NULL theme",     !clk_menu_theme_load(THEME_JSON, NULL));
    TEST("load bad path",       !clk_menu_theme_load("nonexistent.json", &(clk_menu_theme){0}));

    /* === load — success === */
    clk_menu_theme theme = {0};
    bool ok = clk_menu_theme_load(THEME_JSON, &theme);
    TEST_REQUIRE("load success", ok);

    TEST("def_count > 0",       theme.def_count > 0);
    TEST("section_count > 0",   theme.section_count > 0);
    TEST("layout_count > 0",    theme.layout_count > 0);

    /* === find_def === */
    const clk_menu_def* d = clk_menu_theme_find_def(&theme, "frame_top");
    TEST("find_def frame_top",  d != NULL && d->type == CLK_MENU_DEF_COMPOSITE);
    TEST("find_def NULL name",  clk_menu_theme_find_def(&theme, NULL) == NULL);
    TEST("find_def missing",    clk_menu_theme_find_def(&theme, "no_such_def") == NULL);

    /* === find_section === */
    const clk_menu_section* s = clk_menu_theme_find_section(&theme, "header");
    TEST("find_section header", s != NULL && s->type == CLK_MENU_SEC_NORMAL);
    TEST("find_section NULL",   clk_menu_theme_find_section(&theme, NULL) == NULL);
    TEST("find_section missing",clk_menu_theme_find_section(&theme, "nope") == NULL);

    /* === def special composites === */
    d = clk_menu_theme_find_def(&theme, "tab");
    TEST("tab special exists",  d != NULL && d->type == CLK_MENU_DEF_TAB);
    TEST("tab has active",      d && d->active_cnt > 0);
    TEST("tab has inactive",    d && d->inactive_cnt > 0);

    d = clk_menu_theme_find_def(&theme, "item_label");
    TEST("item_label special",  d != NULL && d->type == CLK_MENU_DEF_ITEM_LABEL);

    d = clk_menu_theme_find_def(&theme, "item_value");
    TEST("item_value special",  d != NULL && d->type == CLK_MENU_DEF_ITEM_VALUE);

    /* === def dynamic leaves === */
    d = clk_menu_theme_find_def(&theme, "tab_str");
    TEST("tab_str leaf",        d != NULL && d->type == CLK_MENU_DEF_TAB_STR);
    TEST("tab_str active_id>0", d && d->active_style_id > 0);

    d = clk_menu_theme_find_def(&theme, "item_label_str");
    TEST("item_label_str leaf", d != NULL && d->type == CLK_MENU_DEF_ITEM_LABEL_STR);

    d = clk_menu_theme_find_def(&theme, "item_value_str");
    TEST("item_value_str leaf", d != NULL && d->type == CLK_MENU_DEF_ITEM_VALUE_STR);

    /* === def string leaves === */
    d = clk_menu_theme_find_def(&theme, "corner_tl");
    TEST("corner_tl string",    d != NULL && d->type == CLK_MENU_DEF_STRING && d->string_val != NULL);

    /* === framework layout === */
    TEST("layout[0] = header",  strcmp(theme.layout[0], "header") == 0);

    /* === reload === */
    ok = clk_menu_theme_reload(THEME_JSON, &theme);
    TEST("reload success",      ok);
    TEST("reload defs > 0",     theme.def_count > 0);

    /* === destroy === */
    clk_menu_theme_destroy(NULL);
    TEST("destroy NULL safe",   1);

    clk_menu_theme_destroy(&theme);
    TEST("destroy ok",          1);

    /* === reload after destroy === */
    ok = clk_menu_theme_load(THEME_JSON, &theme);
    TEST("reload after destroy", ok);
    clk_menu_theme_destroy(&theme);

test_cleanup:
    printf("\n%d/%d passed\n", test_total - test_failed, test_total);
    return test_failed > 0 ? 1 : 0;
}
