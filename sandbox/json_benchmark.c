#include "clk_json.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifndef __unix__
#define _POSIX_
#endif

#define ROUNDS (10)

static char *read_whole_file(const char *path, size_t *out_size) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "Failed to open: %s\n", path);
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    if (size < 0) {
        fclose(f);
        return NULL;
    }
    rewind(f);
    char *buf = malloc((size_t)size + 1);
    if (!buf) {
        fclose(f);
        return NULL;
    }
    size_t rd = fread(buf, 1, (size_t)size, f);
    fclose(f);
    if (rd != (size_t)size) {
        free(buf);
        return NULL;
    }
    buf[size] = '\0';
    *out_size = (size_t)size;
    return buf;
}

static double now_sec(void) { return (double)clock() / (double)CLOCKS_PER_SEC; }

int main(int argc, char **argv) {
    const char *path = (argc > 1) ? argv[1] : "../assets/bench_19mb.json";

    size_t file_size = 0;
    char *json_text = read_whole_file(path, &file_size);
    if (!json_text)
        return 1;

    double mb = (double)file_size / (1024.0 * 1024.0);
    printf("File:  %s  (%.2f MB)\n", path, mb);

    /* ================================================================
     *  Parse benchmark
     * ================================================================ */
    {
        /* warm-up */
        clk_json_value *v = clk_json_parse(json_text);
        if (!v) {
            fprintf(stderr, "Parse warm-up failed\n");
            free(json_text);
            return 1;
        }
        clk_json_free(v);

        double start = now_sec();
        for (int i = 0; i < ROUNDS; i++) {
            v = clk_json_parse(json_text);
            clk_json_free(v);
        }
        double elapsed = now_sec() - start;
        double avg_s = elapsed / (double)ROUNDS;
        printf("Parse:  %.3f s/round  (%.1f MB/s)\n",
               avg_s, mb / avg_s);
    }

    /* ================================================================
     *  Stringify (compact) benchmark
     * ================================================================ */
    {
        clk_json_value *v = clk_json_parse(json_text);
        if (!v) {
            fprintf(stderr, "Parse for stringify failed\n");
            free(json_text);
            return 1;
        }

        /* warm-up — measure output size */
        char *s = clk_json_stringify(v);
        if (!s) {
            fprintf(stderr, "Stringify warm-up failed\n");
            clk_json_free(v);
            free(json_text);
            return 1;
        }
        size_t out_len = strlen(s);
        free(s);

        double start = now_sec();
        for (int i = 0; i < ROUNDS; i++) {
            s = clk_json_stringify(v);
            free(s);
        }
        double elapsed = now_sec() - start;
        double avg_s = elapsed / (double)ROUNDS;
        printf("Strfy:  %.3f s/round  (output: %.2f MB)\n",
               avg_s, (double)out_len / (1024.0 * 1024.0));

        clk_json_free(v);
    }

    free(json_text);
    printf("\nDone  (%d rounds)\n", ROUNDS);
    return 0;
}
