#ifndef CLK_JSON_H
#define CLK_JSON_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    JSON_NULL,
    JSON_TRUE,
    JSON_FALSE,
    JSON_NUMBER,
    JSON_STRING,
    JSON_ARRAY,
    JSON_OBJECT
} clk_json_type;

typedef struct clk_json_value clk_json_value;

typedef struct {
    clk_json_value** items;
    size_t count;
    size_t capacity;
} clk_json_array;

typedef struct {
    char* key;
    clk_json_value* value;
} clk_json_key_value_pair;

typedef struct {
    clk_json_key_value_pair* pairs;
    size_t count;
    size_t capacity;
} clk_json_object;

struct clk_json_value {
    clk_json_type type;
    union {
        double num_value;
        char* str_value;
        clk_json_array array_value;
        clk_json_object object_value;
    };
};

// 解析JSON字符串，返回指向JSON值的指针，失败返回NULL
clk_json_value* clk_json_parse(const char* json_str);

// 带有错误信息输出的JSON解析函数，如果解析失败会将错误信息写入errbuf
clk_json_value* clk_json_parse_ex(const char* json_str, char* errbuf, size_t errbuf_size);

// 释放JSON值及其所有子节点
void clk_json_free(clk_json_value* root);

// JSON值生成JSON字符串，失败返回NULL
char* clk_json_stringify(const clk_json_value* root);

// JSON值生成格式化的JSON字符串，失败返回NULL
char* clk_json_stringify_pretty(const clk_json_value* root);

// 创建JSON值函数

clk_json_value* clk_json_create_null(void);
clk_json_value* clk_json_create_true(void);
clk_json_value* clk_json_create_false(void);
clk_json_value* clk_json_create_number(double num);
clk_json_value* clk_json_create_string(const char* str);
clk_json_value* clk_json_create_array(void);
clk_json_value* clk_json_create_object(void);

// 获取JSON值类型的函数
clk_json_type clk_json_get_type(const clk_json_value* value);

// 判断JSON值类型的函数

bool clk_json_is_null(const clk_json_value* value);
bool clk_json_is_true(const clk_json_value* value);
bool clk_json_is_false(const clk_json_value* value);
bool clk_json_is_number(const clk_json_value* value);
bool clk_json_is_string(const clk_json_value* value);
bool clk_json_is_array(const clk_json_value* value);
bool clk_json_is_object(const clk_json_value* value);

// 从JSON值中获取数据的函数

int clk_json_get_boolean(const clk_json_value* value, bool* bool_val);
int clk_json_get_number(const clk_json_value* value, double* num_val);
int clk_json_get_string(const clk_json_value* value, const char** str_val);

// 查找对象
clk_json_value* clk_json_object_get(const clk_json_value* object, const char* key);

// 在对象中设置键值对
int clk_json_object_set(clk_json_value* object, const char* key, clk_json_value* value);

// 快捷设置对象中键值对的函数
int clk_json_object_set_null(clk_json_value* object, const char* key);
int clk_json_object_set_true(clk_json_value* object, const char* key);
int clk_json_object_set_false(clk_json_value* object, const char* key);
int clk_json_object_set_number(clk_json_value* object, const char* key, double num);
int clk_json_object_set_string(clk_json_value* object, const char* key, const char* str);

// 删除对象中的键值对
int clk_json_object_remove(clk_json_value* object, const char* key);

// 遍历对象

size_t clk_json_object_count(const clk_json_value* object);

typedef struct clk_json_object_iterator {
    const void* opaque[4];
} clk_json_object_iterator;

int clk_json_object_iterator_init(const clk_json_value* object, clk_json_object_iterator* iter);

bool clk_json_object_iterator_next(clk_json_object_iterator* iter, const char** key,
                                   clk_json_value** value);

// 数组相关
clk_json_value* clk_json_array_get(const clk_json_value* array, size_t index);
int clk_json_array_append(clk_json_value* array, clk_json_value* value);
int clk_json_array_remove(clk_json_value* array, size_t index);
int clk_json_array_insert(clk_json_value* array, size_t index, clk_json_value* value);
int clk_json_array_set(clk_json_value* array, size_t index, clk_json_value* value);
int clk_json_array_count(const clk_json_value* array);

// 深拷贝JSON值
clk_json_value* clk_json_deep_copy(const clk_json_value* value);

// 比较两个JSON值是否相等
bool clk_json_equals(const clk_json_value* a, const clk_json_value* b);

// 路径访问JSON值，格式为 "key1.key2[0].key3"
clk_json_value* clk_json_get_by_path(const clk_json_value* root, const char* path);

// 合并两个JSON对象，后者的键值对会覆盖前者的同名键值对
int clk_json_merge_objects(clk_json_value* dest, const clk_json_value* src);

#ifdef __cplusplus
}
#endif

#endif  // CLK_JSON_H
