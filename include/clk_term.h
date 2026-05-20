#ifndef CLK_TERM_H
#define CLK_TERM_H

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
    } ch;
    uint32_t raw;
} Color24;

typedef struct {
    Color24 fg_color;
    Color24 bg_color;
    uint8_t attrs;
} clk_cell;

typedef struct {
    int posx, posy;
    int tex_w, tex_h;
    clk_cell** texture;
} clk_texture;

void clk_term_init(void);
void clk_term_close(void);

void clk_add_texture_to_term(const clk_texture* const texture);

void clk_term_draw(void);

void clk_update_term(void);

#ifdef __cplusplus
}
#endif

#endif  // CLK_TERM_H
