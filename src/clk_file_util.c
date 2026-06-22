#include "clk_file_util.h"

#include <stdio.h>
#include <stdlib.h>

char* clk_file_read_all(const char* path, size_t* out_size) {
    if (out_size)
        *out_size = 0;

    FILE* file = fopen(path, "rb");
    if (!file)
        return NULL;

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return NULL;
    }

    long file_size = ftell(file);
    if (file_size < 0) {
        fclose(file);
        return NULL;
    }

    rewind(file);

    char* content = malloc(file_size + 1);
    if (!content) {
        fclose(file);
        return NULL;
    }

    size_t read_size = fread(content, 1, file_size, file);
    fclose(file);

    if (read_size != (size_t)file_size) {
        free(content);
        return NULL;
    }

    content[file_size] = '\0';
    if (out_size)
        *out_size = (size_t)file_size;
    return content;
}
