#include "clk_clock.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/**
 * Translate a user-friendly format string into strftime format.
 *   yyyy → %Y    yy → %y
 *   MM   → %m    dd → %d
 *   hh   → %H    mm → %M    ss → %S
 * Characters not listed above are passed through literally
 * (spaces, colons, newlines, etc.).
 */
static bool translate_time_format(const char* fmt, char* out, size_t out_size) {
    int i = 0, o = 0;
    int end = (int)strlen(fmt);
    int out_end = (int)out_size - 1;

    while (i < end && o < out_end) {
        char c = fmt[i];

        /* ----- year (yyyy / yy) ----- */
        if (c == 'y') {
            int n = 0;
            while (i + n < end && fmt[i + n] == 'y')
                n++;
            if (n == 4) {
                out[o++] = '%';
                out[o++] = 'Y';
                i += 4;
            } else if (n == 2) {
                out[o++] = '%';
                out[o++] = 'y';
                i += 2;
            } else
                return false;
            continue;
        }

        /* ----- month (MM) ----- */
        if (c == 'M') {
            if (i + 1 < end && fmt[i + 1] == 'M') {
                out[o++] = '%';
                out[o++] = 'm';
                i += 2;
                continue;
            }
            return false;
        }

        /* ----- day (dd) ----- */
        if (c == 'd') {
            if (i + 1 < end && fmt[i + 1] == 'd') {
                out[o++] = '%';
                out[o++] = 'd';
                i += 2;
                continue;
            }
            return false;
        }

        /* ----- hour (hh) ----- */
        if (c == 'h') {
            if (i + 1 < end && fmt[i + 1] == 'h') {
                out[o++] = '%';
                out[o++] = 'H';
                i += 2;
                continue;
            }
            return false;
        }

        /* ----- minute (mm) ----- */
        if (c == 'm') {
            if (i + 1 < end && fmt[i + 1] == 'm') {
                out[o++] = '%';
                out[o++] = 'M';
                i += 2;
                continue;
            }
            return false;
        }

        /* ----- second (ss) ----- */
        if (c == 's') {
            if (i + 1 < end && fmt[i + 1] == 's') {
                out[o++] = '%';
                out[o++] = 'S';
                i += 2;
                continue;
            }
            return false;
        }

        /* literal character */
        out[o++] = fmt[i++];
    }

    out[o] = '\0';
    return true;
}

static bool clk_clock_format_current_time(const char* time_format, char* buffer,
                                          size_t buffer_size) {
    if (!time_format || !buffer || buffer_size == 0)
        return false;

    /* translate "yyyy:MM:dd hh:mm:ss" → "%Y:%m:%d %H:%M:%S" */
    char translated[128];
    if (!translate_time_format(time_format, translated, sizeof(translated)))
        return false;

    time_t rawtime;
    struct tm timeinfo;
    time(&rawtime);
#if defined(_WIN32) || defined(_WIN64)
    if (localtime_s(&timeinfo, &rawtime) != 0)
        return false;
#else
    if (!localtime_r(&rawtime, &timeinfo))
        return false;
#endif

    return strftime(buffer, buffer_size, translated, &timeinfo) > 0;
}

bool clk_clock_create(clk_clock* clk, const char* time_format, const char* font_path) {
    if (!clk || !time_format || !font_path)
        return false;

    memset(clk, 0, sizeof(*clk));

    size_t fmt_len = strlen(time_format);
    if (fmt_len >= CLK_CLOCK_TIME_FORMAT_MAX_LENGTH)
        return false;
    memcpy(clk->clk_clock_time_format, time_format, fmt_len + 1);

    clk->clk_clock_font_path = strdup(font_path);
    if (!clk->clk_clock_font_path) {
        clk_clock_destroy(clk);
        return false;
    }

    return true;
}

void clk_clock_destroy(clk_clock* clk) {
    if (!clk)
        return;
    free(clk->clk_clock_font_path);
    clk->clk_clock_font_path = NULL;
    if (clk->clk_clock_texture) {
        clk_texture_destroy(clk->clk_clock_texture);
        free(clk->clk_clock_texture);
        clk->clk_clock_texture = NULL;
    }
}

bool clk_clock_change_time_format(clk_clock* clk, const char* new_time_format) {
    (void)clk;
    (void)new_time_format;
    return false;
}

bool clk_clock_change_font_path(clk_clock* clk, const char* new_font_path) {
    (void)clk;
    (void)new_font_path;
    return false;
}

void clk_clock_update(clk_clock* clk) {
    (void)clk;
}

bool clk_clock_get_texture_size(const clk_clock* clk, int* width, int* height) {
    (void)clk;
    (void)width;
    (void)height;
    return false;
}

bool clk_clock_get_texture_pos(const clk_clock* clk, int* posx, int* posy) {
    (void)clk;
    (void)posx;
    (void)posy;
    return false;
}

bool clk_clock_set_texture_pos(clk_clock* clk, int posx, int posy) {
    (void)clk;
    (void)posx;
    (void)posy;
    return false;
}

const clk_texture* clk_clock_get_texture(const clk_clock* clk) {
    (void)clk;
    return NULL;
}
