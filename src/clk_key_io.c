#include "clk_key_io.h"

#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "clk_time.h"

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#else
#include <sys/select.h>
#include <termios.h>
#include <unistd.h>
#endif

/* ================================================================
 *  Ring buffer
 * ================================================================ */

#define RING_SIZE 256

static clk_key_event ring[RING_SIZE];
static int ring_write;
static int ring_read;
static pthread_mutex_t ring_mutex;

static void ring_push(clk_key_event ev) {
    pthread_mutex_lock(&ring_mutex);
    ring[ring_write] = ev;
    ring_write = (ring_write + 1) % RING_SIZE;
    if (ring_write == ring_read)
        ring_read = (ring_read + 1) % RING_SIZE;
    pthread_mutex_unlock(&ring_mutex);
}

static clk_key_event ring_pop(void) {
    clk_key_event ev = {0};
    pthread_mutex_lock(&ring_mutex);
    if (ring_read != ring_write) {
        ev = ring[ring_read];
        ring_read = (ring_read + 1) % RING_SIZE;
    }
    pthread_mutex_unlock(&ring_mutex);
    return ev;
}

/* ================================================================
 *  Mode state
 * ================================================================ */

typedef enum { MODE_NORMAL, MODE_INPUT } key_mode_t;

static key_mode_t current_mode = MODE_NORMAL;
static char* input_buf;
static size_t input_buf_max_bytes; /* derived byte ceiling */
static size_t *input_len, *input_pos;

#ifndef _WIN32
static struct termios old_tio;
#else
static HANDLE g_hStdin;
static DWORD g_old_console_mode;
static bool g_console_mode_saved;
#endif

static int test_next_byte = -1; /* test hook — override for raw_getch */
static size_t ra_pos, ra_len;   /* readahead cursor + length (used by fill_readahead) */

/* ================================================================
 *  Platform helpers
 * ================================================================ */

#if defined(_WIN32) || defined(_WIN64)

#include <conio.h> /* _kbhit / _getch */

/* ---- readahead buffer — one greedy ReadFile per burst, then serve from cache ---- */
static unsigned char ra_buf[4096];

static int fill_readahead(void) {
    ra_pos = 0;
    ra_len = 0;

    DWORD available;
    /* real pipe — read exactly the known count */
    if (PeekNamedPipe(g_hStdin, NULL, 0, NULL, &available, NULL)) {
        if (available == 0)
            return -1;
        DWORD to_read = available < sizeof(ra_buf) ? available : (DWORD)sizeof(ra_buf);
        DWORD read;
        if (ReadFile(g_hStdin, ra_buf, to_read, &read, NULL) && read > 0) {
            ra_len = read;
            return 0;
        }
        return -1;
    }

    /* ConPTY / console — peek first so ReadFile never blocks */
    if (WaitForSingleObject(g_hStdin, 0) != WAIT_OBJECT_0)
        return -1;
    {
        DWORD read;
        if (ReadFile(g_hStdin, ra_buf, sizeof(ra_buf), &read, NULL) && read > 0) {
            ra_len = read;
            return 0;
        }
    }

    return -1;
}

static bool raw_kbhit(void) {
    if (test_next_byte >= 0)
        return true;
    if (ra_pos < ra_len)
        return true;
    if (fill_readahead() == 0)
        return true;
    return _kbhit() != 0;
}

static int raw_getch(void) {
    if (test_next_byte >= 0) {
        int ch = test_next_byte;
        test_next_byte = -1;
        return ch;
    }
    if (ra_pos < ra_len)
        return ra_buf[ra_pos++];
    if (fill_readahead() == 0)
        return ra_buf[ra_pos++];
    return -1;
}

#else /* POSIX */

static bool raw_kbhit(void) {
    if (test_next_byte >= 0)
        return true;
    struct timeval tv = {0, 0};
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    return select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv) > 0;
}

static int raw_getch(void) {
    if (test_next_byte >= 0) {
        int ch = test_next_byte;
        test_next_byte = -1;
        return ch;
    }
    unsigned char ch;
    return (read(STDIN_FILENO, &ch, 1) == 1) ? (int)ch : -1;
}

#endif

/* ================================================================
 *  Key-event parser
 * ================================================================ */

typedef enum { SM_NORMAL, SM_UTF8, SM_CSI, SM_SS3 } parser_sm_t;

static parser_sm_t sm = SM_NORMAL;

