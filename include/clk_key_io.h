#ifndef CLK_KEY_IO_H
#define CLK_KEY_IO_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------
 *  Virtual key codes
 * ------------------------------------------------------------------ */

#define CLK_KEY_NONE 0xFFFFFFFF

#define CLK_KEY_UP 0xE000
#define CLK_KEY_DOWN 0xE001
#define CLK_KEY_LEFT 0xE002
#define CLK_KEY_RIGHT 0xE003
#define CLK_KEY_ESC 27

/* ------------------------------------------------------------------
 *  Types
 * ------------------------------------------------------------------ */

typedef struct {
    uint32_t key; /* virtual key (CLK_KEY_*) or ASCII char */
    uint32_t raw; /* platform-specific raw scancode */
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
 *  buffer is empty.  Non-blocking — never waits for input.
 *
 *  When string-input mode is active this function always returns
 *  CLK_KEY_NONE — keys go to clk_key_io_text_poll() instead. */
clk_key_event clk_get_key_event(void);

/* ------------------------------------------------------------------
 *  String input
 * ------------------------------------------------------------------ */

/** Enter string-input mode.  Binds @p buf for accumulation.  While
 *  active, clk_get_key_event() returns CLK_KEY_NONE automatically. */
void clk_key_io_text_start(char* buf, size_t buf_size, size_t* len, size_t* pos);

/** Poll one character from the ring buffer and append it to the bound
 *  buffer.  Supports Left/Right arrows, Backspace, Delete, and
 *  printable ASCII (32-126).  Insert semantics — new characters are
 *  placed at *pos.
 *
 *  Returns '\r' on Enter, CLK_KEY_ESC on cancel, or CLK_KEY_NONE
 *  while input is still in progress.  On Enter or ESC the mode
 *  switches back to normal automatically. */
uint32_t clk_key_io_text_poll(void);

/** Exit string-input mode early without processing. */
void clk_key_io_text_stop(void);

#ifdef __cplusplus
}
#endif

#endif /* CLK_KEY_IO_H */
