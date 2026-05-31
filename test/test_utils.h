#ifndef TEST_UTILS_H
#define TEST_UTILS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <io.h>
#define isatty_fd _isatty
#else
#include <unistd.h>
#define isatty_fd isatty
#endif

static int test_total = 0;
static int test_failed = 0;

static const char* g_pass = "  [PASS] ";
static const char* g_fail = "  [FAIL] ";

#define TEST(name, expr)                                                \
    do {                                                                \
        test_total++;                                                   \
        if (expr) {                                                     \
            printf("%s%s\n", g_pass, name);                             \
        } else {                                                        \
            test_failed++;                                              \
            printf("%s%s (%s:%d)\n", g_fail, name, __FILE__, __LINE__); \
        }                                                               \
    } while (0)

#define TEST_REQUIRE(name, expr)                                        \
    do {                                                                \
        test_total++;                                                   \
        if (expr) {                                                     \
            printf("%s%s\n", g_pass, name);                             \
        } else {                                                        \
            test_failed++;                                              \
            printf("%s%s (%s:%d)\n", g_fail, name, __FILE__, __LINE__); \
            goto test_cleanup;                                          \
        }                                                               \
    } while (0)

#endif