/* ---- UTF-8 accumulation ---- */
static struct {
    char pending[4];
    int expected;
    int received;
} utf8_state;

/* ---- CSI accumulation ---- */
static int csi_params[4];
static int csi_nparams;
static int csi_cur;

/* ---- byte already consumed by ESC detection but not yet processed ---- */
static int pending_byte = -1;

static bool paste_mode = false;

/* ================================================================
 *  ASCII → key-mask lookup
 * ================================================================ */

static __uint128_t ascii_key_mask(int ch) {
    if (ch < 0 || ch > 127)
        return 0;
    static const __uint128_t table[128] = {
        /* 0x00-0x0F */
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        /* 0x10-0x1F */
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        /* 0x20-0x2F  space ! " # $ % & ' ( ) * + , - . / */
        KEY_SPACE,
        KEY_EXCLAM,
        KEY_DQUOTE,
        KEY_HASH,
        KEY_DOLLAR,
        KEY_PERCENT,
        KEY_AMPERSAND,
        KEY_SQUOTE,
        KEY_LPAREN,
        KEY_RPAREN,
        KEY_ASTERISK,
        KEY_PLUS,
        KEY_COMMA,
        KEY_DASH,
        KEY_DOT,
        KEY_SLASH,
        /* 0x30-0x3F  0-9 : ; < = > ? */
        KEY_0,
        KEY_1,
        KEY_2,
        KEY_3,
        KEY_4,
        KEY_5,
        KEY_6,
        KEY_7,
        KEY_8,
        KEY_9,
        KEY_COLON,
        KEY_SEMICOLON,
        KEY_LANGLE,
        KEY_EQUALS,
        KEY_RANGLE,
        KEY_QUESTION,
        /* 0x40-0x4F  @ A-O */
        KEY_AT,
        KEY_A_UPPER,
        KEY_B_UPPER,
        KEY_C_UPPER,
        KEY_D_UPPER,
        KEY_E_UPPER,
        KEY_F_UPPER,
        KEY_G_UPPER,
        KEY_H_UPPER,
        KEY_I_UPPER,
        KEY_J_UPPER,
        KEY_K_UPPER,
        KEY_L_UPPER,
        KEY_M_UPPER,
        KEY_N_UPPER,
        KEY_O_UPPER,
        /* 0x50-0x5F  P-Z [ \ ] ^ _ */
        KEY_P_UPPER,
        KEY_Q_UPPER,
        KEY_R_UPPER,
        KEY_S_UPPER,
        KEY_T_UPPER,
        KEY_U_UPPER,
        KEY_V_UPPER,
        KEY_W_UPPER,
        KEY_X_UPPER,
        KEY_Y_UPPER,
        KEY_Z_UPPER,
        KEY_LBRACKET,
        KEY_BACKSLSH,
        KEY_RBRACKET,
        KEY_CARET,
        KEY_UNDERSCORE,
        /* 0x60-0x6F  ` a-o */
        KEY_BACKTICK,
        KEY_a_LOWER,
        KEY_b_LOWER,
        KEY_c_LOWER,
        KEY_d_LOWER,
        KEY_e_LOWER,
        KEY_f_LOWER,
        KEY_g_LOWER,
        KEY_h_LOWER,
        KEY_i_LOWER,
        KEY_j_LOWER,
        KEY_k_LOWER,
        KEY_l_LOWER,
        KEY_m_LOWER,
        KEY_n_LOWER,
        KEY_o_LOWER,
        /* 0x70-0x7F  p-z { | } ~ DEL */
        KEY_p_LOWER,
        KEY_q_LOWER,
        KEY_r_LOWER,
        KEY_s_LOWER,
        KEY_t_LOWER,
        KEY_u_LOWER,
        KEY_v_LOWER,
        KEY_w_LOWER,
        KEY_x_LOWER,
        KEY_y_LOWER,
        KEY_z_LOWER,
        KEY_LCURLY,
        KEY_PIPE,
        KEY_RCURLY,
        KEY_TILDE,
        0,
    };
    return table[ch];
}

/* ================================================================
 *  CSI helpers
 * ================================================================ */

static void csi_init(void) {
    csi_nparams = 0;
    csi_cur = -1;
}

static void csi_add_digit(int digit) {
    if (csi_cur < 0)
        csi_cur = 0;
    csi_cur = csi_cur * 10 + digit;
}

