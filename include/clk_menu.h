#ifndef CLK_MENU_H
#define CLK_MENU_H

#include "clk_term.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
} clk_menu;

void clk_destroy_menu(clk_menu* menu);

clk_texture clk_menu_to_texture(const clk_menu* menu);

#ifdef __cplusplus
}
#endif

#endif  // CLK_MENU_H
