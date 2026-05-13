#define _XOPEN_SOURCE_EXTENDED
#define _GNU_SOURCE

#include "clk_render.h"

#include <locale.h>
#include <ncursesw/ncurses.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#include <windows.h>

clk_render_texture* clk_render_texture_create(int x, int y, int width, int height, int z_index) {
    if (width <= 0 || height <= 0) {
        return NULL;
    }
    clk_render_texture* texture = calloc(1, sizeof(clk_render_texture));
    if (!texture) {
        return NULL;
    }
    texture->x = x;
    texture->y = y;
    texture->width = width;
    texture->height = height;
    texture->z_index = z_index;
    texture->cells = calloc(width * height, sizeof(clk_cell));
    if (!texture->cells) {
        free(texture);
        return NULL;
    }

    // 初始化为空白单元格
    clk_cell empty_cell = {L' ', 0};
    clk_render_texture_fill(texture, empty_cell);
    return texture;
}

void clk_render_texture_destroy(clk_render_texture* texture) {
    if (texture) {
        free(texture->cells);
        free(texture);
    }
}

bool clk_render_texture_set_cell(clk_render_texture* texture, int x, int y, clk_cell cell) {
    if (!texture || x < 0 || x >= texture->width || y < 0 || y >= texture->height) {
        return false;
    }
    int index = y * texture->width + x;
    texture->cells[index] = cell;
    return true;
}

bool clk_render_texture_fill(clk_render_texture* texture, clk_cell cell) {
    if (!texture) {
        return false;
    }
    for (int i = 0; i < texture->width * texture->height; i++) {
        texture->cells[i] = cell;
    }
    return true;
}

// 用于翻译样式id到 ncurses 颜色对 id 的映射
typedef struct {
    int style_id;
    int ncurses_pair_id;
} pair_map;

struct clk_render {
    int term_width, term_height;

    clk_cell* front_buffer;
    clk_cell* back_buffer;

    clk_style* styles;
    int style_count;
    int style_capacity;

    clk_render_texture** textures;
    int texture_count;
    int texture_capacity;

    pair_map* pair_maps;
};

static int clk_compare_texture_z_index(const void* a, const void* b) {
    const clk_render_texture* tex_a = *(const clk_render_texture**)a;
    const clk_render_texture* tex_b = *(const clk_render_texture**)b;
    return tex_a->z_index - tex_b->z_index;
}

static int clk_ensure_style(clk_render* render, int style_id) {
    if (style_id == 0)
        return 0;  // 默认样式
    if (style_id < 0 || style_id >= render->style_count)
        return -1;

    if (render->pair_maps[style_id].style_id == style_id) {
        return render->pair_maps[style_id].ncurses_pair_id;
    }

    int pair_id = style_id;  // 简单映射，实际应用中可能需要更复杂的管理
    clk_style* style = &render->styles[style_id];

    int fg_idx = style_id * 2 + CLK_STYLE_DEFAULT_QUANTITY;
    int bg_idx = style_id * 2 + CLK_STYLE_DEFAULT_QUANTITY + 1;

    // 将 RGB 颜色转换为 ncurses 的颜色索引，并初始化颜色对
    init_extended_color(fg_idx, (int)((style->fg_rgb >> 16) & 0xFF) * 1000 / 255,
                        (int)((style->fg_rgb >> 8) & 0xFF) * 1000 / 255,
                        (int)(style->fg_rgb & 0xFF) * 1000 / 255);
    init_extended_color(bg_idx, (int)((style->bg_rgb >> 16) & 0xFF) * 1000 / 255,
                        (int)((style->bg_rgb >> 8) & 0xFF) * 1000 / 255,
                        (int)(style->bg_rgb & 0xFF) * 1000 / 255);

    init_extended_pair(pair_id, fg_idx, bg_idx);

    render->pair_maps[style_id].style_id = style_id;
    render->pair_maps[style_id].ncurses_pair_id = pair_id;
    return pair_id;
}

