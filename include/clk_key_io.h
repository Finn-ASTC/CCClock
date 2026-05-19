#ifndef CLK_KEY_IO_H
#define CLK_KEY_IO_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CLK_KEY_NONE 0xFFFFFFFF

#define CLK_KEY_UP 0xE000
#define CLK_KEY_DOWN 0xE001
#define CLK_KEY_LEFT 0xE002
#define CLK_KEY_RIGHT 0xE003
#define CLK_KEY_ESC 27

typedef struct {
    uint32_t key;
    uint32_t raw;
    uint32_t _reserved[2];
} clk_key_event;

void clk_key_io_init(void);
void clk_key_io_close(void);

clk_key_event clk_get_key_event(void);

#ifdef __cplusplus
}
#endif

#endif  // CLK_KEY_IO_H
