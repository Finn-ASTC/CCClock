#include <stdio.h>

#include "clk_menu.h"
#include "clk_menu_instance.h"
#include "clk_menu_theme.h"
#include "clk_term.h"
#include "test_utils.h"

#define THEME_JSON "../assets/config/menu_config/menu_theme_config.json"

int main(void) {
    if (isatty_fd(fileno(stdout))) {
        g_pass = "  \033[32m[PASS]\033[0m ";
        g_fail = "  \033[31m[FAIL]\033[0m ";
    }

    if (!clk_term_init()) {
        printf("  [SKIP] No terminal — all tests skipped\n");
        printf("\n0/0 passed\n");
        return 0;
    }

    clk_menu_theme theme = {0};
    bool ok = clk_menu_theme_load(THEME_JSON, &theme);
    TEST_REQUIRE("load theme", ok);

    clk_menu* menu = clk_menu_create();
    TEST_REQUIRE("create menu", menu != NULL);

    /* ================================================================
     *  create — invalid args
     * ================================================================ */
    TEST("create NULL menu", clk_menu_instance_create(NULL, &theme) == NULL);
    TEST("create NULL theme", clk_menu_instance_create(menu, NULL) == NULL);

    /* ================================================================
     *  create — success
     * ================================================================ */
    clk_menu_instance* inst = clk_menu_instance_create(menu, &theme);
    TEST_REQUIRE("create success", inst != NULL);

    TEST("create has menu", inst->menu == menu);
    TEST("create has theme", inst->theme == &theme);
    TEST("create has sprite", inst->sprite != NULL);
    TEST("create sprite default z", inst->sprite->z_order == 0);
    TEST("create sprite pos=0,0", inst->sprite->posx == 0 && inst->sprite->posy == 0);
    TEST("create defaults", inst->active_item_pos_idx == 1 && inst->last_active_item_pos_idx == 1 &&
                                inst->align_top == true);

    /* ================================================================
     *  set_position
     * ================================================================ */
    clk_menu_instance_set_position(inst, 5, 10);
    TEST("set_position", inst->sprite->posx == 5 && inst->sprite->posy == 10);

    clk_menu_instance_set_position(NULL, 0, 0);
    TEST("set_position NULL no crash", 1);

    /* ================================================================
     *  set_size
     * ================================================================ */
    int old_w = inst->tex.tex_w, old_h = inst->tex.tex_h;

    /* below min → clamped */
    clk_menu_instance_set_size(inst, 1, 1);
    int mw = theme.min_width, mh = theme.min_height;
    TEST("set_size clamp to min w", inst->tex.tex_w == mw);
    TEST("set_size clamp to min h", inst->tex.tex_h == mh);

    /* resize bigger */
    clk_menu_instance_set_size(inst, 60, 20);
    TEST("set_size bigger w", inst->tex.tex_w == 60);
    TEST("set_size bigger h", inst->tex.tex_h == 20);
    TEST("set_size sprite tex updated", inst->sprite->tex == &inst->tex);

    /* same size = no-op */
    clk_menu_instance_set_size(inst, 60, 20);
    TEST("set_size same no-op", inst->tex.tex_w == 60);

    /* restore min */
    clk_menu_instance_set_size(inst, mw, mh);

    /* NULL guard */
    clk_menu_instance_set_size(NULL, 10, 10);
    TEST("set_size NULL no crash", 1);

    /* ================================================================
     *  set_visible
     * ================================================================ */
    clk_menu_instance_set_visible(inst, false);
    TEST("set_visible false", inst->sprite->is_hidden == true);

    clk_menu_instance_set_visible(inst, true);
    TEST("set_visible true", inst->sprite->is_hidden == false);

    clk_menu_instance_set_visible(NULL, true);
    TEST("set_visible NULL no crash", 1);

    /* ================================================================
     *  add / remove to/from term
     * ================================================================ */
    TEST("sprite not added yet", !inst->sprite_added);

    clk_menu_instance_add_to_term(inst);
    TEST("sprite added", inst->sprite_added);

    clk_menu_instance_add_to_term(inst); /* double add = no-op */
    TEST("double add no crash", inst->sprite_added);

    clk_menu_instance_remove_from_term(inst);
    TEST("sprite removed", !inst->sprite_added);

    clk_menu_instance_remove_from_term(inst); /* double remove */
    TEST("double remove no crash", 1);

    clk_menu_instance_add_to_term(NULL);
    TEST("add_to_term NULL no crash", 1);
    clk_menu_instance_remove_from_term(NULL);
    TEST("remove_from_term NULL no crash", 1);

    /* ================================================================
     *  handle_input — guards
     * ================================================================ */
    /* hide → no input */
    clk_menu_instance_set_visible(inst, false);
    {
        clk_menu_event ev = clk_menu_instance_handle_input(inst, CLK_MENU_INPUT_NEXT_ITEM);
        TEST("hnd hidden return NONE", ev.type == CLK_MENU_EVENT_NONE);
    }
    clk_menu_instance_set_visible(inst, true);

    /* handle_input NULL inst */
    {
        clk_menu_event ev = clk_menu_instance_handle_input(NULL, CLK_MENU_INPUT_NEXT_ITEM);
        TEST("hnd NULL inst", ev.type == CLK_MENU_EVENT_NONE);
    }

    /* need tabs + items to test pos_idx update */
    clk_menu_add_tab(menu, 0, "test");
    clk_menu_add_item_int(menu, 0, 1, "x", 0, 0, 10, 1);
    clk_menu_add_item_int(menu, 0, 2, "y", 0, 0, 10, 1);
    clk_menu_add_item_int(menu, 0, 3, "z", 0, 0, 10, 1);

    /* ================================================================
     *  handle_input — basic scroll + tab switch
     * ================================================================ */
    /* next item */
    {
        clk_menu_event ev = clk_menu_instance_handle_input(inst, CLK_MENU_INPUT_NEXT_ITEM);
        TEST("active moves on NEXT", menu->tabs[0]->active_item == 1);
        TEST("hnd returns event", ev.type != CLK_MENU_EVENT_NONE);
    }
    /* prev item */
    {
        clk_menu_event ev = clk_menu_instance_handle_input(inst, CLK_MENU_INPUT_PREV_ITEM);
        TEST("active back to 0", menu->tabs[0]->active_item == 0);
    }
    /* tab switch resets P */
    clk_menu_add_tab(menu, 1, "other");
    clk_menu_add_item_int(menu, 1, 10, "x", 0, 0, 10, 1);
    inst->active_item_pos_idx = 5;
    clk_menu_instance_handle_input(inst, CLK_MENU_INPUT_NEXT_TAB);
    TEST("tab switch resets P", inst->active_item_pos_idx == 1);

    /* ================================================================
     *  render — no crash
     * ================================================================ */
    clk_menu_instance_render(inst);
    TEST("render no crash", 1);

    clk_menu_instance_render(NULL);
    TEST("render NULL no crash", 1);

    /* ================================================================
     *  destroy
     * ================================================================ */
    clk_menu_instance_destroy(inst);
    TEST("destroy ok", 1);

    clk_menu_instance_destroy(NULL);
    TEST("destroy NULL safe", 1);

    clk_menu_destroy(menu);
    clk_menu_theme_destroy(&theme);

test_cleanup:
    printf("\n%d/%d passed\n", test_total - test_failed, test_total);
    return test_failed > 0 ? 1 : 0;
}
