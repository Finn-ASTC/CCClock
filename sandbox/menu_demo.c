#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "clk_key_io.h"
#include "clk_menu.h"
#include "clk_menu_instance.h"
#include "clk_menu_theme.h"
#include "clk_term.h"

#define THEME_PATH "assets/config/menu_config/menu_theme_pinkblue.json"

static const char* theme_list[] = {"assets/config/menu_config/menu_theme_pinkblue.json",
                                   "assets/config/menu_config/menu_theme_vivid.json",
                                   "assets/config/menu_config/menu_theme_config.json"};
static const char* theme_names[] = {"pinkblue", "vivid", "grey"};

enum { TAB_APPEARANCE, TAB_ADVANCED, TAB_ACTIONS, TAB_SPARSE, TAB_LAYOUT };
enum {
    ITEM_TIME_FORMAT = 1,
    ITEM_Z_ORDER = 2,
    ITEM_DARK_MODE = 3,
    ITEM_SPEED = 10,
    ITEM_PRECISION = 11,
    ITEM_QUIT = 20,
    ITEM_RESET = 21,
    ITEM_SP_ON = 30,
    ITEM_SP_OFF = 31,
    ITEM_POS_X = 40,
    ITEM_POS_Y = 41,
    ITEM_WIDTH = 42,
    ITEM_HEIGHT = 43,
    ITEM_THEME = 44
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

    /* ── tab layout — self-modifying: move / resize the menu ── */
    clk_menu_add_tab(menu, TAB_LAYOUT, "layout");
    clk_menu_add_item_int(menu, TAB_LAYOUT, ITEM_POS_X, "pos x", 1, 0, 200, 1);
    clk_menu_add_item_int(menu, TAB_LAYOUT, ITEM_POS_Y, "pos y", 1, 0, 200, 1);
    clk_menu_add_item_int(menu, TAB_LAYOUT, ITEM_WIDTH, "width", 100, 30, 300, 1);
    clk_menu_add_item_int(menu, TAB_LAYOUT, ITEM_HEIGHT, "height", 30, 10, 100, 1);
    clk_menu_add_item_str(menu, TAB_LAYOUT, ITEM_THEME, "theme", 0, theme_names, 3);

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
        "  TAB     : tab     ENTER/q   : quit\n\n"
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

        /* apply layout changes to the instance itself */
        if (mev.type == CLK_MENU_EVENT_VALUE_CHANGED) {
            switch (mev.item_id) {
                case ITEM_POS_X:
                    clk_menu_instance_set_position(inst, (int)mev.value.num, inst->sprite->posy);
                    break;
                case ITEM_POS_Y:
                    clk_menu_instance_set_position(inst, inst->sprite->posx, (int)mev.value.num);
                    break;
                case ITEM_WIDTH:
                    clk_menu_instance_set_size(inst, (int)mev.value.num, inst->tex.tex_h);
                    break;
                case ITEM_HEIGHT:
                    clk_menu_instance_set_size(inst, inst->tex.tex_w, (int)mev.value.num);
                    break;
                case ITEM_THEME: {
                    /* reload theme and recreate instance */
                    int px = inst->sprite->posx, py = inst->sprite->posy;
                    int tw = inst->tex.tex_w, th = inst->tex.tex_h;
                    clk_menu_instance_remove_from_term(inst);
                    clk_menu_instance_destroy(inst);
                    clk_menu_theme_destroy(&theme);
                    memset(&theme, 0, sizeof(theme));
                    for (int ti = 0; ti < 3; ++ti) {
                        if (strcmp(mev.value.str, theme_names[ti]) == 0) {
                            clk_menu_theme_load(theme_list[ti], &theme);
                            break;
                        }
                    }
                    inst = clk_menu_instance_create(menu, &theme);
                    clk_menu_instance_set_position(inst, px, py);
                    clk_menu_instance_set_size(inst, tw, th);
                    clk_menu_instance_set_visible(inst, true);
                    clk_menu_instance_add_to_term(inst);
                    break;
                }
            }
        }

        clk_menu_instance_render(inst);
        clk_term_draw();
        clk_term_sleep_ms(50);
    }

    clk_menu_instance_destroy(inst);
    clk_menu_destroy(menu);
    clk_menu_theme_destroy(&theme);
    clk_term_close();
    return 0;
}
