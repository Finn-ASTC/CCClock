#include "clk_key_io.h"

#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

/* ================================================================
 *  Ring buffer — thread-safe (protected by key_mutex)
 * ================================================================ */

#define KEY_BUF_SIZE 256

static clk_key_event key_ring[KEY_BUF_SIZE];
static int key_write;
static int key_read;
static pthread_mutex_t key_mutex;

static void ring_push(clk_key_event ev) {
    pthread_mutex_lock(&key_mutex);
    key_ring[key_write] = ev;
    key_write = (key_write + 1) % KEY_BUF_SIZE;
    if (key_write == key_read) /* full — discard oldest */
        key_read = (key_read + 1) % KEY_BUF_SIZE;
    pthread_mutex_unlock(&key_mutex);
}

static clk_key_event ring_pop(void) {
    clk_key_event ev = {CLK_KEY_NONE, 0, {0, 0}};
    pthread_mutex_lock(&key_mutex);
    if (key_read != key_write) {
        ev = key_ring[key_read];
        key_read = (key_read + 1) % KEY_BUF_SIZE;
    }
    pthread_mutex_unlock(&key_mutex);
    return ev;
}

/* ================================================================
 *  Thread state
 * ================================================================ */

static pthread_t key_thread;
static volatile bool key_thread_running;
static bool key_thread_started;

/* ================================================================
 *  Platform-specific raw I/O
 * ================================================================ */

#if defined(_WIN32) || defined(_WIN64)

#include <conio.h>
#include <windows.h>

static bool raw_kbhit(void) {
    return _kbhit() != 0;
}

static int raw_getch(void) {
    return _getch();
}

static void raw_sleep_ms(int ms) {
    Sleep(ms);
}

static clk_key_event raw_read_key(void) {
    clk_key_event res = {CLK_KEY_NONE, 0, {0, 0}};

    int ch = raw_getch();
    res.raw = (uint32_t)ch;

    if (ch == 27) {
        res.key = CLK_KEY_ESC;
        return res;
    }

    /* extended key prefix (arrow keys etc.) */
    if (ch == 0 || ch == 224) {
        int ch2 = raw_getch();
        res.raw = (uint32_t)ch2;
        switch (ch2) {
            case 72:
                res.key = CLK_KEY_UP;
                break;
            case 80:
                res.key = CLK_KEY_DOWN;
                break;
            case 75:
                res.key = CLK_KEY_LEFT;
                break;
            case 77:
                res.key = CLK_KEY_RIGHT;
                break;
            default:
                break;
        }
        return res;
    }

    res.key = (uint32_t)ch;
    return res;
}

#else /* POSIX */

#include <sys/select.h>
#include <termios.h>
#include <unistd.h>

static struct termios old_tio;

static bool raw_kbhit(void) {
    struct timeval tv = {0, 0};
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    return select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv) > 0;
}

static int raw_getch(void) {
    unsigned char ch;
    return (read(STDIN_FILENO, &ch, 1) == 1) ? (int)ch : -1;
}

static void raw_sleep_ms(int ms) {
    usleep(ms * 1000);
}