static void csi_push_param(void) {
    if (csi_nparams < 4) {
        csi_params[csi_nparams++] = (csi_cur >= 0) ? csi_cur : 0;
        csi_cur = -1;
    }
}

static int csi_param(int idx) {
    return (idx < csi_nparams) ? csi_params[idx] : 0;
}

static __uint128_t csi_mod_mask(int code) {
    if (code < 2 || code > 16)
        return 0;
    int bits = code - 1;
    uint64_t hi = 0;
    if (bits & 1)
        hi |= UINT64_C(1) << 53; /* MOD_SHIFT: bit 117 = 64+53 */
    if (bits & 2)
        hi |= UINT64_C(1) << 55; /* MOD_ALT:   bit 119 = 64+55 */
    if (bits & 4)
        hi |= UINT64_C(1) << 54; /* MOD_CTRL:  bit 118 = 64+54 */
    if (bits & 8)
        hi |= UINT64_C(1) << 56; /* MOD_META:  bit 120 = 64+56 */
    return (__uint128_t)hi << 64;
}

static clk_key_event csi_term_event(unsigned char term) {
    clk_key_event ev = {0};
    __uint128_t mod = csi_mod_mask(csi_param(1));
    int p0 = csi_param(0);

    if (term == 'u') {
        __uint128_t m = ascii_key_mask(p0);
        if ((unsigned int)p0 <= 127 && m != 0) {
            if (mod & MOD_SHIFT) {
                mod &= (__uint128_t)~MOD_SHIFT;
            }
        } else if ((unsigned)p0 >= 1 && (unsigned)p0 <= 26) {
            m = ((__uint128_t)1 << ((unsigned)p0 - 1));
        }
        ev.key_mask = m | mod;
        if (m != 0 && !(mod & (MOD_CTRL | MOD_ALT))) {
            ev.text[0] = (char)p0;
            ev.text_len = 1;
            ev.has_text = true;
        }
        return ev;
    }

    switch (term) {
        case 'A':
            ev.key_mask = KEY_UP;
            break;
        case 'B':
            ev.key_mask = KEY_DOWN;
            break;
        case 'C':
            ev.key_mask = KEY_RIGHT;
            break;
        case 'D':
            ev.key_mask = KEY_LEFT;
            break;
        case 'H':
            ev.key_mask = KEY_HOME;
            break;
        case 'F':
            ev.key_mask = KEY_END;
            break;
        case 'Z':
            ev.key_mask = KEY_TAB;
            mod = MOD_SHIFT;
            break;
        case '~':
            switch (p0) {
                case 1:
                    ev.key_mask = KEY_HOME;
                    break;
                case 2:
                    ev.key_mask = KEY_INSERT;
                    break;
                case 3:
                    ev.key_mask = KEY_DEL;
                    break;
                case 4:
                    ev.key_mask = KEY_END;
                    break;
                case 5:
                    ev.key_mask = KEY_PAGE_UP;
                    break;
                case 6:
                    ev.key_mask = KEY_PAGE_DOWN;
                    break;
                case 11:
                    ev.key_mask = KEY_F1;
                    break;
                case 12:
                    ev.key_mask = KEY_F2;
                    break;
                case 13:
                    ev.key_mask = KEY_F3;
                    break;
                case 14:
                    ev.key_mask = KEY_F4;
                    break;
                case 15:
                    ev.key_mask = KEY_F5;
                    break;
                case 17:
                    ev.key_mask = KEY_F6;
                    break;
                case 18:
                    ev.key_mask = KEY_F7;
                    break;
                case 19:
                    ev.key_mask = KEY_F8;
                    break;
                case 200:
                    ev.key_mask = KEY_PASTE_START;
                    break;
                case 201:
                    ev.key_mask = KEY_PASTE_END;
                    break;
                default:
                    break;
            }
            break;
        default:
            break;
    }
    ev.key_mask |= mod;
    return ev;
}

/* ================================================================
 *  Byte processor
 * ================================================================ */

