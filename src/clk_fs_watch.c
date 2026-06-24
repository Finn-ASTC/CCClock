#include "clk_fs_watch.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#else
#include <dirent.h>
#endif

/* ================================================================
 *  Directory scanner
 * ================================================================ */

#if defined(_WIN32) || defined(_WIN64)
char** clk_fs_scan_dir(const char* dir_path, const char* extension, int* out_count) {
    *out_count = 0;
    if (!dir_path || !extension)
        return NULL;

    char pattern[1024];
    snprintf(pattern, sizeof(pattern), "%s\\*%s", dir_path, extension);

    WIN32_FIND_DATA find_data;
    HANDLE find_handle = FindFirstFile(pattern, &find_data);
    if (find_handle == INVALID_HANDLE_VALUE)
        return NULL;

    char** list = NULL;
    int count = 0, capacity = 0;
    size_t ext_len = strlen(extension);
    do {
        if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            continue;

        size_t name_len = strlen(find_data.cFileName);
        if (name_len <= ext_len || strcmp(find_data.cFileName + name_len - ext_len, extension) != 0)
            continue;

        if (count >= capacity) {
            capacity = capacity ? capacity * 2 : 8;
            char** tmp = realloc(list, capacity * sizeof(char*));
            if (!tmp) {
                free(list);
                FindClose(find_handle);
                return NULL;
            }
            list = tmp;
        }

        size_t path_len = strlen(dir_path) + 1 + name_len + 1;
        list[count] = malloc(path_len);
        snprintf(list[count], path_len, "%s\\%s", dir_path, find_data.cFileName);
        count++;
    } while (FindNextFile(find_handle, &find_data));
    FindClose(find_handle);

    *out_count = count;
    return list;
}
#else
char** clk_fs_scan_dir(const char* dir_path, const char* extension, int* out_count) {
    *out_count = 0;
    if (!dir_path || !extension)
        return NULL;

    DIR* dir = opendir(dir_path);
    if (!dir)
        return NULL;

    char** list = NULL;
    int count = 0, capacity = 0;
    size_t ext_len = strlen(extension);
    struct dirent* entry;

    while ((entry = readdir(dir)) != NULL) {
        size_t name_len = strlen(entry->d_name);
        if (name_len <= ext_len || strcmp(entry->d_name + name_len - ext_len, extension) != 0)
            continue;

        if (count >= capacity) {
            capacity = capacity ? capacity * 2 : 8;
            char** tmp = realloc(list, capacity * sizeof(char*));
            if (!tmp) {
                free(list);
                closedir(dir);
                return NULL;
            }
            list = tmp;
        }

        size_t path_len = strlen(dir_path) + 1 + name_len + 1;
        list[count] = malloc(path_len);
        snprintf(list[count], path_len, "%s/%s", dir_path, entry->d_name);
        count++;
    }
    closedir(dir);

    *out_count = count;
    return list;
}
#endif

void clk_fs_free_list(char** list, int count) {
    if (!list)
        return;
    for (int i = 0; i < count; ++i)
        free(list[i]);
    free(list);
}

/* ================================================================
 *  File mtime watcher
 * ================================================================ */

bool clk_fs_file_changed(const char* path, time_t* last_mtime) {
    if (!path || !last_mtime)
        return false;

    struct stat stat_buf;
    if (stat(path, &stat_buf) != 0)
        return false;

    if (stat_buf.st_mtime != *last_mtime) {
        *last_mtime = stat_buf.st_mtime;
        return true;
    }
    return false;
}
