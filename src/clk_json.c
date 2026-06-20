#include "clk_json.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ================================================================
 *  Error handling
 * ================================================================ */

static char clk_json_parse_err[256];

#define SET_ERROR(fmt, ...) \
    snprintf(clk_json_parse_err, sizeof(clk_json_parse_err), fmt, ##__VA_ARGS__)

/* ================================================================
 *  Internal types: Lexer / Token
 * ================================================================ */

typedef struct {
    const char* json;
    int pos;
    int json_len;
    int line;
    int col;
} clk_json_lexer;

typedef enum {
    TOKEN_LEFT_BRACE,    /* { */
    TOKEN_RIGHT_BRACE,   /* } */
    TOKEN_LEFT_BRACKET,  /* [ */
    TOKEN_RIGHT_BRACKET, /* ] */
    TOKEN_COLON,         /* : */
    TOKEN_COMMA,         /* , */
    TOKEN_STRING,        /* "..." */
    TOKEN_NUMBER,        /* 123 */
    TOKEN_TRUE,          /* true */
    TOKEN_FALSE,         /* false */
    TOKEN_NULL,          /* null */
    TOKEN_EOF,           /* end of input */
    TOKEN_ERROR          /* lex error */
} clk_json_token_type;

typedef struct {
    clk_json_token_type type;
    char* str_value;
    int str_len;
    double num_value;
    int line, col;
} clk_json_token;

/* ================================================================
 *  Internal helpers
 * ================================================================ */

/** Portable strndup replacement — copies at most n bytes, stopping
 *  early at the first NUL. Always null-terminates. */
static char* clk_json_strndup(const char* src, size_t n) {
    if (!src || n == 0) {
        if (!src)
            return NULL;
        char* dst = malloc(1);
        if (dst)
            dst[0] = '\0';
        return dst;
    }
    size_t len = 0;
    while (len < n && src[len] != '\0')
        len++;
    char* dst = malloc(len + 1);
    if (dst) {
        memcpy(dst, src, len);
        dst[len] = '\0';
    }
    return dst;
}

static bool clk_json_is_delimiter(char c) {
    switch (c) {
        case '\0':
        case ' ':
        case '\t':
        case '\r':
        case '\n':
        case '{':
        case '}':
        case '[':
        case ']':
        case ':':
        case ',':
            return true;
        default:
            return false;
    }
}

static unsigned int parse_hex4(clk_json_lexer* lexer) {
    if (lexer->pos + 4 > lexer->json_len)
        return 0xFFFFFFFF;
    unsigned int codepoint = 0;
    for (int i = 0; i < 4; i++) {
        char c = lexer->json[lexer->pos + i];
        codepoint <<= 4;
        if (c >= '0' && c <= '9') {
            codepoint |= (c - '0');
        } else if (c >= 'a' && c <= 'f') {
            codepoint |= (c - 'a' + 10);
        } else if (c >= 'A' && c <= 'F') {
            codepoint |= (c - 'A' + 10);
        } else {
            return 0xFFFFFFFF;
        }
    }
    return codepoint;
}

static int encode_utf8(unsigned int cp, char* dst) {
    if (cp <= 0x7F) {
        dst[0] = cp;
        return 1;
    } else if (cp <= 0x7FF) {
        dst[0] = 0xC0 | (cp >> 6);
        dst[1] = 0x80 | (cp & 0x3F);
        return 2;
    } else if (cp <= 0xFFFF) {
        dst[0] = 0xE0 | (cp >> 12);
        dst[1] = 0x80 | ((cp >> 6) & 0x3F);
        dst[2] = 0x80 | (cp & 0x3F);
        return 3;
    } else {
        dst[0] = 0xF0 | (cp >> 18);
        dst[1] = 0x80 | ((cp >> 12) & 0x3F);
        dst[2] = 0x80 | ((cp >> 6) & 0x3F);
        dst[3] = 0x80 | (cp & 0x3F);
        return 4;
    }
}

/* ================================================================
 *  Lexer
 * ================================================================ */

static void clk_json_lexer_init(clk_json_lexer* lexer, const char* json_str) {
    lexer->json = json_str;
    lexer->pos = 0;
    lexer->json_len = json_str ? (int)strlen(json_str) : 0;
    lexer->line = 1;
    lexer->col = 1;
}

static void clk_json_lexer_next(clk_json_lexer* lexer, clk_json_token* token) {
    if (!token)
        return;

    free(token->str_value);
    memset(token, 0, sizeof(clk_json_token));

    while (1) {
        if (lexer->pos >= lexer->json_len)
            break;
        char c = lexer->json[lexer->pos];
        if (c == ' ' || c == '\t' || c == '\r') {
            lexer->pos++;
            lexer->col++;
        } else if (c == '\n') {
            lexer->pos++;
            lexer->line++;
            lexer->col = 1;
        } else {
            break;
        }
    }

    if (lexer->pos >= lexer->json_len) {
        token->type = TOKEN_EOF;
        return;
    }

    char c = lexer->json[lexer->pos];

    switch (c) {
        case '{':
            token->type = TOKEN_LEFT_BRACE;
            lexer->pos++;
            lexer->col++;
            break;
        case '}':
            token->type = TOKEN_RIGHT_BRACE;
            lexer->pos++;
            lexer->col++;
            break;
        case '[':
            token->type = TOKEN_LEFT_BRACKET;
            lexer->pos++;
            lexer->col++;
            break;
        case ']':
            token->type = TOKEN_RIGHT_BRACKET;
            lexer->pos++;
            lexer->col++;
            break;
        case ':':
            token->type = TOKEN_COLON;
            lexer->pos++;
            lexer->col++;
            break;
        case ',':
            token->type = TOKEN_COMMA;
            lexer->pos++;
            lexer->col++;
            break;
        case 't':
            if (lexer->pos + 4 <= lexer->json_len &&
                strncmp(lexer->json + lexer->pos, "true", 4) == 0 &&
                (lexer->pos + 4 >= lexer->json_len ||
                 clk_json_is_delimiter(lexer->json[lexer->pos + 4]))) {
                token->type = TOKEN_TRUE;
                lexer->pos += 4;
                lexer->col += 4;
            } else {
                token->type = TOKEN_ERROR;
            }
            break;
        case 'f':
            if (lexer->pos + 5 <= lexer->json_len &&
                strncmp(lexer->json + lexer->pos, "false", 5) == 0 &&
                (lexer->pos + 5 >= lexer->json_len ||
                 clk_json_is_delimiter(lexer->json[lexer->pos + 5]))) {
                token->type = TOKEN_FALSE;
                lexer->pos += 5;
                lexer->col += 5;
            } else {
                token->type = TOKEN_ERROR;
            }
            break;
        case 'n':
            if (lexer->pos + 4 <= lexer->json_len &&
                strncmp(lexer->json + lexer->pos, "null", 4) == 0 &&
                (lexer->pos + 4 >= lexer->json_len ||
                 clk_json_is_delimiter(lexer->json[lexer->pos + 4]))) {
                token->type = TOKEN_NULL;
                lexer->pos += 4;
                lexer->col += 4;
            } else {
                token->type = TOKEN_ERROR;
            }
            break;
        case '\0':
            token->type = TOKEN_EOF;
            break;
        case '"': {
            size_t capacity = 64;
            size_t len = 0;
            char* buf = malloc(capacity);

            lexer->pos++;
            lexer->col++;

            while (1) {
                if (lexer->pos >= lexer->json_len) {
                    token->type = TOKEN_ERROR;
                    goto string_error;
                }
                char c = lexer->json[lexer->pos];

                /* unterminated string */
                if (c == '\n') {
                    token->type = TOKEN_ERROR;
                    goto string_error;
                }

                if (c == '"')
                    break;

                if (c == '\\') {
                    lexer->pos++;
                    lexer->col++;
                    if (lexer->pos >= lexer->json_len) {
                        token->type = TOKEN_ERROR;
                        goto string_error;
                    }
                    char esc = lexer->json[lexer->pos];

                    switch (esc) {
                        case '"':
                            buf[len++] = '"';
                            break;
                        case '\\':
                            buf[len++] = '\\';
                            break;
                        case '/':
                            buf[len++] = '/';
                            break;
                        case 'b':
                            buf[len++] = '\b';
                            break;
                        case 'f':
                            buf[len++] = '\f';
                            break;
                        case 'n':
                            buf[len++] = '\n';
                            break;
                        case 'r':
                            buf[len++] = '\r';
                            break;
                        case 't':
                            buf[len++] = '\t';
                            break;
                        case 'u': {
                            /* skip 'u' */
                            lexer->pos++;
                            lexer->col++;
                            unsigned int codepoint = parse_hex4(lexer);

                            if (codepoint == 0xFFFFFFFF ||
                                (codepoint >= 0xDC00 && codepoint <= 0xDFFF)) {
                                token->type = TOKEN_ERROR;
                                goto string_error;
                            }

                            lexer->pos += 4;
                            lexer->col += 4;

                            /* surrogate pair: high surrogate must be
                             * followed by a low surrogate */
                            if (codepoint >= 0xD800 && codepoint <= 0xDBFF) {
                                if (lexer->pos + 2 <= lexer->json_len &&
                                    lexer->json[lexer->pos] == '\\' &&
                                    lexer->json[lexer->pos + 1] == 'u') {
                                    lexer->pos += 2;
                                    lexer->col += 2;
                                    unsigned int low = parse_hex4(lexer);
                                    if (low >= 0xDC00 && low <= 0xDFFF) {
                                        codepoint =
                                            0x10000 + ((codepoint - 0xD800) << 10) + (low - 0xDC00);
                                        lexer->pos += 4;
                                        lexer->col += 4;
                                    } else {
                                        token->type = TOKEN_ERROR;
                                        goto string_error;
                                    }
                                } else {
                                    token->type = TOKEN_ERROR;
                                    goto string_error;
                                }
                            }

                            if (len + 4 >= capacity) {
                                capacity *= 2;
                                char* temp_buf = realloc(buf, capacity);
                                if (!temp_buf) {
                                    free(buf);
                                    token->type = TOKEN_ERROR;
                                    return;
                                }
                                buf = temp_buf;
                            }

                            len += encode_utf8(codepoint, buf + len);
                            continue;
                        }
                        default:
                            token->type = TOKEN_ERROR;
                            goto string_error;
                    }
                    lexer->pos++;
                    lexer->col++;
                } else {
                    buf[len++] = c;
                    lexer->pos++;
                    lexer->col++;
                }

                if (len + 5 >= capacity) {
                    capacity *= 2;
                    char* temp_buf = realloc(buf, capacity);
                    if (!temp_buf) {
                        free(buf);
                        token->type = TOKEN_ERROR;
                        return;
                    }
                    buf = temp_buf;
                }
            }

            buf[len] = '\0';
            token->type = TOKEN_STRING;
            token->str_value = buf;
            token->str_len = len;
            lexer->pos++;
            lexer->col++;
            break;

        string_error:
            free(buf);
            /* skip closing quote even on error so the caller can continue */
            lexer->pos++;
            lexer->col++;
            break;
        }
        default:
            if ((c >= '0' && c <= '9') || c == '-') {
                const char* start = lexer->json + lexer->pos;
                char* endptr;
                token->num_value = strtod(start, &endptr);
                if (endptr == start) {
                    token->type = TOKEN_ERROR;
                } else {
                    token->type = TOKEN_NUMBER;
                    int len = endptr - start;
                    lexer->pos += len;
                    lexer->col += len;
                }
            } else {
                token->type = TOKEN_ERROR;
            }
            break;
    }
}

/* ================================================================
 *  Parser (recursive descent)
 * ================================================================ */

static clk_json_value* clk_json_alloc_node(void) {
    clk_json_value* v = malloc(sizeof(clk_json_value));
    if (!v)
        return NULL;
    memset(v, 0, sizeof(clk_json_value));
    return v;
}

static clk_json_value* clk_parse_object(clk_json_lexer* lexer);
static clk_json_value* clk_parse_array(clk_json_lexer* lexer);

/** Dispatch a pre-read token to the appropriate value constructor. */
static clk_json_value* clk_parse_value_from_token(clk_json_lexer* lexer, clk_json_token* token) {
    switch (token->type) {
        case TOKEN_LEFT_BRACE:
            return clk_parse_object(lexer);
        case TOKEN_LEFT_BRACKET:
            return clk_parse_array(lexer);
        case TOKEN_STRING:
            return clk_json_create_string(token->str_value);
        case TOKEN_NUMBER:
            return clk_json_create_number(token->num_value);
        case TOKEN_TRUE:
            return clk_json_create_true();
        case TOKEN_FALSE:
            return clk_json_create_false();
        case TOKEN_NULL:
            return clk_json_create_null();
        default:
            SET_ERROR("Unexpected token at line %d, col %d", token->line, token->col);
            return NULL;
    }
}

/** Read the next token and dispatch it. */
static clk_json_value* clk_parse_value(clk_json_lexer* lexer) {
    clk_json_token token = {0};
    clk_json_lexer_next(lexer, &token);
    clk_json_value* result = clk_parse_value_from_token(lexer, &token);
    free(token.str_value);
    return result;
}

static clk_json_value* clk_parse_object(clk_json_lexer* lexer) {
    clk_json_value* obj = clk_json_create_object();
    if (!obj)
        return NULL;

    clk_json_token token = {0};
    clk_json_lexer_next(lexer, &token);

    if (token.type == TOKEN_RIGHT_BRACE)
        return obj;

    while (1) {
        if (token.type != TOKEN_STRING) {
            SET_ERROR("Expected string key in object at line %d, col %d", token.line, token.col);
            clk_json_free(obj);
            return NULL;
        }
        char* key = clk_json_strndup(token.str_value, strlen(token.str_value));

        clk_json_lexer_next(lexer, &token);
        if (token.type != TOKEN_COLON) {
            SET_ERROR("Expected ':' after object key at line %d, col %d", token.line, token.col);
            free(key);
            clk_json_free(obj);
            return NULL;
        }

        clk_json_value* val = clk_parse_value(lexer);
        if (!val) {
            SET_ERROR("Invalid value in object at line %d, col %d", token.line, token.col);
            free(key);
            clk_json_free(obj);
            return NULL;
        }
        clk_json_object_set(obj, key, val);
        free(key);

        clk_json_lexer_next(lexer, &token);
        if (token.type == TOKEN_RIGHT_BRACE)
            return obj;
        if (token.type != TOKEN_COMMA) {
            SET_ERROR("Expected ',' or '}' after value at line %d, col %d", token.line, token.col);
            clk_json_free(obj);
            return NULL;
        }

        clk_json_lexer_next(lexer, &token);
    }
}

static clk_json_value* clk_parse_array(clk_json_lexer* lexer) {
    clk_json_value* arr = clk_json_create_array();
    if (!arr)
        return NULL;

    clk_json_token token = {0};
    clk_json_lexer_next(lexer, &token);

    if (token.type == TOKEN_RIGHT_BRACKET)
        return arr;

    while (1) {
        /* token has already been pre-read — hand off to the
         * token-aware dispatcher so we don't lose the first value */
        clk_json_value* val = clk_parse_value_from_token(lexer, &token);
        if (!val) {
            SET_ERROR("Invalid value in array at line %d, col %d", token.line, token.col);
            clk_json_free(arr);
            return NULL;
        }
        clk_json_array_append(arr, val);

        clk_json_lexer_next(lexer, &token);
        if (token.type == TOKEN_RIGHT_BRACKET)
            return arr;
        if (token.type != TOKEN_COMMA) {
            SET_ERROR("Expected ',' or ']' after array value at line %d, col %d", token.line,
                      token.col);
            clk_json_free(arr);
            return NULL;
        }

        clk_json_lexer_next(lexer, &token);

        if (token.type == TOKEN_RIGHT_BRACKET) {
            SET_ERROR("Trailing comma in array at line %d, col %d", token.line, token.col);
            clk_json_free(arr);
            return NULL;
        }
    }
}

/* ================================================================
 *  Public: Parse
 * ================================================================ */

clk_json_value* clk_json_parse(const char* json_str) {
    if (!json_str)
        return NULL;

    clk_json_parse_err[0] = '\0';

    clk_json_lexer lexer;
    clk_json_lexer_init(&lexer, json_str);
    return clk_parse_value(&lexer);
}

clk_json_value* clk_json_parse_ex(const char* json_str, char* errbuf, size_t errbuf_size) {
    clk_json_value* root = clk_json_parse(json_str);
    if (!root && errbuf && errbuf_size > 0) {
#ifdef _MSC_VER
        strncpy_s(errbuf, errbuf_size, clk_json_parse_err, errbuf_size - 1);
#else
        strncpy(errbuf, clk_json_parse_err, errbuf_size - 1);
        errbuf[errbuf_size - 1] = '\0';
#endif
    }
    return root;
}

/* ================================================================
 *  Public: Create
 * ================================================================ */

clk_json_value* clk_json_create_null(void) {
    clk_json_value* v = clk_json_alloc_node();
    if (v)
        v->type = JSON_NULL;
    return v;
}

clk_json_value* clk_json_create_true(void) {
    clk_json_value* v = clk_json_alloc_node();
    if (v)
        v->type = JSON_TRUE;
    return v;
}

clk_json_value* clk_json_create_false(void) {
    clk_json_value* v = clk_json_alloc_node();
    if (v)
        v->type = JSON_FALSE;
    return v;
}

clk_json_value* clk_json_create_number(double num) {
    clk_json_value* v = clk_json_alloc_node();
    if (!v)
        return NULL;
    v->type = JSON_NUMBER;
    v->num_value = num;
    return v;
}

clk_json_value* clk_json_create_string(const char* str) {
    clk_json_value* v = clk_json_alloc_node();
    if (!v)
        return NULL;
    v->type = JSON_STRING;
    v->str_value = clk_json_strndup(str, strlen(str));
    if (!v->str_value) {
        clk_json_free(v);
        return NULL;
    }
    return v;
}

clk_json_value* clk_json_create_array(void) {
    clk_json_value* v = clk_json_alloc_node();
    if (!v)
        return NULL;
    v->type = JSON_ARRAY;
    v->array_value.capacity = 8;
    v->array_value.items = malloc(v->array_value.capacity * sizeof(clk_json_value*));
    if (!v->array_value.items) {
        clk_json_free(v);
        return NULL;
    }
    return v;
}

clk_json_value* clk_json_create_object(void) {
    clk_json_value* v = clk_json_alloc_node();
    if (!v)
        return NULL;
    v->type = JSON_OBJECT;
    v->object_value.capacity = 8;
    v->object_value.pairs = malloc(v->object_value.capacity * sizeof(clk_json_key_value_pair));
    if (!v->object_value.pairs) {
        clk_json_free(v);
        return NULL;
    }
    return v;
}

/* ================================================================
 *  Public: Type queries
 * ================================================================ */

clk_json_type clk_json_get_type(const clk_json_value* value) {
    if (!value)
        return JSON_NULL;
    return value->type;
}

bool clk_json_is_null(const clk_json_value* value) {
    return value && value->type == JSON_NULL;
}

bool clk_json_is_true(const clk_json_value* value) {
    return value && value->type == JSON_TRUE;
}

bool clk_json_is_false(const clk_json_value* value) {
    return value && value->type == JSON_FALSE;
}

bool clk_json_is_number(const clk_json_value* value) {
    return value && value->type == JSON_NUMBER;
}

bool clk_json_is_string(const clk_json_value* value) {
    return value && value->type == JSON_STRING;
}

bool clk_json_is_array(const clk_json_value* value) {
    return value && value->type == JSON_ARRAY;
}

bool clk_json_is_object(const clk_json_value* value) {
    return value && value->type == JSON_OBJECT;
}

int clk_json_get_boolean(const clk_json_value* value, bool* bool_val) {
    if (!value || !bool_val)
        return -1;
    if (value->type == JSON_TRUE) {
        *bool_val = true;
    } else if (value->type == JSON_FALSE) {
        *bool_val = false;
    } else {
        return -1;
    }
    return 0;
}

int clk_json_get_number(const clk_json_value* value, double* num_val) {
    if (!value || value->type != JSON_NUMBER || !num_val)
        return -1;
    *num_val = value->num_value;
    return 0;
}

int clk_json_get_string(const clk_json_value* value, const char** str_val) {
    if (!value || value->type != JSON_STRING || !str_val)
        return -1;
    *str_val = value->str_value;
    return 0;
}

/* ================================================================
 *  Public: Lifecycle
 * ================================================================ */

void clk_json_free(clk_json_value* root) {
    if (!root)
        return;
    switch (root->type) {
        case JSON_STRING:
            free(root->str_value);
            break;
        case JSON_ARRAY:
            for (size_t i = 0; i < root->array_value.count; ++i)
                clk_json_free(root->array_value.items[i]);
            free(root->array_value.items);
            break;
        case JSON_OBJECT:
            for (size_t i = 0; i < root->object_value.count; ++i) {
                free(root->object_value.pairs[i].key);
                clk_json_free(root->object_value.pairs[i].value);
            }
            free(root->object_value.pairs);
            break;
        default:
            break;
    }
    free(root);
}

/* ================================================================
 *  Public: Object operations
 * ================================================================ */

clk_json_value* clk_json_object_get(const clk_json_value* object, const char* key) {
    if (!object || object->type != JSON_OBJECT || !key)
        return NULL;

    for (size_t i = 0; i < object->object_value.count; ++i) {
        if (strcmp(object->object_value.pairs[i].key, key) == 0)
            return object->object_value.pairs[i].value;
    }
    return NULL;
}

int clk_json_object_set(clk_json_value* object, const char* key, clk_json_value* value) {
    if (!object || object->type != JSON_OBJECT || !key || !value)
        return -1;

    for (size_t i = 0; i < object->object_value.count; ++i) {
        if (strcmp(object->object_value.pairs[i].key, key) == 0) {
            clk_json_free(object->object_value.pairs[i].value);
            object->object_value.pairs[i].value = value;
            return 0;
        }
    }

    if (object->object_value.count >= object->object_value.capacity) {
        size_t new_cap = object->object_value.capacity * 2;
        clk_json_key_value_pair* temp =
            realloc(object->object_value.pairs, new_cap * sizeof(clk_json_key_value_pair));
        if (!temp)
            return -1;
        object->object_value.pairs = temp;
        object->object_value.capacity = new_cap;
    }

    object->object_value.pairs[object->object_value.count].key = clk_json_strndup(key, strlen(key));
    object->object_value.pairs[object->object_value.count].value = value;
    object->object_value.count++;
    return 0;
}

int clk_json_object_set_null(clk_json_value* object, const char* key) {
    return clk_json_object_set(object, key, clk_json_create_null());
}

int clk_json_object_set_true(clk_json_value* object, const char* key) {
    return clk_json_object_set(object, key, clk_json_create_true());
}

int clk_json_object_set_false(clk_json_value* object, const char* key) {
    return clk_json_object_set(object, key, clk_json_create_false());
}

int clk_json_object_set_number(clk_json_value* object, const char* key, double num) {
    return clk_json_object_set(object, key, clk_json_create_number(num));
}

int clk_json_object_set_string(clk_json_value* object, const char* key, const char* str) {
    return clk_json_object_set(object, key, clk_json_create_string(str));
}

int clk_json_object_remove(clk_json_value* object, const char* key) {
    if (!object || object->type != JSON_OBJECT || !key)
        return -1;

    for (size_t i = 0; i < object->object_value.count; ++i) {
        if (strcmp(object->object_value.pairs[i].key, key) == 0) {
            free(object->object_value.pairs[i].key);
            clk_json_free(object->object_value.pairs[i].value);
            memmove(&object->object_value.pairs[i], &object->object_value.pairs[i + 1],
                    (object->object_value.count - i - 1) * sizeof(clk_json_key_value_pair));
            object->object_value.count--;
            return 0;
        }
    }
    return -1;
}

size_t clk_json_object_count(const clk_json_value* object) {
    if (!object || object->type != JSON_OBJECT)
        return 0;
    return object->object_value.count;
}

int clk_json_object_iterator_init(const clk_json_value* object, clk_json_object_iterator* iter) {
    if (!object || object->type != JSON_OBJECT || !iter)
        return -1;
    iter->opaque[0] = (uintptr_t)object;
    iter->opaque[1] = 0;
    return 0;
}

bool clk_json_object_iterator_next(clk_json_object_iterator* iter, clk_json_key_value_pair* pair) {
    if (!iter || !pair)
        return false;

    const clk_json_value* object = (const clk_json_value*)iter->opaque[0];
    const size_t index = (size_t)iter->opaque[1];

    if (!object || index >= object->object_value.count)
        return false;

    pair->key = object->object_value.pairs[index].key;
    pair->value = object->object_value.pairs[index].value;
    iter->opaque[1] = (uintptr_t)(index + 1);
    return true;
}

/* ================================================================
 *  Public: Array operations
 * ================================================================ */

clk_json_value* clk_json_array_get(const clk_json_value* array, size_t index) {
    if (!array || array->type != JSON_ARRAY || index >= array->array_value.count)
        return NULL;
    return array->array_value.items[index];
}

int clk_json_array_append(clk_json_value* array, clk_json_value* value) {
    if (!array || array->type != JSON_ARRAY || !value)
        return -1;

    if (array->array_value.count >= array->array_value.capacity) {
        size_t new_cap = array->array_value.capacity * 2;
        clk_json_value** temp =
            realloc(array->array_value.items, new_cap * sizeof(clk_json_value*));
        if (!temp)
            return -1;
        array->array_value.items = temp;
        array->array_value.capacity = new_cap;
    }

    array->array_value.items[array->array_value.count++] = value;
    return 0;
}

int clk_json_array_remove(clk_json_value* array, size_t index) {
    if (!array || array->type != JSON_ARRAY || index >= array->array_value.count)
        return -1;

    clk_json_free(array->array_value.items[index]);
    memmove(&array->array_value.items[index], &array->array_value.items[index + 1],
            (array->array_value.count - index - 1) * sizeof(clk_json_value*));
    array->array_value.count--;
    return 0;
}

int clk_json_array_insert(clk_json_value* array, size_t index, clk_json_value* value) {
    if (!array || array->type != JSON_ARRAY || index > array->array_value.count || !value)
        return -1;

    if (array->array_value.count >= array->array_value.capacity) {
        size_t new_cap = array->array_value.capacity * 2;
        clk_json_value** temp =
            realloc(array->array_value.items, new_cap * sizeof(clk_json_value*));
        if (!temp)
            return -1;
        array->array_value.items = temp;
        array->array_value.capacity = new_cap;
    }

    memmove(&array->array_value.items[index + 1], &array->array_value.items[index],
            (array->array_value.count - index) * sizeof(clk_json_value*));
    array->array_value.items[index] = value;
    array->array_value.count++;
    return 0;
}

int clk_json_array_set(clk_json_value* array, size_t index, clk_json_value* value) {
    if (!array || array->type != JSON_ARRAY || index >= array->array_value.count || !value)
        return -1;

    clk_json_free(array->array_value.items[index]);
    array->array_value.items[index] = value;
    return 0;
}

int clk_json_array_count(const clk_json_value* array) {
    if (!array || array->type != JSON_ARRAY)
        return 0;
    return (int)array->array_value.count;
}

/* ================================================================
 *  Serialize — compact
 * ================================================================ */

static bool clk_json_str_ensure_capacity(char** buf, size_t* len, size_t* capacity,
                                         size_t additional) {
    if (*len > SIZE_MAX - additional)
        return false;
    if (*len + additional + 1 >= *capacity) {
        size_t new_cap = (*capacity) * 2 + additional;
        char* temp = realloc(*buf, new_cap);
        if (!temp)
            return false;
        *buf = temp;
        *capacity = new_cap;
    }
    return true;
}

static bool clk_json_append_null_to_str(char** buf, size_t* len, size_t* capacity) {
    if (!clk_json_str_ensure_capacity(buf, len, capacity, 4))
        return false;
    memcpy(*buf + *len, "null", 4);
    *len += 4;
    return true;
}

static bool clk_json_append_true_to_str(char** buf, size_t* len, size_t* capacity) {
    if (!clk_json_str_ensure_capacity(buf, len, capacity, 4))
        return false;
    memcpy(*buf + *len, "true", 4);
    *len += 4;
    return true;
}

static bool clk_json_append_false_to_str(char** buf, size_t* len, size_t* capacity) {
    if (!clk_json_str_ensure_capacity(buf, len, capacity, 5))
        return false;
    memcpy(*buf + *len, "false", 5);
    *len += 5;
    return true;
}

static bool clk_json_append_number_to_str(char** buf, size_t* len, size_t* capacity, double num) {
    int num_len = snprintf(NULL, 0, "%.17g", num);
    if (num_len < 0 || !clk_json_str_ensure_capacity(buf, len, capacity, (size_t)num_len))
        return false;
    snprintf(*buf + *len, num_len + 1, "%.17g", num);
    *len += (size_t)num_len;
    return true;
}

static bool clk_json_append_string_to_str(char** buf, size_t* len, size_t* capacity,
                                          const char* str) {
    if (!str)
        return false;

    if (!clk_json_str_ensure_capacity(buf, len, capacity, 1))
        return false;
    (*buf)[(*len)++] = '"';

    for (const char* p = str; *p; ++p) {
        char c = *p;
        switch (c) {
            case '"':
                if (!clk_json_str_ensure_capacity(buf, len, capacity, 2))
                    return false;
                (*buf)[(*len)++] = '\\';
                (*buf)[(*len)++] = '"';
                break;
            case '\\':
                if (!clk_json_str_ensure_capacity(buf, len, capacity, 2))
                    return false;
                (*buf)[(*len)++] = '\\';
                (*buf)[(*len)++] = '\\';
                break;
            case '\b':
                if (!clk_json_str_ensure_capacity(buf, len, capacity, 2))
                    return false;
                (*buf)[(*len)++] = '\\';
                (*buf)[(*len)++] = 'b';
                break;
            case '\f':
                if (!clk_json_str_ensure_capacity(buf, len, capacity, 2))
                    return false;
                (*buf)[(*len)++] = '\\';
                (*buf)[(*len)++] = 'f';
                break;
            case '\n':
                if (!clk_json_str_ensure_capacity(buf, len, capacity, 2))
                    return false;
                (*buf)[(*len)++] = '\\';
                (*buf)[(*len)++] = 'n';
                break;
            case '\r':
                if (!clk_json_str_ensure_capacity(buf, len, capacity, 2))
                    return false;
                (*buf)[(*len)++] = '\\';
                (*buf)[(*len)++] = 'r';
                break;
            case '\t':
                if (!clk_json_str_ensure_capacity(buf, len, capacity, 2))
                    return false;
                (*buf)[(*len)++] = '\\';
                (*buf)[(*len)++] = 't';
                break;
            default:
                /* control characters → \u00XX */
                if ((unsigned char)c < 0x20) {
                    if (!clk_json_str_ensure_capacity(buf, len, capacity, 6))
                        return false;
                    sprintf(*buf + *len, "\\u%04x", (unsigned char)c);
                    *len += 6;
                } else {
                    if (!clk_json_str_ensure_capacity(buf, len, capacity, 1))
                        return false;
                    (*buf)[(*len)++] = c;
                }
        }
    }

    if (!clk_json_str_ensure_capacity(buf, len, capacity, 1))
        return false;
    (*buf)[(*len)++] = '"';
    return true;
}

static bool clk_json_append_array_to_str(char** buf, size_t* len, size_t* cap,
                                         const clk_json_value* root);
static bool clk_json_append_object_to_str(char** buf, size_t* len, size_t* cap,
                                          const clk_json_value* root);

/** Single dispatch entry-point for compact serialization.
 *  Every value (including nested children) passes through here. */
static bool clk_json_append_value_to_str(char** buf, size_t* len, size_t* cap,
                                         const clk_json_value* root) {
    switch (root->type) {
        case JSON_NULL:
            return clk_json_append_null_to_str(buf, len, cap);
        case JSON_TRUE:
            return clk_json_append_true_to_str(buf, len, cap);
        case JSON_FALSE:
            return clk_json_append_false_to_str(buf, len, cap);
        case JSON_NUMBER:
            return clk_json_append_number_to_str(buf, len, cap, root->num_value);
        case JSON_STRING:
            return clk_json_append_string_to_str(buf, len, cap, root->str_value);
        case JSON_ARRAY:
            return clk_json_append_array_to_str(buf, len, cap, root);
        case JSON_OBJECT:
            return clk_json_append_object_to_str(buf, len, cap, root);
        default:
            return false;
    }
}

char* clk_json_stringify(const clk_json_value* root) {
    if (!root)
        return NULL;

    char* buf = malloc(256);
    if (!buf)
        return NULL;

    size_t capacity = 256;
    size_t len = 0;

    if (!clk_json_append_value_to_str(&buf, &len, &capacity, root)) {
        free(buf);
        return NULL;
    }

    buf[len] = '\0';
    return buf;
}

static bool clk_json_append_array_to_str(char** buf, size_t* len, size_t* cap,
                                         const clk_json_value* root) {
    if (!clk_json_str_ensure_capacity(buf, len, cap, 1))
        return false;
    (*buf)[(*len)++] = '[';

    for (size_t i = 0; i < root->array_value.count; ++i) {
        if (i > 0) {
            if (!clk_json_str_ensure_capacity(buf, len, cap, 1))
                return false;
            (*buf)[(*len)++] = ',';
        }
        if (!clk_json_append_value_to_str(buf, len, cap, root->array_value.items[i]))
            return false;
    }

    if (!clk_json_str_ensure_capacity(buf, len, cap, 1))
        return false;
    (*buf)[(*len)++] = ']';
    return true;
}

static bool clk_json_append_object_to_str(char** buf, size_t* len, size_t* cap,
                                          const clk_json_value* root) {
    if (!clk_json_str_ensure_capacity(buf, len, cap, 1))
        return false;
    (*buf)[(*len)++] = '{';

    for (size_t i = 0; i < root->object_value.count; ++i) {
        if (i > 0) {
            if (!clk_json_str_ensure_capacity(buf, len, cap, 1))
                return false;
            (*buf)[(*len)++] = ',';
        }
        if (!clk_json_append_string_to_str(buf, len, cap, root->object_value.pairs[i].key))
            return false;
        if (!clk_json_str_ensure_capacity(buf, len, cap, 1))
            return false;
        (*buf)[(*len)++] = ':';
        if (!clk_json_append_value_to_str(buf, len, cap, root->object_value.pairs[i].value))
            return false;
    }

    if (!clk_json_str_ensure_capacity(buf, len, cap, 1))
        return false;
    (*buf)[(*len)++] = '}';
    return true;
}

/* ================================================================
 *  Serialize — pretty-print
 * ================================================================ */

static bool clk_json_append_newline(char** buf, size_t* len, size_t* cap) {
    if (!clk_json_str_ensure_capacity(buf, len, cap, 1))
        return false;
    (*buf)[(*len)++] = '\n';
    return true;
}

static bool clk_json_append_indent(char** buf, size_t* len, size_t* cap, int depth) {
    for (int i = 0; i < depth; ++i) {
        if (!clk_json_str_ensure_capacity(buf, len, cap, 2))
            return false;
        (*buf)[(*len)++] = ' ';
        (*buf)[(*len)++] = ' ';
    }
    return true;
}

/* forward declarations */
static bool clk_json_append_array_to_str_pretty(char** buf, size_t* len, size_t* cap,
                                                const clk_json_value* root, int depth);
static bool clk_json_append_object_to_str_pretty(char** buf, size_t* len, size_t* cap,
                                                 const clk_json_value* root, int depth);

static bool clk_json_append_value_to_str_pretty(char** buf, size_t* len, size_t* cap,
                                                const clk_json_value* root, int depth) {
    switch (root->type) {
        case JSON_NULL:
            return clk_json_append_null_to_str(buf, len, cap);
        case JSON_TRUE:
            return clk_json_append_true_to_str(buf, len, cap);
        case JSON_FALSE:
            return clk_json_append_false_to_str(buf, len, cap);
        case JSON_NUMBER:
            return clk_json_append_number_to_str(buf, len, cap, root->num_value);
        case JSON_STRING:
            return clk_json_append_string_to_str(buf, len, cap, root->str_value);
        case JSON_ARRAY:
            return clk_json_append_array_to_str_pretty(buf, len, cap, root, depth);
        case JSON_OBJECT:
            return clk_json_append_object_to_str_pretty(buf, len, cap, root, depth);
        default:
            return false;
    }
}

static bool clk_json_append_array_to_str_pretty(char** buf, size_t* len, size_t* cap,
                                                const clk_json_value* root, int depth) {
    if (!clk_json_str_ensure_capacity(buf, len, cap, 1))
        return false;
    (*buf)[(*len)++] = '[';

    /* Determine if this array needs multi-line formatting.
     * Arrays of only scalars stay inline; arrays containing other
     * arrays or objects get expanded. */
    bool new_line_needed = false;
    for (size_t i = 0; i < root->array_value.count; ++i) {
        if (root->array_value.items[i]->type == JSON_ARRAY ||
            root->array_value.items[i]->type == JSON_OBJECT) {
            new_line_needed = true;
            break;
        }
    }

    if (new_line_needed) {
        if (!clk_json_append_newline(buf, len, cap))
            return false;
    }

    for (size_t i = 0; i < root->array_value.count; ++i) {
        if (new_line_needed) {
            if (!clk_json_append_indent(buf, len, cap, depth + 1))
                return false;
        }
        if (!clk_json_append_value_to_str_pretty(buf, len, cap, root->array_value.items[i],
                                                 depth + 1))
            return false;
        if (i < root->array_value.count - 1) {
            if (!clk_json_str_ensure_capacity(buf, len, cap, 1))
                return false;
            (*buf)[(*len)++] = ',';
        }
        if (new_line_needed) {
            if (!clk_json_append_newline(buf, len, cap))
                return false;
        }
    }

    if (new_line_needed) {
        if (!clk_json_append_indent(buf, len, cap, depth))
            return false;
    }
    if (!clk_json_str_ensure_capacity(buf, len, cap, 1))
        return false;
    (*buf)[(*len)++] = ']';
    return true;
}

static bool clk_json_append_object_to_str_pretty(char** buf, size_t* len, size_t* cap,
                                                 const clk_json_value* root, int depth) {
    if (!clk_json_str_ensure_capacity(buf, len, cap, 1))
        return false;
    (*buf)[(*len)++] = '{';

    if (root->object_value.count > 0) {
        if (!clk_json_append_newline(buf, len, cap))
            return false;
    }

    for (size_t i = 0; i < root->object_value.count; ++i) {
        if (!clk_json_append_indent(buf, len, cap, depth + 1))
            return false;
        if (!clk_json_append_string_to_str(buf, len, cap, root->object_value.pairs[i].key))
            return false;
        if (!clk_json_str_ensure_capacity(buf, len, cap, 2))
            return false;
        (*buf)[(*len)++] = ':';
        (*buf)[(*len)++] = ' ';
        if (!clk_json_append_value_to_str_pretty(buf, len, cap, root->object_value.pairs[i].value,
                                                 depth + 1))
            return false;
        if (i < root->object_value.count - 1) {
            if (!clk_json_str_ensure_capacity(buf, len, cap, 1))
                return false;
            (*buf)[(*len)++] = ',';
        }
        if (!clk_json_append_newline(buf, len, cap))
            return false;
    }

    if (root->object_value.count > 0) {
        if (!clk_json_append_indent(buf, len, cap, depth))
            return false;
    }

    if (!clk_json_str_ensure_capacity(buf, len, cap, 1))
        return false;
    (*buf)[(*len)++] = '}';
    return true;
}

char* clk_json_stringify_pretty(const clk_json_value* root) {
    if (!root)
        return NULL;

    char* buf = malloc(256);
    if (!buf)
        return NULL;

    size_t capacity = 256;
    size_t len = 0;

    if (!clk_json_append_value_to_str_pretty(&buf, &len, &capacity, root, 0)) {
        free(buf);
        return NULL;
    }

    buf[len] = '\0';
    return buf;
}

/* ================================================================
 *  Equality
 * ================================================================ */

bool clk_json_equals(const clk_json_value* a, const clk_json_value* b) {
    if (a == b)
        return true;
    if (!a || !b || a->type != b->type)
        return false;

    switch (a->type) {
        case JSON_NULL:
        case JSON_TRUE:
        case JSON_FALSE:
            return true;
        case JSON_NUMBER:
            return a->num_value == b->num_value;
        case JSON_STRING:
            return strcmp(a->str_value, b->str_value) == 0;
        case JSON_ARRAY:
            if (a->array_value.count != b->array_value.count)
                return false;
            for (size_t i = 0; i < a->array_value.count; ++i) {
                if (!clk_json_equals(a->array_value.items[i], b->array_value.items[i]))
                    return false;
            }
            return true;
        case JSON_OBJECT:
            if (a->object_value.count != b->object_value.count)
                return false;
            {
                clk_json_object_iterator iter;
                clk_json_object_iterator_init(a, &iter);
                clk_json_key_value_pair pair;
                while (clk_json_object_iterator_next(&iter, &pair)) {
                    clk_json_value* bv = clk_json_object_get(b, pair.key);
                    if (!bv || !clk_json_equals(pair.value, bv))
                        return false;
                }
            }
            return true;
        default:
            return false;
    }
}

/* ================================================================
 *  Deep copy
 * ================================================================ */

clk_json_value* clk_json_deep_copy(const clk_json_value* src) {
    if (!src)
        return NULL;

    switch (src->type) {
        case JSON_NULL:
            return clk_json_create_null();
        case JSON_TRUE:
            return clk_json_create_true();
        case JSON_FALSE:
            return clk_json_create_false();
        case JSON_NUMBER:
            return clk_json_create_number(src->num_value);
        case JSON_STRING:
            return clk_json_create_string(src->str_value);
        case JSON_ARRAY: {
            clk_json_value* arr = clk_json_create_array();
            if (!arr)
                return NULL;
            for (size_t i = 0; i < src->array_value.count; ++i) {
                clk_json_value* copy = clk_json_deep_copy(src->array_value.items[i]);
                if (!copy) {
                    clk_json_free(arr);
                    return NULL;
                }
                clk_json_array_append(arr, copy);
            }
            return arr;
        }
        case JSON_OBJECT: {
            clk_json_value* obj = clk_json_create_object();
            if (!obj)
                return NULL;
            clk_json_object_iterator iter;
            clk_json_object_iterator_init(src, &iter);
            clk_json_key_value_pair pair;
            while (clk_json_object_iterator_next(&iter, &pair)) {
                clk_json_value* copy = clk_json_deep_copy(pair.value);
                if (!copy) {
                    clk_json_free(obj);
                    return NULL;
                }
                clk_json_object_set(obj, pair.key, copy);
            }
            return obj;
        }
        default:
            return NULL;
    }
}

/* ================================================================
 *  Merge
 * ================================================================ */

int clk_json_merge_objects(clk_json_value* dest, const clk_json_value* src) {
    if (!dest || dest->type != JSON_OBJECT || !src || src->type != JSON_OBJECT)
        return -1;

    clk_json_object_iterator iter;
    if (clk_json_object_iterator_init(src, &iter) != 0)
        return -1;

    clk_json_key_value_pair pair;
    while (clk_json_object_iterator_next(&iter, &pair)) {
        if (clk_json_object_set(dest, pair.key, clk_json_deep_copy(pair.value)) != 0)
            return -1;
    }
    return 0;
}

/* ================================================================
 *  Path access
 * ================================================================ */

clk_json_value* clk_json_get_by_path(const clk_json_value* root, const char* path) {
    if (!root || !path)
        return NULL;
    clk_json_value* current = (clk_json_value*)root;

    int pos = 0;

    while (1) {
        char c = path[pos++];

        switch (c) {
            case '\0':
                return current;
            case '.': {
                if (current->type != JSON_OBJECT)
                    return NULL;
                size_t start = pos;
                while (path[pos] && path[pos] != '.' && path[pos] != '[')
                    pos++;
                size_t key_len = pos - start;
                char* key = clk_json_strndup(path + start, key_len);
                if (!key)
                    return NULL;
                clk_json_value* next = clk_json_object_get(current, key);
                free(key);
                if (!next)
                    return NULL;
                current = next;
                break;
            }
            case '[': {
                if (current->type != JSON_ARRAY)
                    return NULL;
                size_t start = pos;
                while (path[pos] && path[pos] != ']')
                    pos++;
                if (path[pos] != ']')
                    return NULL;
                size_t index_len = pos - start;
                char* index_str = clk_json_strndup(path + start, index_len);
                if (!index_str)
                    return NULL;
                char* endptr;
                long index = strtol(index_str, &endptr, 10);
                bool bad_index = (*endptr != '\0' || index < 0);
                free(index_str);
                if (bad_index)
                    return NULL;
                clk_json_value* next = clk_json_array_get(current, (size_t)index);
                if (!next)
                    return NULL;
                current = next;
                /* skip closing ']' */
                pos++;
                break;
            }
            default: {
                /* bare key (not preceded by '.') */
                if (current->type != JSON_OBJECT)
                    return NULL;
                size_t start = pos - 1;
                while (path[pos] && path[pos] != '.' && path[pos] != '[')
                    pos++;
                char* key = clk_json_strndup(path + start, pos - start);
                if (!key)
                    return NULL;
                clk_json_value* next = clk_json_object_get(current, key);
                free(key);
                if (!next)
                    return NULL;
                current = next;
                break;
            }
        }
    }
}
