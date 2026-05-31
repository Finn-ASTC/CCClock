#ifndef CLK_KEY_IO_H
#define CLK_KEY_IO_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------
 *  Virtual key codes
 * ------------------------------------------------------------------ */

#define CLK_KEY_NONE  0xFFFFFFFF

#define CLK_KEY_UP    0xE000
#define CLK_KEY_DOWN  0xE001
#define CLK_KEY_LEFT  0xE002
#define CLK_KEY_RIGHT 0xE003
#define CLK_KEY_ESC   27

/* ------------------------------------------------------------------
 *  Types
 * ------------------------------------------------------------------ */

typedef struct {
    uint32_t key;             /* virtual key (CLK_KEY_*) or ASCII char */
    uint32_t raw;             /* platform-specific raw scancode */
    uint32_t _reserved[2];
} clk_key_event;

/* ------------------------------------------------------------------
 *  Lifecycle
 * ------------------------------------------------------------------ */

/** Enable non-blocking keyboard input. Safe to call more than once. */
void clk_key_io_init(void);

/** Restore terminal to canonical mode and disable key reading. */
void clk_key_io_close(void);

/* ------------------------------------------------------------------
 *  Input
 * ------------------------------------------------------------------ */

/** Return the next pending key event, or CLK_KEY_NONE if the input
 *  buffer is empty. Non-blocking — never waits for input. */
clk_key_event clk_get_key_event(void);

#ifdef __cplusplus
}
#endif

#endif  /* CLK_KEY_IO_H */