static void clk_render_resize(clk_render* render) {
    int old_width = render->term_width, old_height = render->term_height;
    int new_width, new_height;
    getmaxyx(stdscr, new_height, new_width);

    if (new_width == old_width && new_height == old_height)
        return;

    // 如果窗口变小，清除屏幕以避免显示残留内容
    if (new_width < old_width || new_height < old_height) {
        clear();
        refresh();
    }

    int new_size = new_width * new_height;

    clk_cell* new_front_buffer = malloc(new_size * sizeof(clk_cell));
    clk_cell* new_back_buffer = malloc(new_size * sizeof(clk_cell));

    if (!new_front_buffer || !new_back_buffer) {
        free(new_front_buffer);
        free(new_back_buffer);
        return;
    }

    free(render->front_buffer);
    free(render->back_buffer);

    render->front_buffer = new_front_buffer;
    render->back_buffer = new_back_buffer;

    render->term_width = new_width;
    render->term_height = new_height;

    clk_cell empty_cell = {L' ', 0};
    for (int i = 0; i < new_size; i++) {
        render->front_buffer[i] = empty_cell;
        render->back_buffer[i] = empty_cell;
    }
    return;
}

clk_render* clk_render_create(void) {
    setlocale(LC_ALL, "");
    SetEnvironmentVariableA("TERM", "xterm-direct");
    putenv("TERM=xterm-direct");
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);
    nodelay(stdscr, TRUE);

    if (has_colors()) {
        start_color();
    }

    clk_render* render = calloc(1, sizeof(clk_render));

    if (!render) {
        endwin();
        printf("\033[0m\033[2J\033[H\033[3J\033[?25h");
        fflush(stdout);
        return NULL;
    }

    getmaxyx(stdscr, render->term_height, render->term_width);

    int size = render->term_width * render->term_height;

    clk_cell empty_cell = {L' ', 0};

    render->front_buffer = calloc(size, sizeof(clk_cell));

    if (!render->front_buffer) {
        free(render);
        endwin();
        printf("\033[0m\033[2J\033[H\033[3J\033[?25h");
        fflush(stdout);
        return NULL;
    }
    for (int i = 0; i < size; i++) {
        render->front_buffer[i] = empty_cell;
    }

    render->back_buffer = calloc(size, sizeof(clk_cell));
    if (!render->back_buffer) {
        free(render->front_buffer);
        free(render);
        endwin();
        printf("\033[0m\033[2J\033[H\033[3J\033[?25h");
        fflush(stdout);
        return NULL;
    }
    for (int i = 0; i < size; i++) {
        render->back_buffer[i] = empty_cell;
    }

    render->style_capacity = CLK_STYLE_DEFAULT_QUANTITY;
    render->styles = calloc(render->style_capacity, sizeof(clk_style));

    if (!render->styles) {
        free(render->front_buffer);
        free(render->back_buffer);
        free(render);
        endwin();
        return NULL;
    }

    render->pair_maps = calloc(render->style_capacity, sizeof(pair_map));

    if (!render->pair_maps) {
        free(render->styles);
        free(render->front_buffer);
        free(render->back_buffer);
        free(render);
        endwin();
        printf("\033[0m\033[2J\033[H\033[3J\033[?25h");
        fflush(stdout);
        return NULL;
    }

    render->styles[0] = (clk_style){.fg_rgb = 0xFFFFFF, .bg_rgb = 0x000000, .attrs = A_NORMAL};
    render->style_count = 1;

    return render;
}

void clk_render_destroy(clk_render* render) {
    if (!render) {
        return;
    }
    clear();
    refresh();

    free(render->textures);
    free(render->styles);
    free(render->pair_maps);
    free(render->front_buffer);
    free(render->back_buffer);
    free(render);
    endwin();
    printf("\033[0m\033[2J\033[H\033[3J\033[?25h");
    fflush(stdout);
}

