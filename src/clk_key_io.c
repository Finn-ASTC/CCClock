#include "clk_key_io.h"

#include <stdint.h>

/* ================================================================
 *  Shared State
 * ================================================================ */

static bool clk_key_io_is_init = false;

#if defined(_WIN32) || defined(_WIN64)

/* ================================================================
 *  Windows Implementation
 * ================================================================ */

#include <conio.h>
#include <stdbool.h>

/** Initialize the keyboard input subsystem. */
void clk_key_io_init(void) {
    if (!clk_key_io_is_init)
        clk_key_io_is_init = true;
}

/** Shut down the keyboard input subsystem. */
void clk_key_io_close(void) {
    if (clk_key_io_is_init)
        clk_key_io_is_init = false;
}

/** Poll for and decode a single key event. */
clk_key_event clk_get_key_event(void) {
    clk_key_event res = {CLK_KEY_NONE, CLK_KEY_NONE, {0, 0}};
    if (!_kbhit() || !clk_key_io_is_init)
        return res;

    int ch = _getch();
    res.raw = (uint32_t)ch;

    if (ch == 27) {
        res.key = CLK_KEY_ESC;
        return res;
    }

    /* extended key prefix (arrow keys etc.) */
    if (ch == 0 || ch == 224) {
        int ch2 = _getch();
        res.raw = (uint32_t)ch2;
        switch (ch2) {
            case 72: res.key = CLK_KEY_UP;    break;
            case 80: res.key = CLK_KEY_DOWN;  break;
            case 75: res.key = CLK_KEY_LEFT;  break;
            case 77: res.key = CLK_KEY_RIGHT; break;
            default: break;
        }
        return res;
    }

    res.key = (uint32_t)ch;
    return res;
}

#else  /* Linux / macOS / Unix */

/* ================================================================
 *  POSIX Implementation
 * ================================================================ */

#include <sys/select.h>
#include <termios.h>
#include <unistd.h>

static struct termios old_tio;

/** Return non-zero if a key press is waiting to be read. */
static int linux_kbhit(void) {
    struct timeval tv = {0, 0};
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    return select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv) > 0;
}

/** Read a single raw byte from standard input. */
static int linux_getch(void) {
    unsigned char ch;
    return (read(STDIN_FILENO, &ch, 1) == 1) ? (int)ch : CLK_KEY_NONE;
}

/** Initialize the keyboard input subsystem. */
void clk_key_io_init(void) {
    struct termios new_tio;
    tcgetattr(STDIN_FILENO, &old_tio);
    new_tio = old_tio;
    new_tio.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &new_tio);
    clk_key_io_is_init = true;
}

/** Shut down the keyboard input subsystem. */
void clk_key_io_close(void) {
    if (clk_key_io_is_init) {
        tcsetattr(STDIN_FILENO, TCSANOW, &old_tio);
        clk_key_io_is_init = false;
    }
}

/** Poll for and decode a single key event. */
clk_key_event clk_get_key_event(void) {
    clk_key_event res = {CLK_KEY_NONE, 0, {0, 0}};
    if (!linux_kbhit() || !clk_key_io_is_init)
        return res;

    int ch = linux_getch();
    res.raw = (uint32_t)ch;

    if (ch == 27) {
        /* might be ESC or start of an escape sequence */
        if (linux_kbhit()) {
            int ch2 = linux_getch();
            res.raw = (res.raw << 16) | (uint32_t)ch2;

            if (ch2 == '[' || ch2 == 'O') {
                if (linux_kbhit()) {
                    int ch3 = linux_getch();
                    res.raw = (res.raw << 8) | (uint32_t)ch3;
                    switch (ch3) {
                        case 'A': res.key = CLK_KEY_UP;    break;
                        case 'B': res.key = CLK_KEY_DOWN;  break;
                        case 'C': res.key = CLK_KEY_RIGHT; break;
                        case 'D': res.key = CLK_KEY_LEFT;  break;
                        default:  break;
                    }
                }
            }
        } else {
            res.key = CLK_KEY_ESC;
        }
        return res;
    }

    res.key = (uint32_t)ch;
    return res;
}

#endif
