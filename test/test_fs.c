#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "clk_file_util.h"
#include "clk_fs_watch.h"
#include "test_utils.h"

int main(void) {
    if (isatty_fd(fileno(stdout))) {
        g_pass = "  \033[32m[PASS]\033[0m ";
        g_fail = "  \033[31m[FAIL]\033[0m ";
    }

    /* ================================================================
     *  clk_file_util
     * ================================================================ */

    /* --- read existing file --- */
    size_t size = 0;
    char* content = clk_file_read_all("../../test/test_clock_config.json", &size);
    TEST("read_all: non-null", content != NULL);
    TEST("read_all: size > 0", size > 0);
    TEST("read_all: content[0] == '{'", content && content[0] == '{');
    free(content);

    /* --- read non-existent --- */
    content = clk_file_read_all("nonexistent_xyz.abc", NULL);
    TEST("read_all: missing → NULL", content == NULL);

    /* --- read NULL path --- */
    content = clk_file_read_all(NULL, NULL);
    TEST("read_all: NULL path → NULL", content == NULL);

    /* ================================================================
     *  clk_fs_watch — scan_dir
     * ================================================================ */

    /* --- scan existing dir --- */
    int count = 0;
    char** entries = clk_fs_scan_dir("../../assets/config/ascii_fonts", ".json", &count);
    TEST("scan_dir: non-null", entries != NULL);
    TEST("scan_dir: count > 0", count > 0);
    clk_fs_free_list(entries, count);

    /* --- scan missing dir --- */
    entries = clk_fs_scan_dir("nonexistent_dir_xyz", ".json", &count);
    TEST("scan_dir: missing → NULL", entries == NULL);
    TEST("scan_dir: missing count=0", count == 0);

    /* --- scan NULL dir --- */
    entries = clk_fs_scan_dir(NULL, ".json", &count);
    TEST("scan_dir: NULL dir → NULL", entries == NULL);

    /* --- scan NULL extension --- */
    entries = clk_fs_scan_dir("../../assets/config/ascii_fonts", NULL, &count);
    TEST("scan_dir: NULL ext → NULL", entries == NULL);

    /* --- free_list NULL --- */
    clk_fs_free_list(NULL, 0);
    TEST("free_list: NULL safe", 1);
    clk_fs_free_list(NULL, 5);
    TEST("free_list: NULL + count safe", 1);

    /* ================================================================
     *  clk_fs_watch — file_changed
     * ================================================================ */

    /* --- file_changed: first call detects change --- */
    time_t last = 0;
    bool changed = clk_fs_file_changed("../../test/test_clock_config.json", &last);
    TEST("file_changed: first call → true", changed);
    TEST("file_changed: last updated", last > 0);

    /* --- file_changed: same mtime → false --- */
    changed = clk_fs_file_changed("../../test/test_clock_config.json", &last);
    TEST("file_changed: same mtime → false", !changed);

    /* --- file_changed: missing file → false --- */
    last = 0;
    changed = clk_fs_file_changed("nonexistent_xyz", &last);
    TEST("file_changed: missing → false", !changed);

    /* --- file_changed: NULL path → false --- */
    changed = clk_fs_file_changed(NULL, &last);
    TEST("file_changed: NULL path → false", !changed);

    /* --- file_changed: NULL last_mtime → false --- */
    changed = clk_fs_file_changed("../../test/test_clock_config.json", NULL);
    TEST("file_changed: NULL mtime → false", !changed);

    printf("\n%d/%d passed\n", test_total - test_failed, test_total);
    return test_failed > 0 ? 1 : 0;
}