static clk_key_event process_byte(int ch) {
    clk_key_event ev = {0};

    if (ch < 0)
        return ev;

    /* ---- UTF-8 continuation ---- */
    if (sm == SM_UTF8) {
        if ((ch & 0xC0) == 0x80) {
            utf8_state.pending[utf8_state.received++] = (char)ch;
            if (utf8_state.received == utf8_state.expected) {
                memcpy(ev.text, utf8_state.pending, (size_t)utf8_state.received);
                ev.text_len = (uint8_t)utf8_state.received;
                ev.has_text = true;
                sm = SM_NORMAL;
            }
            return ev;
        }
        /* broken sequence — discard */
        sm = SM_NORMAL;
        /* fall through to re-interpret ch */
    }

    /* ---- CSI continuation ---- */
    if (sm == SM_CSI) {
        if (ch >= '0' && ch <= '9') {
            csi_add_digit(ch - '0');
        } else if (ch == ';') {
            csi_push_param();
        } else {
            csi_push_param();
            ev = csi_term_event((unsigned char)ch);
            sm = SM_NORMAL;
        }
        return ev;
    }

    /* ---- SS3 (ESC O)  ---- */
    if (sm == SM_SS3) {
        sm = SM_NORMAL;
        switch (ch) {
            case 'P':
                ev.key_mask = KEY_F1;
                break;
            case 'Q':
                ev.key_mask = KEY_F2;
                break;
            case 'R':
                ev.key_mask = KEY_F3;
                break;
            case 'S':
                ev.key_mask = KEY_F4;
                break;
            default:
                break;
        }
        return ev;
    }

    /* ---- Backspace ---- */
    if (ch == 0x7F || ch == 0x08) {
        ev.key_mask = KEY_BS;
        return ev;
    }
    /* ---- Enter ---- */
    if (ch == 0x0D) {
        ev.key_mask = KEY_ENTER;
        return ev;
    }
    /* ---- Tab ---- */
    if (ch == 0x09) {
        ev.key_mask = KEY_TAB;
        return ev;
    }
    /* ---- Ctrl+letter (0x01-0x1A, excluding already-handled) ---- */
    if (ch >= 0x01 && ch <= 0x1A) {
        ev.key_mask = MOD_CTRL | ((__uint128_t)1 << (unsigned)(ch - 1));
        return ev;
    }

    /* ---- ESC detection ---- */
    if (ch == 0x1B) {
        if (raw_kbhit()) {
            int ch2 = raw_getch();
            if (ch2 == '[') {
                csi_init();
                sm = SM_CSI;
                return ev;
            } else if (ch2 == 'O') {
                sm = SM_SS3;
                return ev;
            } else {
                pending_byte = ch2;
            }
        }
        ev.key_mask = KEY_ESC;
        return ev;
    }

    /* ---- ASCII printable ---- */
    if (ch >= 0x20 && ch <= 0x7E) {
        __uint128_t m = ascii_key_mask(ch);
        ev.key_mask = m;
        ev.text[0] = (char)ch;
        ev.text_len = 1;
        ev.has_text = true;
        return ev;
    }

#if defined(_WIN32) || defined(_WIN64)
    /* ---- Windows extended-key prefix (0xE0 / 0x00) ----
     *
     * On non-ConPTY consoles, _getch() returns extended keys as a
     * two-byte sequence.  Must be handled BEFORE UTF-8 lead-byte
     * detection because 0xE0 is also a valid 3-byte UTF-8 lead
     * on POSIX. */
    if ((ch == 0xE0 || ch == 0x00) && sm == SM_NORMAL) {
        if (raw_kbhit()) {
            int ch2 = raw_getch();
            switch (ch2) {
                case 0x47:
                    ev.key_mask = KEY_HOME;
                    break;
                case 0x48:
                    ev.key_mask = KEY_UP;
                    break;
                case 0x49:
                    ev.key_mask = KEY_PAGE_UP;
                    break;
                case 0x4B:
                    ev.key_mask = KEY_LEFT;
                    break;
                case 0x4D:
                    ev.key_mask = KEY_RIGHT;
                    break;
                case 0x4F:
                    ev.key_mask = KEY_END;
                    break;
                case 0x50:
                    ev.key_mask = KEY_DOWN;
                    break;
                case 0x51:
                    ev.key_mask = KEY_PAGE_DOWN;
                    break;
                case 0x52:
                    ev.key_mask = KEY_INSERT;
                    break;
                case 0x53:
                    ev.key_mask = KEY_DEL;
                    break;
                case 0x3B:
                    ev.key_mask = KEY_F1;
                    break;
                case 0x3C:
                    ev.key_mask = KEY_F2;
                    break;
                case 0x3D:
                    ev.key_mask = KEY_F3;
                    break;
                case 0x3E:
                    ev.key_mask = KEY_F4;
                    break;
                case 0x3F:
                    ev.key_mask = KEY_F5;
                    break;
                case 0x40:
                    ev.key_mask = KEY_F6;
                    break;
                case 0x41:
                    ev.key_mask = KEY_F7;
                    break;
                case 0x42:
                    ev.key_mask = KEY_F8;
                    break;
                case 0x43:
                    ev.key_mask = KEY_F9;
                    break;
                case 0x44:
                    ev.key_mask = KEY_F10;
                    break;
                case 0x57:
                    ev.key_mask = KEY_F11;
                    break;
                case 0x58:
                    ev.key_mask = KEY_F12;
                    break;
                case 0x85:
                    ev.key_mask = KEY_F11;
                    break;
                case 0x86:
                    ev.key_mask = KEY_F12;
                    break;
                case 0x5B:
                case 0x5C:
                    ev.key_mask = MOD_META;
                    break;
                case 0x5D:
                    break;
            }
        }
        return ev;
    }
#endif

    /* ---- UTF-8 lead byte ---- */
    if ((ch & 0xE0) == 0xC0) {
        if (ch >= 0xC2) {
            utf8_state.pending[0] = (char)ch;
            utf8_state.expected = 2;
            utf8_state.received = 1;
            sm = SM_UTF8;
        }
        return ev;
    }
    if ((ch & 0xF0) == 0xE0) {
        utf8_state.pending[0] = (char)ch;
        utf8_state.expected = 3;
        utf8_state.received = 1;
        sm = SM_UTF8;
        return ev;
    }
    if ((ch & 0xF8) == 0xF0 && ch <= 0xF4) {
        utf8_state.pending[0] = (char)ch;
        utf8_state.expected = 4;
        utf8_state.received = 1;
        sm = SM_UTF8;
        return ev;
    }

    return ev;
}

