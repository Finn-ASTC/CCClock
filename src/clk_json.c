#include "clk_json.h"

#include <stdbool.h>
#include <stddef.h>

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
    TOKEN_EOF             // 输入结束
} clk_json_token_type;

typedef struct {
    clk_json_token_type type;
    char* str_value;
    double num_value;
    int line, col;
} clk_json_token;

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
