#ifndef CLK_FS_WATCH_H
#define CLK_FS_WATCH_H

#include <stdbool.h>
#include <time.h>

#include "clk_menu.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Scan a directory for files matching the given extension.
 *  @p extension includes the leading dot, e.g. ".json" ".mp3".
 *  Returns full paths; caller frees via clk_fs_free_list.
 *  Returns NULL on error — *out_count is set to 0. */
char** clk_fs_scan_dir(const char* dir_path, const char* extension, int* out_count);

/** Free the result of clk_fs_scan_dir. */
void clk_fs_free_list(char** list, int count);

/** Detect file modification via mtime comparison.
 *  Returns true when stale or on first call (*last_mtime == 0). */
bool clk_fs_file_changed(const char* path, time_t* last_mtime);

/** Re-scan a directory and rebuild a menu item if changed.
 *  Frees old data on change; replaces *paths, *count, *display_names, *index. */
void clk_fs_sync_dir(const char* dir_path, char*** paths, int* count, char*** display_names,
                     int* index, clk_menu* menu, int tab_id, int item_id);

#ifdef __cplusplus
}
#endif

#endif
