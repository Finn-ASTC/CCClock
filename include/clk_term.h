#ifndef CLK_TERM_H
#define CLK_TERM_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
} clk_texture;

void clk_term_init(void);
void clk_term_close(void);

void clk_add_texture_to_term(const clk_texture* texture);

void clk_term_draw(void);

void clk_update_term(void);

#ifdef __cplusplus
}
#endif

#endif  // CLK_TERM_H