static void filter_and_push(clk_key_event ev) {
    if (ev.key_mask == KEY_PASTE_START)
        paste_mode = true;
    else if (ev.key_mask == KEY_PASTE_END)
        paste_mode = false;
    else if (paste_mode)
        ev.key_mask = 0;

    if (ev.key_mask != 0 || ev.has_text)
        ring_push(ev);
}

/* ================================================================
 *  Background thread
 * ================================================================ */

#define CLK_KEY_POLL_MS 10

static pthread_t io_thread;
static volatile bool io_thread_running;

static void* io_thread_func(void* arg) {
    (void)arg;
    while (io_thread_running) {
        int ch;
        if (pending_byte >= 0) {
            ch = pending_byte;
            pending_byte = -1;
        } else if (!raw_kbhit()) {
            clk_time_sleep_ms(CLK_KEY_POLL_MS);
            continue;
        } else {
            ch = raw_getch();
        }

        clk_key_event ev = process_byte(ch);
        filter_and_push(ev);
    }
    return NULL;
}

/* ================================================================
 *  Lifecycle
 * ================================================================ */

void clk_key_io_init(void) {
    if (io_thread_running)
        return;

    pthread_mutex_init(&ring_mutex, NULL);
    ring_write = 0;
    ring_read = 0;
    memset(ring, 0, sizeof(ring));

    current_mode = MODE_NORMAL;
    input_buf = NULL;
    input_buf_max_bytes = 0;
    input_len = NULL;
    input_pos = NULL;

    sm = SM_NORMAL;
    pending_byte = -1;
    memset(&utf8_state, 0, sizeof(utf8_state));
    memset(csi_params, 0, sizeof(csi_params));
    csi_nparams = 0;
    ra_pos = 0;
    ra_len = 0;

#ifndef _WIN32
    {
        struct termios new_tio;
        tcgetattr(STDIN_FILENO, &old_tio);
        new_tio = old_tio;
        new_tio.c_lflag &= ~(ICANON | ECHO | ISIG);
        new_tio.c_iflag &= ~(ICRNL | IXON);
        tcsetattr(STDIN_FILENO, TCSANOW, &new_tio);
    }
#else
    g_hStdin = GetStdHandle(STD_INPUT_HANDLE);
    if (GetConsoleMode(g_hStdin, &g_old_console_mode)) {
        g_console_mode_saved = true;
        DWORD mode = g_old_console_mode;
        mode &= ~(ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT);
        mode |= ENABLE_EXTENDED_FLAGS | ENABLE_VIRTUAL_TERMINAL_INPUT;
        SetConsoleMode(g_hStdin, mode);
    }
#endif
    paste_mode = false;
    printf("\033[?2004h");
    fflush(stdout);
    printf("\033[>4;2m");
    fflush(stdout);

    io_thread_running = true;
    if (pthread_create(&io_thread, NULL, io_thread_func, NULL) != 0) {
        io_thread_running = false;
#ifndef _WIN32
        tcsetattr(STDIN_FILENO, TCSANOW, &old_tio);
#endif
        pthread_mutex_destroy(&ring_mutex);
        return;
    }
}

