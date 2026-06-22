#ifndef CLK_FILE_UTIL_H
#define CLK_FILE_UTIL_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Read an entire file into a malloc'd, NUL-terminated buffer.
 *  Returns NULL on any error (missing file, read failure, OOM).
 *  Caller must free the returned pointer. */
char* clk_file_read_all(const char* path, size_t* out_size);

#ifdef __cplusplus
}
#endif

#endif