static clk_key_event raw_read_key(void) {
    clk_key_event res = {CLK_KEY_NONE, 0, {0, 0}};

    int ch = raw_getch();
    res.raw = (uint32_t)ch;

    if (ch == 27) {
        /* might be ESC or start of an escape sequence */
        if (raw_kbhit()) {
            int ch2 = raw_getch();
            res.raw = (res.raw << 16) | (uint32_t)ch2;

            if (ch2 == '[' || ch2 == 'O') {
                if (raw_kbhit()) {
                    int ch3 = raw_getch();
                    res.raw = (res.raw << 8) | (uint32_t)ch3;
                    switch (ch3) {
                        case 'A':
                            res.key = CLK_KEY_UP;
                            break;
                        case 'B':
                            res.key = CLK_KEY_DOWN;
                            break;
                        case 'C':
                            res.key = CLK_KEY_RIGHT;
                            break;
                        case 'D':
                            res.key = CLK_KEY_LEFT;
                            break;
                        default:
                            break;
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

/* ================================================================
 *  Background thread
 * ================================================================ */

static void* key_thread_func(void* arg) {
    (void)arg;

    while (key_thread_running) {
        if (!raw_kbhit()) {
            raw_sleep_ms(10);
            continue;
        }

        clk_key_event ev = raw_read_key();
        if (ev.key != CLK_KEY_NONE)
            ring_push(ev);
    }

    return NULL;
}

/* ================================================================
 *  Lifecycle
 * ================================================================ */

void clk_key_io_init(void) {
    if (key_thread_started)
        return;

    pthread_mutex_init(&key_mutex, NULL);
    key_write = 0;
    key_read = 0;

#ifndef _WIN32
    struct termios new_tio;
    tcgetattr(STDIN_FILENO, &old_tio);
    new_tio = old_tio;
    new_tio.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &new_tio);
#endif

    key_thread_running = true;
    pthread_create(&key_thread, NULL, key_thread_func, NULL);
    key_thread_started = true;
}

void clk_key_io_close(void) {
    if (!key_thread_started)
        return;

    key_thread_running = false;
    pthread_join(key_thread, NULL);

    pthread_mutex_destroy(&key_mutex);

#ifndef _WIN32
    tcsetattr(STDIN_FILENO, TCSANOW, &old_tio);
#endif

    key_thread_started = false;
}

/* ================================================================
 *  Input mode (string-input vs normal)
 * ================================================================ */

static enum { KEY_MODE_NORMAL, KEY_MODE_STRING_INPUT } key_mode;
static char* key_input_buf;
static size_t key_input_buf_size;
static size_t *key_input_len, *key_input_pos;

/* ================================================================
 *  Public API
 * ================================================================ */

clk_key_event clk_get_key_event(void) {
    if (key_mode == KEY_MODE_STRING_INPUT)
        return (clk_key_event){CLK_KEY_NONE, 0, {0, 0}};
    return ring_pop();
}

/* --- string input --- */

void clk_key_io_text_start(char* buf, size_t buf_size, size_t* len, size_t* pos) {
    if (!buf || !buf_size || !len || !pos)
        return;
    key_input_buf = buf;
    key_input_buf_size = buf_size;
    key_input_len = len;
    key_input_pos = pos;
    key_mode = KEY_MODE_STRING_INPUT;
    *len = 0;
    *pos = 0;
    buf[0] = '\0';
}

void clk_key_io_text_stop(void) {
    key_mode = KEY_MODE_NORMAL;
}

uint32_t clk_key_io_text_poll(void) {
    if (key_mode != KEY_MODE_STRING_INPUT)
        return CLK_KEY_NONE;

    clk_key_event ev = ring_pop();
    uint32_t key = ev.key;
    if (key == CLK_KEY_NONE)
        return CLK_KEY_NONE;

    if (key == '\r' || key == '\n') {
        key_input_buf[*key_input_len] = '\0';
        clk_key_io_text_stop();
        return '\r';
    }
    if (key == CLK_KEY_ESC) {
        key_input_buf[0] = '\0';
        *key_input_len = 0;
        *key_input_pos = 0;
        clk_key_io_text_stop();
        return CLK_KEY_ESC;
    }

    if (key == CLK_KEY_LEFT) {
        if (*key_input_pos > 0)
            (*key_input_pos)--;
        return CLK_KEY_NONE;
    }
    if (key == CLK_KEY_RIGHT) {
        if (*key_input_pos < *key_input_len)
            (*key_input_pos)++;
        return CLK_KEY_NONE;
    }

    /* backspace — delete character before cursor */
    if (key == '\b' || key == 127) {
        if (*key_input_pos > 0 && *key_input_len > 0) {
            size_t p = *key_input_pos;
            memmove(key_input_buf + p - 1, key_input_buf + p, *key_input_len - p);
            (*key_input_pos)--;
            (*key_input_len)--;
        }
        return CLK_KEY_NONE;
    }

    /* printable ASCII */
    if (key >= 32 && key < 127) {
        if (*key_input_len + 1 < key_input_buf_size) {
            if (*key_input_pos < *key_input_len)
                memmove(key_input_buf + *key_input_pos + 1, key_input_buf + *key_input_pos,
                        *key_input_len - *key_input_pos);
            key_input_buf[*key_input_pos] = (char)key;
            (*key_input_pos)++;
            (*key_input_len)++;
        }
        return CLK_KEY_NONE;
    }

    return CLK_KEY_NONE;
}

/* --- test hook (not part of public API) --- */

void clk_key_io_test_inject(uint32_t key) {
    clk_key_event ev = {key, 0, {0, 0}};
    ring_push(ev);
}