void clk_key_io_close(void) {
    if (!io_thread_running)
        return;

    io_thread_running = false;
    pthread_join(io_thread, NULL);

    pthread_mutex_destroy(&ring_mutex);

#ifndef _WIN32
    tcsetattr(STDIN_FILENO, TCSANOW, &old_tio);
#else
    if (g_console_mode_saved)
        SetConsoleMode(g_hStdin, g_old_console_mode);
#endif
    printf("\033[?2004l");
    fflush(stdout);
}

/* ================================================================
 *  Mode control
 * ================================================================ */

void clk_key_io_set_normal(void) {
    current_mode = MODE_NORMAL;
    input_buf = NULL;
    input_buf_max_bytes = 0;
    input_len = NULL;
    input_pos = NULL;
}

void clk_key_io_set_input(char* buf, size_t max_chars, size_t* len, size_t* pos) {
    if (!buf || !max_chars || !len || !pos)
        return;
    input_buf = buf;
    input_buf_max_bytes = max_chars + 1;
    input_len = len;
    input_pos = pos;
    *len = 0;
    *pos = 0;
    buf[0] = '\0';
    current_mode = MODE_INPUT;
}

/* ================================================================
 *  Event retrieval
 * ================================================================ */

clk_key_event clk_normal_get_key_event(void) {
    if (current_mode != MODE_NORMAL)
        return (clk_key_event){0};
    clk_key_event ev = ring_pop();
    ev.has_text = false;
    ev.text_len = 0;
    ev.text[0] = '\0';
    return ev;
}

clk_key_event clk_input_get_key_event(void) {
    if (current_mode != MODE_INPUT)
        return (clk_key_event){0};
    return ring_pop();
}

/* ================================================================
 *  UTF-8 helpers
 * ================================================================ */

static int utf8_byte_len(unsigned char first) {
    if (first < 0x80)
        return 1;
    if (first < 0xC0)
        return 0;
    if (first < 0xE0)
        return 2;
    if (first < 0xF0)
        return 3;
    if (first < 0xF8)
        return 4;
    return 0; /* invalid lead byte */
}

static int char_bytes_at(const char* buf, size_t pos, size_t len) {
    if (pos >= len)
        return 0;
    int n = utf8_byte_len((unsigned char)buf[pos]);
    if (n <= 0)
        n = 1; /* treat stray byte as single byte cell */
    return n;
}

static size_t prev_char_boundary(const char* buf, size_t pos) {
    if (pos == 0)
        return 0;
    size_t p = pos - 1;
    while (p > 0 && ((unsigned char)buf[p] & 0xC0) == 0x80)
        p--;
    return p;
}

static size_t next_char_boundary(const char* buf, size_t pos, size_t len) {
    if (pos >= len)
        return len;
    return pos + char_bytes_at(buf, pos, len);
}

/* ================================================================
 *  Internal helpers
 * ================================================================ */

static bool input_active(void) {
    return current_mode == MODE_INPUT && input_buf != NULL;
}

/* ================================================================
 *  Text editing primitives
 * ================================================================ */
static bool write_at_cursor(const char* text, size_t byte_len) {
    if (!input_active())
        return false;

    size_t space = input_buf_max_bytes - *input_len - 1;
    size_t to_write = 0;
    size_t text_pos = 0;

    while (text_pos < byte_len) {
        int ch_bytes = utf8_byte_len((unsigned char)text[text_pos]);
        if (ch_bytes <= 0) {
            text_pos++;
            continue;
        }
        if (text_pos + ch_bytes > byte_len)
            break;
        if (to_write + ch_bytes > space)
            break;

        to_write += ch_bytes;
        text_pos += ch_bytes;
    }

    if (to_write == 0)
        return false;

    if (*input_pos < *input_len)
        memmove(input_buf + *input_pos + to_write, input_buf + *input_pos, *input_len - *input_pos);

    memcpy(input_buf + *input_pos, text, to_write);
    *input_len += to_write;
    *input_pos += to_write;
    input_buf[*input_len] = '\0';
    return text_pos >= byte_len;
}

