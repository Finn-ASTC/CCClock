#include "clk_clock.h"

#include <string.h>
#include <time.h>

/* ================================================================
 *  Time format translation
 * ================================================================ */

bool clk_clock_translate_format(const char* user_format, char* strftime_format,
                                size_t strftime_format_size) {
    if (!user_format || !strftime_format || strftime_format_size == 0)
        return false;

    int i = 0, o = 0;
    int end = (int)strlen(user_format);
    int out_end = (int)strftime_format_size - 1;

    while (i < end && o < out_end) {
        char c = user_format[i];

        switch (c) {
            case 'y': {
                int n = 0;
                while (i + n < end && user_format[i + n] == 'y')
                    n++;
                if (n == 4) {
                    strftime_format[o++] = '%';
                    strftime_format[o++] = 'Y';
                    i += 4;
                } else if (n == 2) {
                    strftime_format[o++] = '%';
                    strftime_format[o++] = 'y';
                    i += 2;
                } else
                    return false;
                continue;
            }
            case 'M':
                if (i + 1 >= end || user_format[i + 1] != 'M')
                    return false;
                strftime_format[o++] = '%';
                strftime_format[o++] = 'M';
                i += 2;
                continue;
            case 'd':
                if (i + 1 >= end || user_format[i + 1] != 'd')
                    return false;
                strftime_format[o++] = '%';
                strftime_format[o++] = 'd';
                i += 2;
                continue;
            case 'h':
                if (i + 1 >= end || user_format[i + 1] != 'h')
                    return false;
                strftime_format[o++] = '%';
                strftime_format[o++] = 'H';
                i += 2;
                continue;
            case 'm':
                if (i + 1 >= end || user_format[i + 1] != 'm')
                    return false;
                strftime_format[o++] = '%';
                strftime_format[o++] = 'm';
                i += 2;
                continue;
            case 's':
                if (i + 1 >= end || user_format[i + 1] != 's')
                    return false;
                strftime_format[o++] = '%';
                strftime_format[o++] = 'S';
                i += 2;
                continue;
            default:
                strftime_format[o++] = user_format[i++];
                break;
        }
    }

    strftime_format[o] = '\0';
    return true;
}

/* ================================================================
 *  Time formatting
 * ================================================================ */

bool clk_clock_format_now(const char* strftime_format, char* buffer, size_t buffer_size) {
    if (!strftime_format || !buffer || buffer_size == 0)
        return false;

    time_t raw_time;
    struct tm time_info;

    time(&raw_time);

#if defined(_WIN32) || defined(_WIN64)
    if (localtime_s(&time_info, &raw_time) != 0)
        return false;
#else
    if (!localtime_r(&raw_time, &time_info))
        return false;
#endif

    return strftime(buffer, buffer_size, strftime_format, &time_info) > 0;
}
