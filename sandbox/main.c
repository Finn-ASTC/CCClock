#include <stdbool.h>
#include <stdio.h>

#include "clk_key_io.h"
#include "clk_term.h"

int main(void) {
    if (!clk_term_init()) {
        printf("初始化");
        return 1;
    }
    printf("按任意键查看事件（按 q 退出）\n");
    bool running = true;
    while (running) {
        clk_key_event ev = clk_get_key_event();
        if (ev.key == CLK_KEY_NONE)
            continue;
        printf("key=%08X  raw=%08X  ", ev.key, ev.raw);
        switch (ev.key) {
            case 'q':
                printf("→ 退出\n");
                running = false;
                break;
            case CLK_KEY_UP:
                printf("→ ↑\n");
                break;
            case CLK_KEY_DOWN:
                printf("→ ↓\n");
                break;
            case CLK_KEY_LEFT:
                printf("→ ←\n");
                break;
            case CLK_KEY_RIGHT:
                printf("→ →\n");
                break;
            case CLK_KEY_ESC:
                printf("→ ESC\n");
                break;
            default:
                printf("→ '%c'\n", (char)ev.key);
                break;
        }
    }
    clk_term_close();
    return 0;
}
