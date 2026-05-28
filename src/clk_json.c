#include "clk_json.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const char* json;
    int pos;
    int line;
    int col;
} clk_json_lexer;

typedef enum {
    TOKEN_LEFT_BRACE,     // {
    TOKEN_RIGHT_BRACE,    // }
    TOKEN_LEFT_BRACKET,   // [
    TOKEN_RIGHT_BRACKET,  // ]
    TOKEN_COLON,          // :
    TOKEN_COMMA,          // ,
    TOKEN_STRING,         // "abc"
    TOKEN_NUMBER,         // 123
    TOKEN_TRUE,           // true
    TOKEN_FALSE,          // false
    TOKEN_NULL,           // null
    TOKEN_EOF,            // 输入结束
    TOKEN_ERROR           // 解析错误
} clk_json_token_type;

typedef struct {
    clk_json_token_type type;
    char* str_value;
    int str_len;
    double num_value;
    int line, col;
} clk_json_token;

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

static void clk_json_lexer_init(clk_json_lexer* lexer, const char* json_str) {
    lexer->json = json_str;
    lexer->pos = 0;
    lexer->line = 1;
    lexer->col = 1;
}

static void clk_json_lexer_next(clk_json_lexer* lexer, clk_json_token* token) {
    if (!token)
        return;

    free(token->str_value);
    memset(token, 0, sizeof(clk_json_token));

    while (1) {
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
            if (strncmp(lexer->json + lexer->pos, "true", 4) == 0 &&
                clk_json_is_delimiter(lexer->json[lexer->pos + 4])) {
                token->type = TOKEN_TRUE;
                lexer->pos += 4;
                lexer->col += 4;
            } else {
                token->type = TOKEN_ERROR;
            }
            break;
        case 'f':
            if (strncmp(lexer->json + lexer->pos, "false", 5) == 0 &&
                clk_json_is_delimiter(lexer->json[lexer->pos + 5])) {
                token->type = TOKEN_FALSE;
                lexer->pos += 5;
                lexer->col += 5;
            } else {
                token->type = TOKEN_ERROR;
            }
            break;
        case 'n':
            if (strncmp(lexer->json + lexer->pos, "null", 4) == 0 &&
                clk_json_is_delimiter(lexer->json[lexer->pos + 4])) {
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
                char c = lexer->json[lexer->pos];

                if (c == '\0' || c == '\n') {
                    token->type = TOKEN_ERROR;
                    goto string_error;
                }

                if (c == '"')
                    break;

                if (c == '\\') {
                    lexer->pos++;
                    lexer->col++;
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

                            if (codepoint >= 0xD800 && codepoint <= 0xDBFF) {
                                if (lexer->json[lexer->pos] == '\\' &&
                                    lexer->json[lexer->pos + 1] == 'u') {
                                    lexer->pos += 2;
                                    lexer->col += 2;
                                    unsigned int low_surrogate = parse_hex4(lexer);
                                    if (low_surrogate >= 0xDC00 && low_surrogate <= 0xDFFF) {
                                        codepoint = 0x10000 + ((codepoint - 0xD800) << 10) +
                                                    (low_surrogate - 0xDC00);
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
                // 无效字符
                token->type = TOKEN_ERROR;
            }
            break;
    }
}

clk_json_value* clk_json_parse(const char* json_str) {
    // todo
}

clk_json_value* clk_json_parse_ex(const char* json_str, char* errbuf, size_t errbuf_size) {
    // todo
}

void clk_json_free(clk_json_value* root) {
    // todo
}

char* clk_json_stringify(const clk_json_value* root) {
    // todo
}

char* clk_json_stringify_pretty(const clk_json_value* root) {
    // todo
}
