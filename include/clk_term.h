#ifndef CLK_TERM_H
#define CLK_TERM_H

#include <stdbool.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

#define ATTR_NONE 0x00       // 00000000 无属性
#define ATTR_BOLD 0x01       // 00000001 加粗 (ANSI: 1)
#define ATTR_DIM 0x02        // 00000010 暗淡 (ANSI: 2)
#define ATTR_ITALIC 0x04     // 00000100 斜体 (ANSI: 3)
#define ATTR_UNDERLINE 0x08  // 00001000 下划线 (ANSI: 4)
#define ATTR_BLINK 0x10      // 00010000 闪烁 (ANSI: 5)
#define ATTR_REVERSE 0x20    // 00100000 反显/反色 (ANSI: 7)
#define ATTR_HIDDEN 0x40     // 01000000 隐藏 (ANSI: 8)
#define ATTR_STRIKE 0x80     // 10000000 删除线 (ANSI: 9)

typedef union {
    struct {
        uint8_t r;
        uint8_t g;
        uint8_t b;
        uint8_t a;
    } rgb;
    uint32_t raw;
} Color24;

typedef struct {
    Color24 fg_color;
    Color24 bg_color;
    uint8_t attrs;
} clk_style;

typedef struct {
    char cell_tex[5];
    int style_id;
    bool is_empty;
} clk_cell;

typedef struct {
    int posx, posy;
    int tex_w, tex_h;
    int tex_z_order;
    clk_cell* data;
    bool is_invalid;
} clk_texture;

bool clk_term_init(void);
void clk_term_close(void);

bool clk_add_texture_to_render_list(const clk_texture* texture);

void clk_term_draw(void);

bool clk_update_term(void);

bool clk_get_term_size(int* term_w, int* term_h);

int clk_register_style(Color24 fg, Color24 bg, uint8_t attrs);

clk_texture clk_texture_create(int w, int h);

void clk_texture_destroy(clk_texture* tex);

void clk_texture_set_cell(clk_texture* tex, int x, int y, const char* ch, int style_id);

void clk_texture_fill_rect(clk_texture* tex, int x, int y, int w, int h, const char* ch,
                           int style_id);

void clk_texture_write_string(clk_texture* tex, int x, int y, const char* str, int style_id);

void clk_texture_clear_cell(clk_texture* tex, int x, int y);

void clk_texture_clear_all(clk_texture* tex);

void clk_texture_set_pos(clk_texture* tex, int x, int y);
void clk_texture_set_z_order(clk_texture* tex, int z);
void clk_texture_set_invalid(clk_texture* tex, bool invalid);
void clk_texture_set_pos_z(clk_texture* tex, int x, int y, int z);

#ifdef __cplusplus
}
#endif

#endif  // CLK_TERM_H
