/** Sandbox — Snake game using clk_json + clk_term + clk_key_io.
 *
 *  Reads colour config from assets/snake_config.json, registers
 *  styles via clk_term_register_style, renders the game field via
 *  clk_term_draw, and reads input via clk_get_key_event.
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "clk_json.h"
#include "clk_key_io.h"
#include "clk_term.h"

/* ---- config keys -------------------------------------------------- */
#define CFG_PATH "assets/snake_config.json"

/* ---- game constants ----------------------------------------------- */
#define MAX_SNAKE 1024

/* ---- global state ------------------------------------------------- */
static struct {
    int game_w, game_h, speed_ms;
    int head_style, body_style, food_style;
    int border_style, score_style, gameover_style, bg_style;
} cfg;

static int  snake_x[MAX_SNAKE], snake_y[MAX_SNAKE];
static int  snake_len;
static int  dir_x, dir_y;
static int  food_x, food_y;
static int  score;
static bool dead;

/* ---- helpers ------------------------------------------------------ */

/** Read a colour array [r,g,b] from a JSON object key and register it
 *  as a style. Returns the style id. */
static int load_style(clk_json_value* cfg_obj, const char* key,
                      uint8_t attrs) {
    clk_json_value* col = clk_json_object_get(cfg_obj, key);
    if (!col || !clk_json_is_object(col)) return 0;

    clk_json_value* fg = clk_json_object_get(col, "fg");
    int r = 255, g = 255, b = 255;
    if (fg && clk_json_is_array(fg) && clk_json_array_count(fg) >= 3) {
        double dt;
        clk_json_get_number(clk_json_array_get(fg, 0), &dt); r = (int)dt;
        clk_json_get_number(clk_json_array_get(fg, 1), &dt); g = (int)dt;
        clk_json_get_number(clk_json_array_get(fg, 2), &dt); b = (int)dt;
    }

    clk_json_value* at = clk_json_object_get(col, "attr");
    const char* attr_str = NULL;
    if (at) clk_json_get_string(at, &attr_str);
    if (attr_str && strcmp(attr_str, "bold") == 0) attrs |= ATTR_BOLD;
    if (attr_str && strcmp(attr_str, "dim")  == 0) attrs |= ATTR_DIM;

    return clk_term_register_style(
        (Color24){.rgb = {r, g, b}},
        (Color24){0}, attrs);
}

/** Read JSON config, populate cfg struct + register styles. */
static bool load_config(void) {
    /* read file */
    FILE* f = fopen(CFG_PATH, "rb");
    if (!f) return false;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    char* buf = malloc(sz + 1);
    fread(buf, 1, sz, f);
    buf[sz] = '\0';
    fclose(f);

    clk_json_value* root = clk_json_parse(buf);
    free(buf);
    if (!root) return false;

    clk_json_value* gw = clk_json_object_get(root, "game_w");
    clk_json_value* gh = clk_json_object_get(root, "game_h");
    clk_json_value* sp = clk_json_object_get(root, "init_speed_ms");
    double dtmp;
    clk_json_get_number(gw, &dtmp); cfg.game_w   = (int)dtmp;
    clk_json_get_number(gh, &dtmp); cfg.game_h   = (int)dtmp;
    clk_json_get_number(sp, &dtmp); cfg.speed_ms = (int)dtmp;
    if (cfg.game_w < 10 || cfg.game_h < 5) { clk_json_free(root); return false; }

    clk_json_value* colors = clk_json_object_get(root, "colors");
    cfg.head_style     = load_style(colors, "snake_head", 0);
    cfg.body_style     = load_style(colors, "snake_body", 0);
    cfg.food_style     = load_style(colors, "food",       0);
    cfg.border_style   = load_style(colors, "border",     0);
    cfg.score_style    = load_style(colors, "score",      0);
    cfg.gameover_style = load_style(colors, "game_over",  0);
    cfg.bg_style       = load_style(colors, "bg",         0);

    clk_json_free(root);
    return true;
}

static void new_food(void) {
    do {
        /* leave 2 columns for the double-width food char */
        food_x = 1 + rand() % (cfg.game_w - 3);
        food_y = 1 + rand() % (cfg.game_h - 2);
    } while (0);  /* retry if under snake — simplified for now */
}

/* ---- main --------------------------------------------------------- */