static bool overwrite_at_cursor(const char* text, size_t byte_len) {
    if (!input_active())
        return false;

    bool truncated = false;
    size_t text_pos = 0;
    while (text_pos < byte_len) {
        int in_bytes = utf8_byte_len((unsigned char)text[text_pos]);
        if (in_bytes <= 0) {
            text_pos++;
            continue;
        }
        if (text_pos + in_bytes > byte_len) {
            truncated = true;
            break;
        }

        if (*input_pos >= *input_len) {
            if (!write_at_cursor(text + text_pos, byte_len - text_pos))
                truncated = true;
            return !truncated;
        }

        int cur_bytes = char_bytes_at(input_buf, *input_pos, *input_len);
        if (*input_len - cur_bytes + in_bytes >= input_buf_max_bytes) {
            truncated = true;
            break;
        }

        clk_input_delete_after();
        write_at_cursor(text + text_pos, in_bytes);
        text_pos += in_bytes;
    }
    return !truncated;
}

bool clk_input_write(clk_write_mode_t mode, const char* text, size_t byte_len) {
    switch (mode) {
        case CLK_WRITE_INSERT:
            return write_at_cursor(text, byte_len);
        case CLK_WRITE_OVERWRITE:
            return overwrite_at_cursor(text, byte_len);
    }
    return false;
}

void clk_input_move_cursor(int offset) {
    if (!input_active())
        return;

    if (offset > 0) {
        for (int i = 0; i < offset; ++i)
            *input_pos = next_char_boundary(input_buf, *input_pos, *input_len);
    } else {
        for (int i = 0; i < -offset; ++i)
            *input_pos = prev_char_boundary(input_buf, *input_pos);
    }
}

bool clk_input_delete_before(void) {
    if (!input_active() || *input_pos == 0)
        return false;

    size_t prev = prev_char_boundary(input_buf, *input_pos);
    size_t del_bytes = *input_pos - prev;
    if (*input_len > *input_pos)
        memmove(input_buf + prev, input_buf + *input_pos, *input_len - *input_pos);
    *input_len -= del_bytes;
    *input_pos = prev;
    input_buf[*input_len] = '\0';
    return true;
}

bool clk_input_delete_after(void) {
    if (!input_active() || *input_pos >= *input_len)
        return false;

    int del_bytes = char_bytes_at(input_buf, *input_pos, *input_len);
    if (*input_len > *input_pos + del_bytes)
        memmove(input_buf + *input_pos, input_buf + *input_pos + del_bytes,
                *input_len - *input_pos - del_bytes);
    *input_len -= del_bytes;
    input_buf[*input_len] = '\0';
    return true;
}

/* ================================================================
 *  Utility
 * ================================================================ */

bool clk_key_is(clk_key_event ev, __uint128_t key, __uint128_t mods) {
    return ev.key_mask == (key | mods);
}

/* ================================================================
 *  Test hook
 * ================================================================ */

void clk_key_io_test_inject(clk_key_event ev) {
    ring_push(ev);
}

void clk_key_io_test_inject_raw(int ch) {
    clk_key_event ev = process_byte(ch);
    filter_and_push(ev);
}

void clk_key_io_test_queue_byte(int ch) {
    test_next_byte = ch;
}

void clk_key_io_test_reset(void) {
    test_next_byte = -1;
    /* drain ring buffer — discard any stray events */
    pthread_mutex_lock(&ring_mutex);
    ring_read = ring_write;
    memset(ring, 0, sizeof(ring));
    pthread_mutex_unlock(&ring_mutex);
    sm = SM_NORMAL;
    memset(&utf8_state, 0, sizeof(utf8_state));
    memset(csi_params, 0, sizeof(csi_params));
    csi_nparams = 0;
    pending_byte = -1;
    paste_mode = false;
    ra_pos = 0;
    ra_len = 0;
}

void clk_key_io_test_pause(void) {
    if (!io_thread_running)
        return;
    io_thread_running = false;
    pthread_join(io_thread, NULL);
}

void clk_key_io_test_resume(void) {
    if (io_thread_running)
        return;
    io_thread_running = true;
    pthread_create(&io_thread, NULL, io_thread_func, NULL);
}
