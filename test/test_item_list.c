#include <stdio.h>
#include <string.h>

#include "clk_menu.h"
#include "test_utils.h"

static const char* str_opts[] = {"opt_a", "opt_b", "opt_c"};

int main(void) {
    if (isatty_fd(fileno(stdout))) {
        g_pass = "  \033[32m[PASS]\033[0m ";
        g_fail = "  \033[31m[FAIL]\033[0m ";
    }

    clk_item_list* list;

    /* ================================================================
     *  clk_item_list: create / destroy
     * ================================================================ */

    list = clk_item_list_create();
    TEST_REQUIRE("create succeeds", list != NULL);
    TEST("create count == 0", clk_item_list_count(list) == 0);

    clk_item_list_destroy(list);
    TEST("destroy ok", 1);

    clk_item_list_destroy(NULL);
    TEST("destroy NULL safe", 1);

    clk_item_list_add_str(NULL, 0, 1, "x", 0, str_opts, 2);
    TEST("add_str NULL list no crash", 1);

    /* re-create for remaining tests */
    list = clk_item_list_create();

    /* ================================================================
     *  add_str
     * ================================================================ */

    clk_item_list_add_str(list, 0, 1, "x", 0, NULL, 2);
    TEST("add_str NULL options no crash", 1);

    clk_item_list_add_str(list, 0, 2, "x", 0, str_opts, 0);
    TEST("add_str count <= 0 no crash", 1);

    clk_item_list_add_str(list, 0, 3, "format", 2, str_opts, 3);
    TEST("add_str count == 1", clk_item_list_count(list) == 1);
    {
        clk_menu_item* it = clk_item_list_get_at(list, 0);
        TEST_REQUIRE("add_str item != NULL", it != NULL);
        TEST("add_str item.id", it->id == 3);
        TEST("add_str item.type", it->type == CLK_MENU_TYPE_STR);
        TEST("add_str item.label", strcmp(it->label, "format") == 0);
        TEST("add_str option_count", it->option_count == 3);
        TEST("add_str option_idx", it->option_idx == 2);
        TEST("add_str value.str", strcmp(it->value.str, "opt_c") == 0);
    }

    /* default_idx clamp */
    clk_item_list_add_str(list, 0, 4, "clamp_lo", -5, str_opts, 3);
    TEST("add_str clamp_lo idx=0", clk_item_list_get_at(list, 1)->option_idx == 0);

    clk_item_list_add_str(list, 0, 5, "clamp_hi", 99, str_opts, 3);
    TEST("add_str clamp_hi idx=2", clk_item_list_get_at(list, 2)->option_idx == 2);

    /* ================================================================
     *  add_int
     * ================================================================ */

    clk_item_list_add_int(list, 0, 10, "z order", 0, -2, 2, 1);
    TEST("add_int count == 4", clk_item_list_count(list) == 4);
    {
        clk_menu_item* it = clk_item_list_get_at(list, 3);
        TEST_REQUIRE("add_int item != NULL", it != NULL);
        TEST("add_int item.id", it->id == 10);
        TEST("add_int item.type", it->type == CLK_MENU_TYPE_INT);
        TEST("add_int value.num", it->value.num == 0.0);
        TEST("add_int min_val", it->min_val == -2);
        TEST("add_int max_val", it->max_val == 2);
        TEST("add_int step_val", it->step_val == 1);
    }

    clk_item_list_add_int(list, 0, 11, "clamp_hi", 100, 0, 10, 1);
    TEST("add_int clamp_hi", clk_item_list_get_at(list, 4)->value.num == 10);

    clk_item_list_add_int(list, 0, 12, "clamp_lo", -100, 0, 10, 1);
    TEST("add_int clamp_lo", clk_item_list_get_at(list, 5)->value.num == 0);

    /* ================================================================
     *  add_bool
     * ================================================================ */

    clk_item_list_add_bool(list, 0, 20, "dark mode", true);
    TEST("add_bool count == 7", clk_item_list_count(list) == 7);
    {
        clk_menu_item* it = clk_item_list_get_at(list, 6);
        TEST("add_bool item.type", it->type == CLK_MENU_TYPE_BOOL);
        TEST("add_bool value.b", it->value.b == true);
    }

    clk_item_list_add_bool(list, 0, 21, "flag", false);
    TEST("add_bool false", clk_item_list_get_at(list, 7)->value.b == false);

    /* ================================================================
     *  add_action
     * ================================================================ */

    clk_item_list_add_action(list, 0, 30, "quit");
    TEST("add_action count == 9", clk_item_list_count(list) == 9);
    {
        clk_menu_item* it = clk_item_list_get_at(list, 8);
        TEST("add_action item.type", it->type == CLK_MENU_TYPE_ACTION);
        TEST("add_action label", strcmp(it->label, "quit") == 0);
    }

    clk_item_list_add_action(NULL, 0, 30, "x");
    TEST("add_action NULL list no crash", 1);

    /* ================================================================
     *  find / get_at / count
     * ================================================================ */

    TEST("find found", clk_item_list_find(list, 10) == clk_item_list_get_at(list, 3));
    TEST("find absent", clk_item_list_find(list, 999) == NULL);
    TEST("find NULL list", clk_item_list_find(NULL, 1) == NULL);

    TEST("get_at pos=0", clk_item_list_get_at(list, 0)->id == 3);
    TEST("get_at OOB", clk_item_list_get_at(list, 99) == NULL);
    TEST("get_at NULL", clk_item_list_get_at(NULL, 0) == NULL);

    TEST("count == 9", clk_item_list_count(list) == 9);
    TEST("count NULL", clk_item_list_count(NULL) == 0);

    /* ================================================================
     *  add_*_at — position insert
     * ================================================================ */

    /* insert at head */
    clk_item_list_add_str_at(list, 0, 40, "head", 0, str_opts, 3, 0);
    TEST("add_at head count", clk_item_list_count(list) == 10);
    TEST("add_at head item[0].id", clk_item_list_get_at(list, 0)->id == 40);

    /* insert at middle */
    clk_item_list_add_str_at(list, 0, 41, "mid", 0, str_opts, 3, 2);
    TEST("add_at mid item[2].id", clk_item_list_get_at(list, 2)->id == 41);

    /* insert at tail */
    clk_item_list_add_str_at(list, 0, 42, "tail", 0, str_opts, 3, (int)clk_item_list_count(list));
    TEST("add_at tail last.id",
         clk_item_list_get_at(list, clk_item_list_count(list) - 1)->id == 42);

    /* insert append with -1 */
    clk_item_list_add_str_at(list, 0, 43, "append", 0, str_opts, 3, -1);
    TEST("add_at append last.id",
         clk_item_list_get_at(list, clk_item_list_count(list) - 1)->id == 43);

    /* insert OOB */
    clk_item_list_add_str_at(list, 0, 44, "oob", 0, str_opts, 3, 999);
    TEST("add_at OOB last.id", clk_item_list_get_at(list, clk_item_list_count(list) - 1)->id == 44);

    /* insert int / bool / action at position */
    clk_item_list_add_int_at(list, 0, 50, "int", 5, 0, 10, 1, 0);
    TEST("add_int_at head", clk_item_list_get_at(list, 0)->id == 50);
    TEST("add_int_at value", clk_item_list_get_at(list, 0)->value.num == 5);

    clk_item_list_add_bool_at(list, 0, 51, "bool", true, 1);
    TEST("add_bool_at mid", clk_item_list_get_at(list, 1)->id == 51);

    clk_item_list_add_action_at(list, 0, 52, "act", -1);
    TEST("add_action_at last", clk_item_list_get_at(list, clk_item_list_count(list) - 1)->id == 52);

    /* insert NULL list */
    clk_item_list_add_str_at(NULL, 0, 1, "x", 0, str_opts, 3, 0);
    TEST("add_at NULL list no crash", 1);

    /* ================================================================
     *  remove
     * ================================================================ */

    {
        size_t before = clk_item_list_count(list);
        /* item 40 is at position 1 after inserts */
        clk_item_list_remove(list, 40);
        TEST("remove count down", clk_item_list_count(list) == before - 1);
        TEST("remove id gone", clk_item_list_find(list, 40) == NULL);
    }

    /* remove absent */
    clk_item_list_remove(list, 9999);
    TEST("remove absent no crash", 1);

    /* remove NULL list */
    clk_item_list_remove(NULL, 1);
    TEST("remove NULL no crash", 1);

    /* ================================================================
     *  remove_at
     * ================================================================ */

    {
        size_t before = clk_item_list_count(list);
        int head_id = clk_item_list_get_at(list, 0)->id;
        clk_item_list_remove_at(list, 0);
        TEST("remove_at head count", clk_item_list_count(list) == before - 1);
        TEST("remove_at head gone", clk_item_list_find(list, head_id) == NULL);
    }

    {
        size_t last = clk_item_list_count(list) - 1;
        int last_id = clk_item_list_get_at(list, last)->id;
        clk_item_list_remove_at(list, last);
        TEST("remove_at tail gone", clk_item_list_find(list, last_id) == NULL);
    }

    clk_item_list_remove_at(list, 999);
    TEST("remove_at OOB no crash", 1);

    clk_item_list_remove_at(NULL, 0);
    TEST("remove_at NULL no crash", 1);

    /* ================================================================
     *  clear
     * ================================================================ */

    {
        clk_item_list* cl = clk_item_list_create();
        clk_item_list_add_str(cl, 0, 1, "a", 0, str_opts, 3);
        clk_item_list_add_int(cl, 0, 2, "b", 0, 0, 10, 1);
        clk_item_list_clear(cl);
        TEST("clear count == 0", clk_item_list_count(cl) == 0);
        TEST("clear get_at NULL", clk_item_list_get_at(cl, 0) == NULL);
        clk_item_list_destroy(cl);
    }

    clk_item_list_clear(NULL);
    TEST("clear NULL no crash", 1);

    /* ================================================================
     *  set_value_str
     * ================================================================ */

    TEST("set_str NULL list", !clk_item_list_set_value_str(NULL, 1, "x"));
    TEST("set_str NULL val", !clk_item_list_set_value_str(list, 3, NULL));
    TEST("set_str bad item", !clk_item_list_set_value_str(list, 999, "opt_a"));
    TEST("set_str success", clk_item_list_set_value_str(list, 3, "opt_a"));
    TEST("set_str idx updated", clk_item_list_find(list, 3)->option_idx == 0);
    TEST("set_str not found", !clk_item_list_set_value_str(list, 3, "not_exist"));
    {
        clk_menu_item* int_item = clk_item_list_find(list, 10);
        TEST("set_str wrong type", !clk_item_list_set_value_str(list, int_item->id, "x"));
    }

    /* ================================================================
     *  set_value_int
     * ================================================================ */

    TEST("set_int NULL list", !clk_item_list_set_value_int(NULL, 10, 1));
    TEST("set_int bad item", !clk_item_list_set_value_int(list, 999, 1));
    TEST("set_int success", clk_item_list_set_value_int(list, 10, 1.0));
    TEST("set_int val updated", clk_item_list_find(list, 10)->value.num == 1.0);
    TEST("set_int clamp hi", clk_item_list_set_value_int(list, 10, 999));
    TEST("set_int clamped hi", clk_item_list_find(list, 10)->value.num == 2.0);
    TEST("set_int clamp lo", clk_item_list_set_value_int(list, 10, -999));
    TEST("set_int clamped lo", clk_item_list_find(list, 10)->value.num == -2.0);
    {
        clk_menu_item* str_item = clk_item_list_find(list, 3);
        TEST("set_int wrong type", !clk_item_list_set_value_int(list, str_item->id, 0));
    }

    /* ================================================================
     *  set_value_bool
     * ================================================================ */

    TEST("set_bool NULL list", !clk_item_list_set_value_bool(NULL, 20, false));
    TEST("set_bool bad item", !clk_item_list_set_value_bool(list, 999, false));
    TEST("set_bool success", clk_item_list_set_value_bool(list, 20, false));
    TEST("set_bool val updated", clk_item_list_find(list, 20)->value.b == false);
    TEST("set_bool toggle back", clk_item_list_set_value_bool(list, 20, true));
    {
        clk_menu_item* str_item = clk_item_list_find(list, 3);
        TEST("set_bool wrong type", !clk_item_list_set_value_bool(list, str_item->id, false));
    }

    /* ================================================================
     *  add_option
     * ================================================================ */

    {
        clk_menu_item* it = clk_item_list_find(list, 3);
        int prev_count = it->option_count;
        clk_item_list_add_option(list, 3, "opt_d");
        TEST("add_opt count +1", it->option_count == prev_count + 1);
        TEST("add_opt new val", strcmp(it->options[prev_count], "opt_d") == 0);
    }
    clk_item_list_add_option(list, 999, "x");
    TEST("add_opt bad item no crash", 1);
    clk_item_list_add_option(NULL, 3, "x");
    TEST("add_opt NULL list no crash", 1);

    /* ================================================================
     *  remove_option
     * ================================================================ */

    {
        clk_menu_item* it = clk_item_list_find(list, 3);
        int prev_count = it->option_count;
        clk_item_list_remove_option(list, 3, 0);
        TEST("rem_opt count -1", it->option_count == prev_count - 1);
        TEST("rem_opt shift", strcmp(it->options[0], "opt_b") == 0);
    }
    clk_item_list_remove_option(list, 3, 99);
    TEST("rem_opt bad idx no crash", 1);
    clk_item_list_remove_option(list, 999, 0);
    TEST("rem_opt bad item no crash", 1);
    clk_item_list_remove_option(NULL, 3, 0);
    TEST("rem_opt NULL list no crash", 1);

    /* ================================================================
     *  clear_options
     * ================================================================ */

    {
        clk_menu_item* it = clk_item_list_find(list, 3);
        clk_item_list_clear_options(list, 3);
        TEST("clear_opt count == 0", it->option_count == 0);
        TEST("clear_opt idx == 0", it->option_idx == 0);
        TEST("clear_opt options NULL", it->options == NULL);
    }
    clk_item_list_clear_options(list, 999);
    TEST("clear_opt bad item no crash", 1);
    clk_item_list_clear_options(NULL, 3);
    TEST("clear_opt NULL list no crash", 1);

    /* ================================================================
     *  set_range
     * ================================================================ */

    {
        clk_menu_item* it = clk_item_list_find(list, 10);
        clk_item_list_set_value_int(list, 10, 0);
        clk_item_list_set_range(list, 10, -10, 10, 2);
        TEST("range min updated", it->min_val == -10);
        TEST("range max updated", it->max_val == 10);
        TEST("range step updated", it->step_val == 2);
        TEST("range val kept", it->value.num == 0);

        clk_item_list_set_range(list, 10, 0, 1, 1);
        TEST("range shrink clamp", it->value.num == 0);

        clk_item_list_set_value_int(list, 10, 5);
        clk_item_list_set_range(list, 10, 0, 1, 1);
        TEST("range clamp to max", it->value.num == 1);
    }
    clk_item_list_set_range(list, 3, 0, 10, 1);
    TEST("range wrong type no crash", 1);
    clk_item_list_set_range(NULL, 10, 0, 10, 1);
    TEST("range NULL list no crash", 1);

    /* ================================================================
     *  rebuild_item
     * ================================================================ */

    {
        clk_item_list_rebuild_item(list, 3, str_opts, 3, 0);
        clk_menu_item* it = clk_item_list_find(list, 3);
        TEST("rebuild count == 3", it->option_count == 3);
        TEST("rebuild idx == 0", it->option_idx == 0);
        TEST("rebuild val", strcmp(it->value.str, "opt_a") == 0);
    }
    {
        static const char* new_opts[] = {"a", "b", "c", "d", "e"};
        clk_item_list_rebuild_item(list, 3, new_opts, 5, 3);
        clk_menu_item* it = clk_item_list_find(list, 3);
        TEST("rebuild count == 5", it->option_count == 5);
        TEST("rebuild idx == 3", it->option_idx == 3);
        TEST("rebuild val == d", strcmp(it->value.str, "d") == 0);
    }
    clk_item_list_rebuild_item(NULL, 3, str_opts, 3, 0);
    TEST("rebuild NULL list no crash", 1);

    clk_item_list_destroy(list);

    /* ================================================================
     *  clk_tab_list: create / destroy
     * ================================================================ */

    {
        clk_tab_list* tlist = clk_tab_list_create();
        TEST_REQUIRE("tab_list create succeeds", tlist != NULL);
        TEST("tab_list create count == 0", clk_tab_list_count(tlist) == 0);
        clk_tab_list_destroy(tlist);
        TEST("tab_list destroy ok", 1);
    }
    clk_tab_list_destroy(NULL);
    TEST("tab_list destroy NULL safe", 1);

    /* ================================================================
     *  clk_tab_list: add
     * ================================================================ */

    {
        clk_tab_list* tlist = clk_tab_list_create();

        TEST("tab_list add NULL name", clk_tab_list_add(tlist, 0, NULL) == -1);

        int result = clk_tab_list_add(tlist, 10, "test");
        TEST("tab_list add returns id", result == 10);
        TEST("tab_list count == 1", clk_tab_list_count(tlist) == 1);

        clk_menu_tab* tab = clk_tab_list_get_at(tlist, 0);
        TEST_REQUIRE("tab != NULL", tab != NULL);
        TEST("tab id == 10", tab->id == 10);
        TEST("tab name == test", strcmp(tab->name, "test") == 0);
        TEST("tab item_list != NULL", tab->item_list != NULL);
        TEST("tab item_list count == 0", clk_item_list_count(tab->item_list) == 0);
        TEST("tab active_item == 0", tab->active_item == 0);

        /* expansion: add 7 more to trigger realloc (default cap=6) */
        for (int i = 1; i < 8; ++i) {
            char name[16];
            snprintf(name, sizeof(name), "t%d", i);
            result = clk_tab_list_add(tlist, 100 + i, name);
            TEST("tab_list expand no fail", result >= 0);
        }
        TEST("tab_list count == 8", clk_tab_list_count(tlist) == 8);

        clk_tab_list_destroy(tlist);
    }

    /* ================================================================
     *  clk_tab_list: find / get_at / count
     * ================================================================ */

    {
        clk_tab_list* tlist = clk_tab_list_create();
        clk_tab_list_add(tlist, 42, "found");

        clk_menu_tab* found = clk_tab_list_find(tlist, 42);
        TEST("find found", found != NULL && found->id == 42);
        TEST("find absent", clk_tab_list_find(tlist, 99) == NULL);
        TEST("find NULL", clk_tab_list_find(NULL, 0) == NULL);

        TEST("get_at pos=0", clk_tab_list_get_at(tlist, 0)->id == 42);
        TEST("get_at OOB", clk_tab_list_get_at(tlist, 1) == NULL);
        TEST("get_at NULL", clk_tab_list_get_at(NULL, 0) == NULL);

        TEST("count == 1", clk_tab_list_count(tlist) == 1);
        TEST("count NULL", clk_tab_list_count(NULL) == 0);

        clk_tab_list_destroy(tlist);
    }

    /* ================================================================
     *  clk_tab_list: remove
     * ================================================================ */

    {
        clk_tab_list* tlist = clk_tab_list_create();
        clk_tab_list_add(tlist, 10, "first");
        clk_tab_list_add(tlist, 20, "second");
        clk_tab_list_add(tlist, 30, "third");

        clk_tab_list_remove(tlist, 20);
        TEST("remove count down", clk_tab_list_count(tlist) == 2);
        TEST("remove gone", clk_tab_list_find(tlist, 20) == NULL);
        TEST("remove shift head stays", clk_tab_list_get_at(tlist, 0)->id == 10);
        TEST("remove shift tail moves", clk_tab_list_get_at(tlist, 1)->id == 30);

        clk_tab_list_remove(tlist, 999);
        TEST("remove absent no crash", 1);
        clk_tab_list_remove(NULL, 10);
        TEST("remove NULL no crash", 1);

        clk_tab_list_destroy(tlist);
    }

    /* ================================================================
     *  clk_tab_list: set_item_list / get_item_list
     * ================================================================ */

    {
        clk_tab_list* tlist = clk_tab_list_create();
        clk_tab_list_add(tlist, 10, "test");

        /* set_item_list with valid new list */
        clk_item_list* new_list = clk_item_list_create();
        clk_item_list_add_str(new_list, 10, 1, "item1", 0, str_opts, 3);
        clk_item_list_add_int(new_list, 10, 2, "item2", 0, 0, 10, 1);

        clk_tab_list_set_item_list(tlist, 10, new_list);
        {
            clk_menu_tab* tab = clk_tab_list_find(tlist, 10);
            TEST_REQUIRE("set_item_list tab exists", tab != NULL);
            TEST("set_item_list active_item reset", tab->active_item == 0);
            TEST("set_item_list count", clk_item_list_count(tab->item_list) == 2);
            TEST("set_item_list item[0].id", clk_item_list_get_at(tab->item_list, 0)->id == 1);
        }

        /* set_item_list with NULL → empty list */
        clk_tab_list_set_item_list(tlist, 10, NULL);
        {
            clk_menu_tab* tab = clk_tab_list_find(tlist, 10);
            TEST("set_item_list NULL count 0", clk_item_list_count(tab->item_list) == 0);
        }

        /* set_item_list bad tab_id */
        clk_tab_list_set_item_list(tlist, 999, new_list);
        TEST("set_item_list bad tab no crash", 1);

        /* get_item_list */
        TEST("get_item_list found", clk_tab_list_get_item_list(tlist, 10) != NULL);
        TEST("get_item_list absent", clk_tab_list_get_item_list(tlist, 99) == NULL);

        clk_tab_list_destroy(tlist);
    }

    /* ================================================================
     *  item_list expansion (add many items beyond default capacity)
     * ================================================================ */

    {
        clk_item_list* big = clk_item_list_create();
        for (int i = 0; i < 50; ++i) {
            char label[16];
            snprintf(label, sizeof(label), "i%d", i);
            clk_item_list_add_int(big, 0, i, label, i, 0, 100, 1);
        }
        TEST("expansion count == 50", clk_item_list_count(big) == 50);
        TEST("expansion first id", clk_item_list_get_at(big, 0)->id == 0);
        TEST("expansion last id", clk_item_list_get_at(big, 49)->id == 49);
        clk_item_list_destroy(big);
    }

test_cleanup:
    printf("\n%d/%d passed\n", test_total - test_failed, test_total);
    return test_failed > 0 ? 1 : 0;
}