int main(void) {
    if (!clk_term_init()) { printf("term init failed\n"); return 1; }
    if (!load_config())  { printf("config load failed\n"); clk_term_close(); return 1; }

    srand((unsigned)time(NULL));

    /* init snake — centre of field, moving right */
    snake_len = 3;
    int sx = cfg.game_w / 2, sy = cfg.game_h / 2;
    for (int i = 0; i < snake_len; i++) { snake_x[i] = sx - i; snake_y[i] = sy; }
    dir_x = 1; dir_y = 0;
    new_food();
    score = 0;
    dead = false;

    /* textures: field + HUD line */
    clk_texture field = clk_texture_create(cfg.game_w + 2, cfg.game_h + 2);
    clk_texture hud   = clk_texture_create(60, 1);

    clk_sprite field_s = {.tex = &field, .posx = 1, .posy = 0, .z_order = 0};
    clk_sprite hud_s   = {.tex = &hud,   .posx = 1, .posy = cfg.game_h + 2, .z_order = 1};

    clk_term_add_sprite(&field_s);
    clk_term_add_sprite(&hud_s);

    /* game loop */
    int tick = 0;

    while (!dead) {
        /* input */
        clk_key_event ev = clk_get_key_event();
        if (ev.key == 'q' || ev.key == 'Q') break;
        if (ev.key == CLK_KEY_UP    && dir_y != 1)  { dir_x = 0;  dir_y = -1; }
        if (ev.key == CLK_KEY_DOWN  && dir_y != -1) { dir_x = 0;  dir_y = 1;  }
        if (ev.key == CLK_KEY_LEFT  && dir_x != 1)  { dir_x = -1; dir_y = 0;  }
        if (ev.key == CLK_KEY_RIGHT && dir_x != -1) { dir_x = 1;  dir_y = 0;  }

        if (++tick % 3 != 0) { clk_term_sleep_ms(cfg.speed_ms / 3); continue; }
        tick = 0;

        /* move snake: shift body, advance head */
        int tail_x = snake_x[snake_len - 1], tail_y = snake_y[snake_len - 1];
        for (int i = snake_len - 1; i > 0; i--) {
            snake_x[i] = snake_x[i - 1]; snake_y[i] = snake_y[i - 1];
        }
        snake_x[0] += dir_x; snake_y[0] += dir_y;

        /* wall collision */
        if (snake_x[0] < 0 || snake_x[0] >= cfg.game_w ||
            snake_y[0] < 0 || snake_y[0] >= cfg.game_h) {
            dead = true;
        }

        /* food collision */
        bool ate = (snake_x[0] == food_x && snake_y[0] == food_y);
        if (ate) {
            score++;
            if (snake_len < MAX_SNAKE) {
                snake_x[snake_len] = tail_x; snake_y[snake_len] = tail_y;
                snake_len++;
            }
            new_food();
        }

        /* self collision */
        for (int i = 1; i < snake_len; i++)
            if (snake_x[0] == snake_x[i] && snake_y[0] == snake_y[i])
                dead = true;

        /* ---- draw field ---- */
        clk_texture_clear_all(&field);

        /* border */
        for (int x = 0; x < cfg.game_w + 2; x++)
            for (int y = 0; y < cfg.game_h + 2; y++)
                if (x == 0 || x == cfg.game_w + 1 || y == 0 || y == cfg.game_h + 1)
                    clk_texture_write_cell(&field, x, y, "#", cfg.border_style);

        /* background fill */
        clk_texture_fill_rect(&field, 1, 1, cfg.game_w, cfg.game_h,
                              " ", cfg.bg_style);

        /* snake body */
        for (int i = snake_len - 1; i >= 0; i--)
            clk_texture_write_cell(&field, snake_x[i] + 1, snake_y[i] + 1,
                                 i == 0 ? "O" : "o",
                                 i == 0 ? cfg.head_style : cfg.body_style);

        /* food (double-width CJK "食") */
        clk_texture_write_wide_cell(&field, food_x + 1, food_y + 1,
                                  "食", cfg.food_style);

        /* ---- HUD ---- */
        clk_texture_clear_all(&hud);
        char score_buf[64];
        snprintf(score_buf, sizeof(score_buf), "SCORE: %d   Q=quit  arrows=move", score);
        if (dead)
            snprintf(score_buf, sizeof(score_buf),
                     "游戏结束 得分: %d  按q退出", score);

        clk_texture_write_string(&hud, 0, 0, score_buf,
                                 dead ? cfg.gameover_style : cfg.score_style);

        clk_term_draw();
        clk_term_sleep_ms(cfg.speed_ms);
    }

    clk_texture_destroy(&field);
    clk_texture_destroy(&hud);
    clk_term_close();
    return 0;
}
