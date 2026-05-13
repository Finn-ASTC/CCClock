#include "clk_clock.h"

#include <corecrt_wconio.h>
#include <stdlib.h>
#include <string.h>

#include "clk_render.h"

bool clk_get_formatted_time(const clk_clock* clk, char* buffer, size_t buffer_size) {
    if (!clk || !buffer || buffer_size == 0)
        return false;

    size_t result = strftime(buffer, buffer_size, clk->format, &clk->time_info);
    if (result == 0 && buffer[0] != '\0')
        return false;
    return true;
}

/*
 * 创建时钟对象，分配内存并初始化格式和字体路径
 * 但注意没有初始化 time_info，需要手动调用 clk_clock_update 来获取当前时间
 */
clk_clock* clk_clock_create(const char* format, const char* font_path) {
    if (!format || !font_path) {
        return NULL;
    }
    clk_clock* clk = calloc(1, sizeof(clk_clock));
    if (!clk) {
        return NULL;
    }
    clk->format = strdup(format);
    clk->font_path = strdup(font_path);
    if (!clk->format || !clk->font_path) {
        clk_clock_destroy(clk);
        return NULL;
    }
    return clk;
}

void clk_clock_destroy(clk_clock* clk) {
    if (clk) {
        free(clk->format);
        free(clk->font_path);
        free(clk);
    }
}

bool clk_clock_update(clk_clock* clk) {
    if (!clk)
        return false;

    time_t now = time(NULL);
    if (now == (time_t)(-1))
        return false;
    struct tm local_time;
#ifdef _WIN32
    // Windows: localtime_s(tm*, time_t*)，返回 errno_t
    if (localtime_s(&local_time, &now) != 0)
        return false;
#else
    // POSIX (Linux/macOS): localtime_r(time_t*, tm*)，返回 tm* 或 NULL
    if (!localtime_r(&now, &local_time))
        return false;
#endif

    clk->time_info = local_time;
    return true;
}

bool clk_clock_set_format(clk_clock* clk, const char* format) {
    if (!clk || !format)
        return false;
    char* new_format = strdup(format);
    if (!new_format)
        return false;
    free(clk->format);
    clk->format = new_format;
    return true;
}

bool clk_clock_set_font(clk_clock* clk, const char* font_path) {
    if (!clk || !font_path)
        return false;
    char* new_font_path = strdup(font_path);
    if (!new_font_path)
        return false;
    free(clk->font_path);
    clk->font_path = new_font_path;
    return true;
}

clk_render_texture clk_clock_to_texture(const clk_clock* clk, clk_render_texture* texture) {
    if (!clk || !texture)
        return (clk_render_texture){0, 0, 0, 0, NULL, 0};

    char time_str[256];
    if (!clk_get_formatted_time(clk, time_str, sizeof(time_str)))
        return (clk_render_texture){0, 0, 0, 0, NULL, 0};

    
}