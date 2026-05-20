#include <stdio.h>

#include "clk_key_io.h"

int main(void) {
    clk_key_io_init();
    printf("[PASS] 初始化正常\n");

    clk_key_io_close();
    printf("[PASS] 退出正常\n");
}
