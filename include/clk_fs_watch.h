#ifndef CLK_FS_WATCH_H
#define CLK_FS_WATCH_H

#include <stdbool.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Scan a directory for files matching the given extension.
 *
 * @p extension must include the leading dot, e.g. ".json" ".mp3".
 * Returns an array of full paths (dir_path + "/" + filename).
 * Ownership: caller must free each string and the array (or call
 * clk_fs_free_list).
 *
 * Returns NULL on error or when no matches exist.  *out_count is
 * set to 0 on NULL return (error or empty directory).
 */
char** clk_fs_scan_dir(const char* dir_path, const char* extension, int* out_count);

/**
 * Free the result of clk_fs_scan_dir.
 */
void clk_fs_free_list(char** list, int count);

/**
 * Check whether @p path has been modified since the last call.
 *
 * On the first call (or after the file is (re)created) *last_mtime
 * should be 0 so that the initial stat populates it and returns true.
 *
 * Returns true when the file's mtime differs from *last_mtime,
 * and updates *last_mtime to the new value.  Returns false if the
 * file does not exist or its mtime is unchanged.
 */
bool clk_fs_file_changed(const char* path, time_t* last_mtime);

#ifdef __cplusplus
}
#endif

#endif
