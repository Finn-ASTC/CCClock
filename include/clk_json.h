#ifndef CLK_JSON_H
#define CLK_JSON_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------
 *  Types
 * ------------------------------------------------------------------ */

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

/* ------------------------------------------------------------------
 *  Parse
 * ------------------------------------------------------------------ */

/** Parse a null-terminated JSON string into a tree.
 *  @return Root node on success, NULL on failure. Free with
 *          clk_json_free(). */
clk_json_value* clk_json_parse(const char* json_str);

/** Parse JSON with error details. On failure copies the error message
 *  into @p errbuf (at most @p errbuf_size bytes). */
clk_json_value* clk_json_parse_ex(const char* json_str, char* errbuf, size_t errbuf_size);

/* ------------------------------------------------------------------
 *  Lifecycle
 * ------------------------------------------------------------------ */

/** Recursively free a JSON tree. NULL-safe. */
void clk_json_free(clk_json_value* root);

/* ------------------------------------------------------------------
 *  Serialize
 * ------------------------------------------------------------------ */

/** Serialize a JSON tree to a compact JSON string.
 *  @return malloc'd string, or NULL on failure. Caller must free(). */
char* clk_json_stringify(const clk_json_value* root);

/** Serialize with indentation and newlines for readability.
 *  @return malloc'd string, or NULL on failure. Caller must free(). */
char* clk_json_stringify_pretty(const clk_json_value* root);

/* ------------------------------------------------------------------
 *  Create
 * ------------------------------------------------------------------ */

clk_json_value* clk_json_create_null(void);
clk_json_value* clk_json_create_true(void);
clk_json_value* clk_json_create_false(void);
clk_json_value* clk_json_create_number(double num);
clk_json_value* clk_json_create_string(const char* str);
clk_json_value* clk_json_create_array(void);
clk_json_value* clk_json_create_object(void);

/* ------------------------------------------------------------------
 *  Type queries
 * ------------------------------------------------------------------ */

clk_json_type clk_json_get_type(const clk_json_value* value);

bool clk_json_is_null(const clk_json_value* value);
bool clk_json_is_true(const clk_json_value* value);
bool clk_json_is_false(const clk_json_value* value);
bool clk_json_is_number(const clk_json_value* value);
bool clk_json_is_string(const clk_json_value* value);
bool clk_json_is_array(const clk_json_value* value);
bool clk_json_is_object(const clk_json_value* value);

/* ------------------------------------------------------------------
 *  Value accessors
 * ------------------------------------------------------------------ */

/** Return 0 and set *bool_val on success; -1 if value is not boolean. */
int clk_json_get_boolean(const clk_json_value* value, bool* bool_val);

/** Return 0 and set *num_val on success; -1 if value is not a number. */
int clk_json_get_number(const clk_json_value* value, double* num_val);

/** Return 0 and set *str_val on success; -1 if value is not a string.
 *  The returned pointer is a borrow — do not free it. */
int clk_json_get_string(const clk_json_value* value, const char** str_val);

/* ------------------------------------------------------------------
 *  Object operations
 * ------------------------------------------------------------------ */

/** Look up @p key in @p object. Returns NULL if not found. */
clk_json_value* clk_json_object_get(const clk_json_value* object, const char* key);

/** Set @p key → @p value. If @p key already exists its old value is
 *  freed. @p value ownership is transferred to the object.
 *  @return 0 on success, -1 on failure. */
int clk_json_object_set(clk_json_value* object, const char* key, clk_json_value* value);

int clk_json_object_set_null(clk_json_value* object, const char* key);
int clk_json_object_set_true(clk_json_value* object, const char* key);
int clk_json_object_set_false(clk_json_value* object, const char* key);
int clk_json_object_set_number(clk_json_value* object, const char* key, double num);
int clk_json_object_set_string(clk_json_value* object, const char* key, const char* str);

/** Remove @p key and its value. @return 0 on success, -1 if not found. */
int clk_json_object_remove(clk_json_value* object, const char* key);

/** @return Number of key-value pairs in the object. */
size_t clk_json_object_count(const clk_json_value* object);

/* ------------------------------------------------------------------
 *  Object iterator (stack-allocated — no destroy needed)
 * ------------------------------------------------------------------ */

typedef struct clk_json_object_iterator {
    uintptr_t opaque[3];
} clk_json_object_iterator;

/** Initialise an iterator over @p object. @return 0 on success. */
int clk_json_object_iterator_init(const clk_json_value* object, clk_json_object_iterator* iter);

/** Advance the iterator. @return true while there are entries left. */
bool clk_json_object_iterator_next(clk_json_object_iterator* iter, clk_json_key_value_pair* pair);

/* ------------------------------------------------------------------
 *  Array operations
 * ------------------------------------------------------------------ */

clk_json_value* clk_json_array_get(const clk_json_value* array, size_t index);
int clk_json_array_append(clk_json_value* array, clk_json_value* value);
int clk_json_array_remove(clk_json_value* array, size_t index);
int clk_json_array_insert(clk_json_value* array, size_t index, clk_json_value* value);
int clk_json_array_set(clk_json_value* array, size_t index, clk_json_value* value);
int clk_json_array_count(const clk_json_value* array);

/* ------------------------------------------------------------------
 *  Equality & copy
 * ------------------------------------------------------------------ */

/** Deep structural equality — two trees are equal if they have the
 *  same shape and leaf values. Object key order does not matter. */
bool clk_json_equals(const clk_json_value* a, const clk_json_value* b);

/** Recursively deep-copy a JSON tree. The copy is independent of the
 *  original. @return malloc'd copy, or NULL on failure. */
clk_json_value* clk_json_deep_copy(const clk_json_value* value);

/* ------------------------------------------------------------------
 *  Merge
 * ------------------------------------------------------------------ */

/** Merge all key-value pairs from @p src into @p dest. Values are
 *  deep-copied — @p src is not modified. Existing keys in @p dest are
 *  overwritten. @return 0 on success, -1 on failure. */
int clk_json_merge_objects(clk_json_value* dest, const clk_json_value* src);

/* ------------------------------------------------------------------
 *  Path access
 * ------------------------------------------------------------------ */

/** Look up a value by path string, e.g. "users[0].name".
 *  Supports dot-notation for objects and bracket-notation for arrays.
 *  @return Found value, or NULL if any step is missing or invalid. */
clk_json_value* clk_json_get_by_path(const clk_json_value* root, const char* path);

#ifdef __cplusplus
}
#endif

#endif /* CLK_JSON_H */
