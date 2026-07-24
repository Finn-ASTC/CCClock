#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "clk_json.h"

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    char* s = malloc(size + 1);
    if (!s)
        return 0;
    memcpy(s, data, size);
    s[size] = '\0';

    clk_json_value* v = clk_json_parse(s);
    if (v) {
        char* compact = clk_json_stringify(v);
        free(compact);

        char* pretty = clk_json_stringify_pretty(v);
        free(pretty);

        clk_json_value* cp = clk_json_deep_copy(v);
        if (cp) {
            clk_json_equals(v, cp);
            clk_json_free(cp);
        }

        clk_json_free(v);
    }
    free(s);
    return 0;
}
