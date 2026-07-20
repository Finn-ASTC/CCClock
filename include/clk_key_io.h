#ifndef CLK_KEY_IO_H
#define CLK_KEY_IO_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ================================================================
 *  Key mask constants — __uint128_t bit assignments
 *
 *  Each physical key and each modifier occupies one bit.  Combine
 *  them with | for compound masks (e.g. MOD_CTRL | KEY_A_UPPER).
 *
 *  Bit layout:
 *     0-25   uppercase A-Z
 *    26-51   lowercase a-z
 *    52-61   digits 0-9
 *    62-72   symbols (unshifted)
 *    73-83   symbols (shifted)
 *    84-93   punctuation (shifted digits)
 *       94   space
 *    95-98   arrows
 *   99-102  navigation
 *  103-104  deletion
 *  105-108  editing
 *  109-116  function keys F1-F8
 *  117-120  modifiers
 *  121-122  bracketed paste
 *  123-126  function keys F9-F12
 *      127  reserved
 * ================================================================ */

/* ---- Upper-case letters (0-25) ---- */
#define KEY_A_UPPER ((__uint128_t)1 << 0)
#define KEY_B_UPPER ((__uint128_t)1 << 1)
#define KEY_C_UPPER ((__uint128_t)1 << 2)
#define KEY_D_UPPER ((__uint128_t)1 << 3)
#define KEY_E_UPPER ((__uint128_t)1 << 4)
#define KEY_F_UPPER ((__uint128_t)1 << 5)
#define KEY_G_UPPER ((__uint128_t)1 << 6)
#define KEY_H_UPPER ((__uint128_t)1 << 7)
#define KEY_I_UPPER ((__uint128_t)1 << 8)
#define KEY_J_UPPER ((__uint128_t)1 << 9)
#define KEY_K_UPPER ((__uint128_t)1 << 10)
#define KEY_L_UPPER ((__uint128_t)1 << 11)
#define KEY_M_UPPER ((__uint128_t)1 << 12)
#define KEY_N_UPPER ((__uint128_t)1 << 13)
#define KEY_O_UPPER ((__uint128_t)1 << 14)
#define KEY_P_UPPER ((__uint128_t)1 << 15)
#define KEY_Q_UPPER ((__uint128_t)1 << 16)
#define KEY_R_UPPER ((__uint128_t)1 << 17)
#define KEY_S_UPPER ((__uint128_t)1 << 18)
#define KEY_T_UPPER ((__uint128_t)1 << 19)
#define KEY_U_UPPER ((__uint128_t)1 << 20)
#define KEY_V_UPPER ((__uint128_t)1 << 21)
#define KEY_W_UPPER ((__uint128_t)1 << 22)
#define KEY_X_UPPER ((__uint128_t)1 << 23)
#define KEY_Y_UPPER ((__uint128_t)1 << 24)
#define KEY_Z_UPPER ((__uint128_t)1 << 25)

/* ---- Lower-case letters (26-51) ---- */
#define KEY_a_LOWER ((__uint128_t)1 << 26)
#define KEY_b_LOWER ((__uint128_t)1 << 27)
#define KEY_c_LOWER ((__uint128_t)1 << 28)
#define KEY_d_LOWER ((__uint128_t)1 << 29)
#define KEY_e_LOWER ((__uint128_t)1 << 30)
#define KEY_f_LOWER ((__uint128_t)1 << 31)
#define KEY_g_LOWER ((__uint128_t)1 << 32)
#define KEY_h_LOWER ((__uint128_t)1 << 33)
#define KEY_i_LOWER ((__uint128_t)1 << 34)
#define KEY_j_LOWER ((__uint128_t)1 << 35)
#define KEY_k_LOWER ((__uint128_t)1 << 36)
#define KEY_l_LOWER ((__uint128_t)1 << 37)
#define KEY_m_LOWER ((__uint128_t)1 << 38)
#define KEY_n_LOWER ((__uint128_t)1 << 39)
#define KEY_o_LOWER ((__uint128_t)1 << 40)
#define KEY_p_LOWER ((__uint128_t)1 << 41)
#define KEY_q_LOWER ((__uint128_t)1 << 42)
#define KEY_r_LOWER ((__uint128_t)1 << 43)
#define KEY_s_LOWER ((__uint128_t)1 << 44)
#define KEY_t_LOWER ((__uint128_t)1 << 45)
#define KEY_u_LOWER ((__uint128_t)1 << 46)
#define KEY_v_LOWER ((__uint128_t)1 << 47)
#define KEY_w_LOWER ((__uint128_t)1 << 48)
#define KEY_x_LOWER ((__uint128_t)1 << 49)
#define KEY_y_LOWER ((__uint128_t)1 << 50)
#define KEY_z_LOWER ((__uint128_t)1 << 51)