int clk_render_register_style(clk_render* render, const clk_style* style) {
    if (!render || !style)
        return CLK_STYLE_INVALID;

    int style_id = render->style_count;
    if (style_id >= render->style_capacity) {
        int new_capacity =
            render->style_capacity ? render->style_capacity * 2 : CLK_STYLE_DEFAULT_QUANTITY;

        clk_style* new_styles = realloc(render->styles, new_capacity * sizeof(clk_style));
        if (!new_styles)
            return CLK_STYLE_INVALID;

        memset(new_styles + render->style_capacity, 0,
               (new_capacity - render->style_capacity) * sizeof(clk_style));

        pair_map* new_pair_maps = realloc(render->pair_maps, new_capacity * sizeof(pair_map));
        if (!new_pair_maps) {
            free(new_styles);
            return CLK_STYLE_INVALID;
        }

        memset(new_pair_maps + render->style_capacity, 0,
               (new_capacity - render->style_capacity) * sizeof(pair_map));

        render->styles = new_styles;
        render->pair_maps = new_pair_maps;
        render->style_capacity = new_capacity;
    }
    render->pair_maps[style_id].style_id = CLK_STYLE_INVALID;  // 标记为未初始化
    render->pair_maps[style_id].ncurses_pair_id = -1;
    render->styles[style_id] = *style;
    render->style_count++;
    return style_id;
}

bool clk_render_add_texture(clk_render* render, clk_render_texture* texture) {
    if (!render || !texture)
        return false;

    if (render->texture_count >= render->texture_capacity) {
        int new_capacity =
            render->texture_capacity ? render->texture_capacity * 2 : CLK_TEXTURE_DEFAULT_CAPACITY;
        clk_render_texture** new_textures =
            realloc(render->textures, new_capacity * sizeof(clk_render_texture*));
        if (!new_textures) {
            return false;
        }
        memset(new_textures + render->texture_capacity, 0,
               (new_capacity - render->texture_capacity) * sizeof(clk_render_texture*));
        render->texture_capacity = new_capacity;
        render->textures = new_textures;
    }
    render->textures[render->texture_count++] = texture;
    return true;
}

void clk_render_present(clk_render* render) {
    if (!render)
        return;

    clk_render_resize(render);
    int w = render->term_width;
    int h = render->term_height;

    clk_cell empty_cell = {L' ', 0};
    for (int i = 0; i < w * h; i++) {
        render->front_buffer[i] = empty_cell;
    }

    if (render->texture_count > 1) {
        qsort(render->textures, render->texture_count, sizeof(clk_render_texture*),
              clk_compare_texture_z_index);
    }

    for (int t = 0; t < render->texture_count; t++) {
        clk_render_texture* texture = render->textures[t];
        for (int tex_y = 0; tex_y < texture->height; tex_y++) {
            for (int tex_x = 0; tex_x < texture->width; tex_x++) {
                int scr_x = texture->x + tex_x;
                int scr_y = texture->y + tex_y;
                if (scr_x < 0 || scr_x >= w || scr_y < 0 || scr_y >= h) {
                    continue;
                }
                clk_cell cell = texture->cells[tex_y * texture->width + tex_x];
                if (cell.ch == L' ' && cell.style_id == 0)
                    continue;  // 跳过空白，避免覆盖下方内容
                render->front_buffer[scr_y * w + scr_x] = cell;
            }
        }
    }

    // 绘制前后缓冲区差异
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            int index = y * w + x;
            clk_cell* current_cell = &render->front_buffer[index];
            clk_cell* previous_cell = &render->back_buffer[index];

            if (current_cell->ch == previous_cell->ch &&
                current_cell->style_id == previous_cell->style_id)
                continue;

            if (current_cell->style_id < 0 || current_cell->style_id >= render->style_count) {
                continue;  // 无效样式，跳过
            }

            clk_style* style = &render->styles[current_cell->style_id];
            int pair_id = clk_ensure_style(render, current_cell->style_id);

            if (pair_id < 0) {
                continue;  // 样式设置失败，跳过
            }

            attr_t attrs = COLOR_PAIR(pair_id) | style->attrs;
            cchar_t cc;
            wchar_t wstr[2] = {current_cell->ch, L'\0'};
            setcchar(&cc, wstr, attrs, 0, NULL);
            mvadd_wch(y, x, &cc);

            // 更新后缓冲区
            *previous_cell = *current_cell;
        }
    }

    refresh();
}

void clk_render_clear(clk_render* render) {
    if (render)
        render->texture_count = 0;
}