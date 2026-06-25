#ifndef CLK_CLOCK_H
#define CLK_CLOCK_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CLK_CLOCK_FORMAT_MAX_LENGTH 64

/* ================================================================
 *  Time format translation
 *
 *  User-friendly tokens → strftime format strings.
 *  yyyy→%Y yy→%y MM→%M dd→%d hh→%H mm→%m ss→%S
 *  Other characters pass through literally.
 * ================================================================ */

bool clk_clock_translate_format(const char* user_format, char* strftime_format,
                                size_t strftime_format_size);

/* ================================================================
 *  Time formatting
 * ================================================================ */

/** Format the current local time into the supplied buffer using a
 *  strftime-style format string.  Returns false if localtime fails
 *  or the buffer is too small. */
bool clk_clock_format_now(const char* strftime_format, char* buffer, size_t buffer_size);

#ifdef __cplusplus
}
#endif

#endif