/* ---- Digits (52-61) ---- */
#define KEY_0 ((__uint128_t)1 << 52)
#define KEY_1 ((__uint128_t)1 << 53)
#define KEY_2 ((__uint128_t)1 << 54)
#define KEY_3 ((__uint128_t)1 << 55)
#define KEY_4 ((__uint128_t)1 << 56)
#define KEY_5 ((__uint128_t)1 << 57)
#define KEY_6 ((__uint128_t)1 << 58)
#define KEY_7 ((__uint128_t)1 << 59)
#define KEY_8 ((__uint128_t)1 << 60)
#define KEY_9 ((__uint128_t)1 << 61)

/* ---- Symbols — unshifted (62-72) ---- */
#define KEY_BACKTICK ((__uint128_t)1 << 62)  /* ` */
#define KEY_DASH ((__uint128_t)1 << 63)      /* - */
#define KEY_EQUALS ((__uint128_t)1 << 64)    /* = */
#define KEY_LBRACKET ((__uint128_t)1 << 65)  /* [ */
#define KEY_RBRACKET ((__uint128_t)1 << 66)  /* ] */
#define KEY_BACKSLSH ((__uint128_t)1 << 67)  /* \ */
#define KEY_SEMICOLON ((__uint128_t)1 << 68) /* ; */
#define KEY_SQUOTE ((__uint128_t)1 << 69)    /* ' */
#define KEY_COMMA ((__uint128_t)1 << 70)     /* , */
#define KEY_DOT ((__uint128_t)1 << 71)       /* . */
#define KEY_SLASH ((__uint128_t)1 << 72)     /* / */

/* ---- Symbols — shifted (73-83) ---- */
#define KEY_TILDE ((__uint128_t)1 << 73)      /* ~ */
#define KEY_UNDERSCORE ((__uint128_t)1 << 74) /* _ */
#define KEY_PLUS ((__uint128_t)1 << 75)       /* + */
#define KEY_LCURLY ((__uint128_t)1 << 76)     /* { */
#define KEY_RCURLY ((__uint128_t)1 << 77)     /* } */
#define KEY_PIPE ((__uint128_t)1 << 78)       /* | */
#define KEY_COLON ((__uint128_t)1 << 79)      /* : */
#define KEY_DQUOTE ((__uint128_t)1 << 80)     /* " */
#define KEY_LANGLE ((__uint128_t)1 << 81)     /* < */
#define KEY_RANGLE ((__uint128_t)1 << 82)     /* > */
#define KEY_QUESTION ((__uint128_t)1 << 83)   /* ? */

/* ---- Punctuation — shifted digits (84-93) ---- */
#define KEY_EXCLAM ((__uint128_t)1 << 84)    /* ! */
#define KEY_AT ((__uint128_t)1 << 85)        /* @ */
#define KEY_HASH ((__uint128_t)1 << 86)      /* # */
#define KEY_DOLLAR ((__uint128_t)1 << 87)    /* $ */
#define KEY_PERCENT ((__uint128_t)1 << 88)   /* % */
#define KEY_CARET ((__uint128_t)1 << 89)     /* ^ */
#define KEY_AMPERSAND ((__uint128_t)1 << 90) /* & */
#define KEY_ASTERISK ((__uint128_t)1 << 91)  /* * */
#define KEY_LPAREN ((__uint128_t)1 << 92)    /* ( */
#define KEY_RPAREN ((__uint128_t)1 << 93)    /* ) */

/* ---- Space (94) ---- */
#define KEY_SPACE ((__uint128_t)1 << 94)

/* ---- Arrow keys (95-98) ---- */
#define KEY_UP ((__uint128_t)1 << 95)
#define KEY_DOWN ((__uint128_t)1 << 96)
#define KEY_LEFT ((__uint128_t)1 << 97)
#define KEY_RIGHT ((__uint128_t)1 << 98)

/* ---- Navigation (99-102) ---- */
#define KEY_HOME ((__uint128_t)1 << 99)
#define KEY_END ((__uint128_t)1 << 100)
#define KEY_PAGE_UP ((__uint128_t)1 << 101)
#define KEY_PAGE_DOWN ((__uint128_t)1 << 102)

/* ---- Deletion (103-104) ---- */
#define KEY_DEL ((__uint128_t)1 << 103)
#define KEY_INSERT ((__uint128_t)1 << 104)

/* ---- Editing keys (105-108) ---- */
#define KEY_ENTER ((__uint128_t)1 << 105)
#define KEY_TAB ((__uint128_t)1 << 106)
#define KEY_BS ((__uint128_t)1 << 107)
#define KEY_ESC ((__uint128_t)1 << 108)

/* ---- Function keys F1-F8 (109-116) ---- */
#define KEY_F1 ((__uint128_t)1 << 109)
#define KEY_F2 ((__uint128_t)1 << 110)
#define KEY_F3 ((__uint128_t)1 << 111)
#define KEY_F4 ((__uint128_t)1 << 112)
#define KEY_F5 ((__uint128_t)1 << 113)
#define KEY_F6 ((__uint128_t)1 << 114)
#define KEY_F7 ((__uint128_t)1 << 115)
#define KEY_F8 ((__uint128_t)1 << 116)

/* ---- Modifier keys (117-120) ---- */
#define MOD_SHIFT ((__uint128_t)1 << 117)
#define MOD_CTRL ((__uint128_t)1 << 118)
#define MOD_ALT ((__uint128_t)1 << 119)
#define MOD_META ((__uint128_t)1 << 120)

