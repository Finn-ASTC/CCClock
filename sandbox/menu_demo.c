#include <stdbool.h>
#include <stdio.h>

#include "clk_key_io.h"
#include "clk_menu.h"
#include "clk_menu_instance.h"
#include "clk_menu_theme.h"
#include "clk_term.h"

#define THEME_PATH "assets/config/menu_config/menu_theme_pinkblue.json"
#define THEME2_PATH "assets/config/menu_config/menu_theme_vivid.json"

enum { TAB_APPEARANCE, TAB_ADVANCED, TAB_ACTIONS, TAB_SPARSE };
enum {
    ITEM_TIME_FORMAT = 1,
    ITEM_Z_ORDER = 2,
    ITEM_DARK_MODE = 3,
    ITEM_SPEED = 10,
    ITEM_PRECISION = 11,
    ITEM_QUIT = 20,
    ITEM_RESET = 21,
    ITEM_SP_ON = 30,
    ITEM_SP_OFF = 31
};

static const char* time_formats[] = {"hh:MM:ss", "yyyy:mm:dd\n  hh:ss", "hh:MM", "ss:MM:hh"};
static const char* speed_opts[] = {"slow", "normal", "fast", "ultra"};

static clk_menu_input map_input(clk_key_event ev) {
    switch (ev.key) {
        case CLK_KEY_UP:
            return CLK_MENU_INPUT_PREV_ITEM;
        case CLK_KEY_DOWN:
            return CLK_MENU_INPUT_NEXT_ITEM;
        case CLK_KEY_LEFT:
            return CLK_MENU_INPUT_DEC_VALUE;
        case CLK_KEY_RIGHT:
            return CLK_MENU_INPUT_INC_VALUE;
        case '\t':
            return CLK_MENU_INPUT_NEXT_TAB;
        case '\r':
        case '\n':
            return CLK_MENU_INPUT_CONFIRM;
        default:
            return CLK_MENU_INPUT_NONE;
    }
}

int main(void) {
    if (!clk_term_init()) {
        printf("term init fail\n");
        fflush(stdout);
        return 1;
    }

    clk_menu_theme theme = {0};
    if (!clk_menu_theme_load(THEME_PATH, &theme)) {
        printf("theme load fail\n");
        fflush(stdout);
        return 1;
    }

    clk_menu* menu = clk_menu_create();
    if (!menu) {
        printf("menu create fail\n");
        fflush(stdout);
        clk_menu_theme_destroy(&theme);
        return 1;
    }

    /* ── tab appearance ── */
    clk_menu_add_tab(menu, TAB_APPEARANCE, "appearance");
    clk_menu_add_item_str(menu, TAB_APPEARANCE, ITEM_TIME_FORMAT, "time format", 0, time_formats,
                          4);
    clk_menu_add_item_int(menu, TAB_APPEARANCE, ITEM_Z_ORDER, "z order", 0, -2, 2, 1);
    clk_menu_add_item_bool(menu, TAB_APPEARANCE, ITEM_DARK_MODE, "dark mode", false);
    for (int i = 0; i < 20; ++i) {
        char label[32];
        snprintf(label, sizeof(label), "option %d", i + 1);
        clk_menu_add_item_int(menu, TAB_APPEARANCE, 100 + i, label, i, 0, 20, 1);
    }

    /* ── tab advanced ── */
    clk_menu_add_tab(menu, TAB_ADVANCED, "advanced");
    clk_menu_add_item_str(menu, TAB_ADVANCED, ITEM_SPEED, "speed", 1, speed_opts, 4);
    clk_menu_add_item_int(menu, TAB_ADVANCED, ITEM_PRECISION, "precision", 2, 0, 10, 2);
    for (int i = 0; i < 20; ++i) {
        char label[32];
        snprintf(label, sizeof(label), "param %d", i + 1);
        clk_menu_add_item_bool(menu, TAB_ADVANCED, 200 + i, label, i % 2 == 0);
    }

    /* ── tab actions ── */
    clk_menu_add_tab(menu, TAB_ACTIONS, "actions");
    clk_menu_add_item_action(menu, TAB_ACTIONS, ITEM_QUIT, "quit");
    clk_menu_add_item_action(menu, TAB_ACTIONS, ITEM_RESET, "reset to defaults");
    for (int i = 0; i < 4; ++i) {
        char label[32];
        snprintf(label, sizeof(label), "action %d", i + 1);
        clk_menu_add_item_action(menu, TAB_ACTIONS, 300 + i, label);
    }

    /* ── tab sparse — only 2 items, tests empty fill ── */
    clk_menu_add_tab(menu, TAB_SPARSE, "sparse");
    clk_menu_add_item_bool(menu, TAB_SPARSE, ITEM_SP_ON, "enable", true);
    clk_menu_add_item_bool(menu, TAB_SPARSE, ITEM_SP_OFF, "verbose", false);

    clk_menu_instance* inst = clk_menu_instance_create(menu, &theme);
    if (!inst) {
        printf("instance create fail\n");
        fflush(stdout);
        clk_menu_destroy(menu);
        clk_menu_theme_destroy(&theme);
        return 1;
    }

    clk_menu_instance_set_position(inst, 1, 1);
    clk_menu_instance_set_size(inst, 100, 30);
    clk_menu_instance_set_visible(inst, true);
    clk_menu_instance_add_to_term(inst);

    printf("\033[2J\033[H");
    printf(
        "=== Menu Test ===\n"
        "  UP/DOWN : items   LEFT/RIGHT : change value\n"
        "  TAB     : tab     ENTER/q   : quit\n"
        "  Size    : 50 x 70\n\n"
        "Press Enter...\n");
    fflush(stdout);

    while (clk_get_key_event().key == CLK_KEY_NONE)
        clk_term_sleep_ms(16);

    printf("\033[2J\033[H");
    fflush(stdout);

    bool running = true;
    while (running) {
        clk_key_event ev = clk_get_key_event();
        if (ev.key == 'q' || ev.key == 'Q') {
            running = false;
            continue;
        }

        clk_menu_event mev = clk_menu_instance_handle_input(inst, map_input(ev));
        if (mev.type == CLK_MENU_EVENT_SUBMIT && mev.item_id == ITEM_QUIT)
            running = false;

        clk_menu_instance_render(inst);
        clk_term_draw();

        /* debug overlay */
        {
            int total = menu->tabs[menu->active_tab]->item_count;
            int active = menu->tabs[menu->active_tab]->active_item;
            int fixed = 0;
            for (int s = 0; s < inst->theme->section_count; s++)
                if (inst->theme->sections[s].type != CLK_MENU_SEC_ITEM_LIST)
                    fixed += inst->theme->sections[s].row_count;
            int avail = inst->tex.tex_h - fixed;
            int rc = 0;
            for (int s = 0; s < inst->theme->section_count; s++)
                if (inst->theme->sections[s].type == CLK_MENU_SEC_ITEM_LIST) {
                    rc = inst->theme->sections[s].row_count;
                    break;
                }
            int cnt = (avail + rc - 1) / rc;
            int A = active, P = inst->active_item_pos_idx;

            printf(
                "\033[34;1H\033[2K"
                "active=%d  pos=%d  last=%d  cnt=%d  align=%s  A-P=%d  total=%d  avail=%d",
                A, P, inst->last_active_item_pos_idx, cnt, inst->align_top ? "top" : "bot", A - P,
                total, avail);
            fflush(stdout);
        }

        clk_term_sleep_ms(50);
    }

    clk_menu_instance_destroy(inst);
    clk_menu_destroy(menu);
    clk_menu_theme_destroy(&theme);
    clk_term_close();
    return 0;
}