/* ---- Bracketed paste (121-122) ---- */
#define KEY_PASTE_START ((__uint128_t)1 << 121)
#define KEY_PASTE_END ((__uint128_t)1 << 122)

/* ---- Function keys F9-F12 (123-126) ---- */
#define KEY_F9 ((__uint128_t)1 << 123)
#define KEY_F10 ((__uint128_t)1 << 124)
#define KEY_F11 ((__uint128_t)1 << 125)
#define KEY_F12 ((__uint128_t)1 << 126)

/* ================================================================
 *  Event
 * ================================================================ */

typedef struct {
    __uint128_t key_mask; /* one or more KEY_* / MOD_* bits */
    char text[8];         /* UTF-8 text of the key, if any */
    uint8_t text_len;     /* byte length of text (0 when none) */
    bool has_text;        /* true when text was produced by this event */
} clk_key_event;

/* ================================================================
 *  Lifecycle
 * ================================================================ */

/** Initialise keyboard input: enter raw mode, start background
 *  thread, negotiate modifyOtherKeys Level 2 with the terminal.
 *  Safe to call more than once. */
void clk_key_io_init(void);

/** Restore terminal to canonical mode and stop the background
 *  thread.  Safe to call more than once. */
void clk_key_io_close(void);

/* ================================================================
 *  Mode control
 * ================================================================ */

/** Switch to NORMAL mode.  clk_normal_get_key_event() becomes
 *  the active event source.  clk_input_* primitives are no-ops. */
void clk_key_io_set_normal(void);

/** Bind an input buffer and switch to INPUT mode.
 *
 *  @p max_chars  is the maximum number of bytes the buffer can hold
 *                (excluding the NUL terminator).  The buffer must be
 *                at least max_chars + 1 bytes.
 *  @p len        tracks the current byte-length of the buffer.
 *  @p pos        tracks the cursor byte-position within the buffer. */
void clk_key_io_set_input(char* buf, size_t max_chars, size_t* len, size_t* pos);

/* ================================================================
 *  Event retrieval — call the one matching the current mode
 * ================================================================ */

/** Retrieve the next pending key event.
 *  Returns a zero-initialised event when the ring buffer is empty
 *  or the mode is not NORMAL.  Text is always empty. */
clk_key_event clk_normal_get_key_event(void);

/** Retrieve the next pending key event with optional text.  The
 *  text is NOT automatically written to the bound buffer — the
 *  caller must explicitly call clk_input_write().
 *
 *  Returns a zero-initialised event when the ring buffer is empty
 *  or the mode is not INPUT. */
clk_key_event clk_input_get_key_event(void);

/* ================================================================
 *  Text editing primitives — operate on the buffer bound via
 *  clk_key_io_set_input().  All functions use character granularity.
 * ================================================================ */

typedef enum { CLK_WRITE_INSERT, CLK_WRITE_OVERWRITE } clk_write_mode;

/** Write text at the cursor position according to @p mode.
 *
 *  CLK_WRITE_INSERT  — text after cursor is shifted right.
 *  CLK_WRITE_OVERWRITE — existing characters are replaced one-for-one;
 *                         when input runs past the end of buffer content,
 *                         excess text is appended.
 *
 *  Both modes write as many complete UTF-8 characters as fit.
 *  Returns true only when every byte was written
 *  (false indicates truncation; partial writes are kept). */
bool clk_input_write(clk_write_mode mode, const char* text, size_t byte_len);

/** Move the cursor by @p offset characters.
 *  Negative = left, positive = right.  Clamped to valid range. */
void clk_input_move_cursor(int offset);

/** Delete the character before the cursor (Backspace).
 *  Cursor and byte-length are both updated.  Returns false when
 *  the cursor is at position 0. */
bool clk_input_delete_before(void);

/** Delete the character at the cursor position (Delete key).
 *  Byte-length is updated; cursor stays in place.  Returns false
 *  when the cursor is at the end of the buffer. */
bool clk_input_delete_after(void);

/* ================================================================
 *  Utility
 * ================================================================ */

/** Return true when @p ev matches the given @p key and @p mods. */
bool clk_key_is(clk_key_event ev, __uint128_t key, __uint128_t mods);

/* ================================================================
 *  Test hook
 * ================================================================ */

/** Inject an event directly into the ring buffer for testing. */
void clk_key_io_test_inject(clk_key_event ev);

/** Feed a raw byte into the parser and push the resulting event. */
void clk_key_io_test_inject_raw(int ch);

/** Queue a byte for the next raw_getch() call (for CSI/SS3 testing). */
void clk_key_io_test_queue_byte(int ch);

/** Reset parser state and drain the ring buffer (for test isolation). */
void clk_key_io_test_reset(void);

/** Pause the background thread (for deterministic testing). */
void clk_key_io_test_pause(void);

/** Resume the background thread after clk_key_io_test_pause(). */
void clk_key_io_test_resume(void);

#ifdef __cplusplus
}
#endif

#endif /* CLK_KEY_IO_H */
