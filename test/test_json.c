#include "clk_json.h"
#include "test_utils.h"

int main(void) {
    if (isatty_fd(fileno(stdout))) {
        g_pass = "  \033[32m[PASS]\033[0m ";
        g_fail = "  \033[31m[FAIL]\033[0m ";
    }

    clk_json_value* v;
    double num_val;
    const char* str_val;

    /* === Create === */
    v = clk_json_create_null();
    TEST("create_null non-null", v != NULL);
    TEST("create_null type", clk_json_get_type(v) == JSON_NULL);
    TEST("is_null true", clk_json_is_null(v));
    clk_json_free(v);

    v = clk_json_create_number(42.5);
    TEST("create_number type", clk_json_is_number(v));
    TEST("create_number get succeeds", clk_json_get_number(v, &num_val) == 0);
    TEST("create_number value", num_val == 42.5);
    clk_json_free(v);

    v = clk_json_create_string("hello");
    TEST("create_string type", clk_json_is_string(v));
    clk_json_get_string(v, &str_val);
    TEST("create_string value", strcmp(str_val, "hello") == 0);
    clk_json_free(v);

    v = clk_json_create_true();
    TEST("create_true", clk_json_is_true(v));
    clk_json_free(v);

    v = clk_json_create_false();
    TEST("create_false", clk_json_is_false(v));
    clk_json_free(v);

    v = clk_json_create_array();
    TEST("create_array empty", clk_json_array_count(v) == 0);
    clk_json_free(v);

    v = clk_json_create_object();
    TEST("create_object empty", clk_json_object_count(v) == 0);
    clk_json_free(v);

    /* === Parse —— 简单值 === */
    v = clk_json_parse("null");
    TEST("parse null", v != NULL && clk_json_is_null(v));
    clk_json_free(v);

    v = clk_json_parse("true");
    TEST("parse true", clk_json_is_true(v));
    clk_json_free(v);

    v = clk_json_parse("false");
    TEST("parse false", clk_json_is_false(v));
    clk_json_free(v);

    v = clk_json_parse("123");
    TEST("parse int", clk_json_is_number(v));
    clk_json_get_number(v, &num_val);
    TEST("parse int value", num_val == 123.0);
    clk_json_free(v);

    v = clk_json_parse("-5");
    clk_json_get_number(v, &num_val);
    TEST("parse negative", num_val == -5.0);
    clk_json_free(v);

    v = clk_json_parse("\"hello\"");
    clk_json_get_string(v, &str_val);
    TEST("parse string", strcmp(str_val, "hello") == 0);
    clk_json_free(v);

    v = clk_json_parse("\"Hi \\u0041\"");
    clk_json_get_string(v, &str_val);
    TEST("parse unicode \\u0041", strcmp(str_val, "Hi A") == 0);
    clk_json_free(v);

    v = clk_json_parse("\"a\\nb\"");
    clk_json_get_string(v, &str_val);
    TEST("parse \\n", strcmp(str_val, "a\nb") == 0);
    clk_json_free(v);

    v = clk_json_parse("\"hello\\tworld\"");
    clk_json_get_string(v, &str_val);
    TEST("parse \\t", strcmp(str_val, "hello\tworld") == 0);
    clk_json_free(v);

    /* === Parse —— 对象 === */
    v = clk_json_parse("{}");
    TEST_REQUIRE("parse empty obj", v != NULL && clk_json_is_object(v));
    TEST("empty obj count 0", clk_json_object_count(v) == 0);
    clk_json_free(v);

    v = clk_json_parse("{\"a\":1}");
    TEST_REQUIRE("parse simple obj", v != NULL);
    TEST("simple obj count 1", clk_json_object_count(v) == 1);
    {
        clk_json_value* a = clk_json_object_get(v, "a");
        TEST_REQUIRE("simple obj get 'a'", a != NULL);
        clk_json_get_number(a, &num_val);
        TEST("simple obj 'a' value", num_val == 1.0);
    }
    clk_json_free(v);

    v = clk_json_parse("{\"x\":10,\"y\":false,\"z\":\"end\"}");
    TEST_REQUIRE("multi-key obj", v != NULL);
    TEST("multi-key count 3", clk_json_object_count(v) == 3);
    {
        clk_json_value* z = clk_json_object_get(v, "z");
        clk_json_get_string(z, &str_val);
        TEST("multi-key 'z' value", strcmp(str_val, "end") == 0);
    }
    clk_json_free(v);

    v = clk_json_parse("{\"outer\":{\"inner\":99}}");
    TEST_REQUIRE("nested obj", v != NULL);
    {
        clk_json_value* outer = clk_json_object_get(v, "outer");
        TEST_REQUIRE("nested 'outer' exists", outer != NULL && clk_json_is_object(outer));
        clk_json_value* inner = clk_json_object_get(outer, "inner");
        clk_json_get_number(inner, &num_val);
        TEST("nested 'inner' value", num_val == 99.0);
    }
    clk_json_free(v);

    /* === Parse —— 数组 === */
    v = clk_json_parse("[]");
    TEST_REQUIRE("parse empty array", v != NULL);
    TEST("empty array count 0", clk_json_array_count(v) == 0);
    clk_json_free(v);

    v = clk_json_parse("[1, 2, 3]");
    TEST_REQUIRE("simple array", v != NULL);
    TEST("simple array count 3", clk_json_array_count(v) == 3);
    clk_json_get_number(clk_json_array_get(v, 0), &num_val);
    TEST("array[0] == 1", num_val == 1.0);
    clk_json_get_number(clk_json_array_get(v, 2), &num_val);
    TEST("array[2] == 3", num_val == 3.0);
    clk_json_free(v);

    v = clk_json_parse("[{\"id\":1},{\"id\":2}]");
    TEST_REQUIRE("array of objects", v != NULL);
    TEST("array of objs count 2", clk_json_array_count(v) == 2);
    {
        clk_json_value* first = clk_json_array_get(v, 0);
        clk_json_get_number(clk_json_object_get(first, "id"), &num_val);
        TEST("array[0].id == 1", num_val == 1.0);
    }
    clk_json_free(v);

    v = clk_json_parse("[10, \"str\", true, null]");
    TEST_REQUIRE("mixed array", v != NULL);
    TEST("mixed array[0] number", clk_json_is_number(clk_json_array_get(v, 0)));
    TEST("mixed array[1] string", clk_json_is_string(clk_json_array_get(v, 1)));
    TEST("mixed array[2] true", clk_json_is_true(clk_json_array_get(v, 2)));
    TEST("mixed array[3] null", clk_json_is_null(clk_json_array_get(v, 3)));
    clk_json_free(v);

    /* === Parse —— 错误 === */
    v = clk_json_parse("");
    TEST("empty string fails", v == NULL);

    v = clk_json_parse("not json");
    TEST("invalid input fails", v == NULL);

    v = clk_json_parse("{bad}");
    TEST("bad object fails", v == NULL);

    v = clk_json_parse("[1, 2,]");
    TEST("trailing comma fails", v == NULL);

    char err[256] = {0};
    v = clk_json_parse_ex("{bad}", err, sizeof(err));
    TEST("parse_ex invalid fails", v == NULL);
    TEST("parse_ex error non-empty", strlen(err) > 0);

    /* === Object 操作 === */
    v = clk_json_create_object();
    clk_json_object_set(v, "name", clk_json_create_string("clock"));
    TEST("object set new key", clk_json_object_count(v) == 1);

    clk_json_object_set(v, "name", clk_json_create_string("watch"));
    {
        clk_json_value* n = clk_json_object_get(v, "name");
        clk_json_get_string(n, &str_val);
        TEST("object set overwrite", strcmp(str_val, "watch") == 0);
    }

    clk_json_object_remove(v, "name");
    TEST("object remove", clk_json_object_count(v) == 0);
    clk_json_free(v);

    v = clk_json_parse("{\"x\":1,\"y\":2,\"z\":3}");
    TEST_REQUIRE("iterator parse obj", v != NULL);
    {
        clk_json_object_iterator iter;
        clk_json_object_iterator_init(v, &iter);
        clk_json_key_value_pair pair;
        int count = 0;
        while (clk_json_object_iterator_next(&iter, &pair))
            count++;
        TEST("iterator visits all keys", count == 3);
    }
    clk_json_free(v);

    /* === Array 操作 === */
    v = clk_json_create_array();
    clk_json_array_append(v, clk_json_create_number(1));
    clk_json_array_append(v, clk_json_create_number(3));
    clk_json_array_insert(v, 1, clk_json_create_number(2));
    TEST("array insert count 3", clk_json_array_count(v) == 3);
    clk_json_get_number(clk_json_array_get(v, 0), &num_val);
    TEST("insert [0] remains 1", num_val == 1.0);
    clk_json_get_number(clk_json_array_get(v, 1), &num_val);
    TEST("insert [1] is 2", num_val == 2.0);
    clk_json_get_number(clk_json_array_get(v, 2), &num_val);
    TEST("insert [2] is 3", num_val == 3.0);
    clk_json_free(v);

    v = clk_json_create_array();
    clk_json_array_append(v, clk_json_create_number(10));
    clk_json_array_append(v, clk_json_create_number(20));
    clk_json_array_remove(v, 0);
    TEST("array remove count 1", clk_json_array_count(v) == 1);
    clk_json_get_number(clk_json_array_get(v, 0), &num_val);
    TEST("array remove shift to 20", num_val == 20.0);
    clk_json_free(v);

    v = clk_json_create_array();
    clk_json_array_append(v, clk_json_create_number(5));
    clk_json_array_set(v, 0, clk_json_create_number(99));
    clk_json_get_number(clk_json_array_get(v, 0), &num_val);
    TEST("array set value", num_val == 99.0);
    clk_json_free(v);

    /* === Stringify === */
    v = clk_json_parse("null");
    char* s = clk_json_stringify(v);
    TEST("stringify null", s != NULL && strcmp(s, "null") == 0);
    free(s);
    clk_json_free(v);

    v = clk_json_parse("true");
    s = clk_json_stringify(v);
    TEST("stringify true", strcmp(s, "true") == 0);
    free(s);
    clk_json_free(v);

    v = clk_json_parse("false");
    s = clk_json_stringify(v);
    TEST("stringify false", strcmp(s, "false") == 0);
    free(s);
    clk_json_free(v);

    v = clk_json_parse("42.5");
    s = clk_json_stringify(v);
    TEST("stringify number", strcmp(s, "42.5") == 0);
    free(s);
    clk_json_free(v);

    v = clk_json_parse("\"hello\"");
    s = clk_json_stringify(v);
    TEST("stringify string", strcmp(s, "\"hello\"") == 0);
    free(s);
    clk_json_free(v);

    v = clk_json_parse("[1,2,3]");
    s = clk_json_stringify(v);
    TEST("stringify array", strcmp(s, "[1,2,3]") == 0);
    free(s);
    clk_json_free(v);

    v = clk_json_parse("{\"a\":1,\"b\":2}");
    s = clk_json_stringify(v);
    TEST("stringify object", s != NULL);
    /* 解析 stringify 的输出应该得到同样的结构 */
    clk_json_value* v2 = clk_json_parse(s);
    TEST("stringify roundtrip parse OK", v2 != NULL);
    TEST("stringify roundtrip has 'a'", clk_json_object_get(v2, "a") != NULL);
    clk_json_get_number(clk_json_object_get(v2, "a"), &num_val);
    TEST("stringify roundtrip 'a' value", num_val == 1.0);
    clk_json_free(v2);
    free(s);
    clk_json_free(v);

    /* 转义字符往返 */
    v = clk_json_parse("\"line1\\nline2\"");
    s = clk_json_stringify(v);
    TEST("stringify escaped", s != NULL);
    v2 = clk_json_parse(s);
    const char* escaped_val;
    clk_json_get_string(v2, &escaped_val);
    TEST("stringify roundtrip escaped", strcmp(escaped_val, "line1\nline2") == 0);
    clk_json_free(v2);
    free(s);
    clk_json_free(v);

    /* === Pretty-print —— 格式化验证 === */

    /* 空对象 / 空数组 —— 永远一行 */
    v = clk_json_parse("{}");
    s = clk_json_stringify_pretty(v);
    TEST("pretty empty obj", strcmp(s, "{}") == 0);
    free(s); clk_json_free(v);

    v = clk_json_parse("[]");
    s = clk_json_stringify_pretty(v);
    TEST("pretty empty array", strcmp(s, "[]") == 0);
    free(s); clk_json_free(v);

    /* 简单对象 —— 一行一个键值对，2空格缩进 */
    v = clk_json_parse("{\"a\":1}");
    s = clk_json_stringify_pretty(v);
    TEST("pretty simple obj exact", s && strcmp(s, "{\n  \"a\": 1\n}") == 0);
    free(s); clk_json_free(v);

    /* 多键值对 —— 每个独立一行，最后不带逗号 */
    v = clk_json_parse("{\"x\":1,\"y\":2,\"z\":3}");
    s = clk_json_stringify_pretty(v);
    TEST("pretty multi-key obj exact",
         s && strcmp(s,
                      "{\n"
                      "  \"x\": 1,\n"
                      "  \"y\": 2,\n"
                      "  \"z\": 3\n"
                      "}") == 0);
    free(s); clk_json_free(v);

    /* 全是基本值的数组 —— 不换行 */
    v = clk_json_parse("[1,2,3]");
    s = clk_json_stringify_pretty(v);
    TEST("pretty primitive array inline", strcmp(s, "[1,2,3]") == 0);
    free(s); clk_json_free(v);

    /* 数组里有一个容器孩子 —— 换行 */
    v = clk_json_parse("[1,[2],3]");
    s = clk_json_stringify_pretty(v);
    TEST("pretty array with nested inline",
         s && strcmp(s,
                      "[\n"
                      "  1,\n"
                      "  [2],\n"
                      "  3\n"
                      "]") == 0);
    free(s); clk_json_free(v);

    /* 纯容器孩子数组 —— 全换行 */
    v = clk_json_parse("[[1,2],[3,4]]");
    s = clk_json_stringify_pretty(v);
    TEST("pretty array all containers",
         s && strcmp(s,
                      "[\n"
                      "  [1,2],\n"
                      "  [3,4]\n"
                      "]") == 0);
    free(s); clk_json_free(v);

    /* 双层嵌套对象 —— 内层缩进 4 空格 */
    v = clk_json_parse("{\"outer\":{\"inner\":99}}");
    s = clk_json_stringify_pretty(v);
    TEST("pretty nested object depth 2",
         s && strcmp(s,
                      "{\n"
                      "  \"outer\": {\n"
                      "    \"inner\": 99\n"
                      "  }\n"
                      "}") == 0);
    free(s); clk_json_free(v);

    /* 三层嵌套 —— 内层 6 空格 */
    v = clk_json_parse("{\"a\":{\"b\":{\"c\":3}}}");
    s = clk_json_stringify_pretty(v);
    TEST("pretty nested object depth 3",
         s && strcmp(s,
                      "{\n"
                      "  \"a\": {\n"
                      "    \"b\": {\n"
                      "      \"c\": 3\n"
                      "    }\n"
                      "  }\n"
                      "}") == 0);
    free(s); clk_json_free(v);

    /* 对象中含数组 —— 数组换行，内层基本值不换行 */
    v = clk_json_parse("{\"nums\":[1,2,3],\"flag\":true}");
    s = clk_json_stringify_pretty(v);
    TEST("pretty obj with array",
         s && strcmp(s,
                      "{\n"
                      "  \"nums\": [1,2,3],\n"
                      "  \"flag\": true\n"
                      "}") == 0);
    free(s); clk_json_free(v);

    /* 数组中含对象 —— 每个对象换行内联 */
    v = clk_json_parse("[{\"id\":1},{\"id\":2}]");
    s = clk_json_stringify_pretty(v);
    TEST("pretty array of objects",
         s && strcmp(s,
                      "[\n"
                      "  {\n"
                      "    \"id\": 1\n"
                      "  },\n"
                      "  {\n"
                      "    \"id\": 2\n"
                      "  }\n"
                      "]") == 0);
    free(s); clk_json_free(v);

    /* 混合类型 —— null / false / 字符串 */
    v = clk_json_parse("[null,false,\"hi\"]");
    s = clk_json_stringify_pretty(v);
    TEST("pretty mixed primitives inline", strcmp(s, "[null,false,\"hi\"]") == 0);
    free(s); clk_json_free(v);

    /* 往返测试 —— 格式化后数据不丢 */
    v = clk_json_parse("{\"a\":1,\"b\":[2,{\"c\":3}]}");
    s = clk_json_stringify_pretty(v);
    v2 = clk_json_parse(s);
    TEST("pretty roundtrip parse OK", v2 != NULL);
    TEST("pretty roundtrip has 'a'", clk_json_object_get(v2, "a") != NULL);
    clk_json_get_number(clk_json_object_get(v2, "a"), &num_val);
    TEST("pretty roundtrip 'a' value", num_val == 1.0);
    clk_json_value* b = clk_json_object_get(v2, "b");
    TEST("pretty roundtrip has 'b' array", b != NULL && clk_json_is_array(b));
    TEST("pretty roundtrip 'b' count 2", clk_json_array_count(b) == 2);
    clk_json_free(v2);
    free(s);
    clk_json_free(v);

    /* === Deep Copy === */
    v = clk_json_parse("{\"a\":1,\"b\":[2,{\"c\":3}],\"d\":\"hello\"}");
    clk_json_value* cp = clk_json_deep_copy(v);
    TEST("deep_copy non-null", cp != NULL);
    TEST("deep_copy equals original", clk_json_equals(v, cp));
    clk_json_object_set(cp, "a", clk_json_create_number(999));
    clk_json_get_number(clk_json_object_get(v, "a"), &num_val);
    TEST("deep_copy original untouched", num_val == 1.0);
    clk_json_free(cp);
    clk_json_free(v);
    cp = clk_json_deep_copy(NULL);
    TEST("deep_copy NULL returns NULL", cp == NULL);

    /* === Merge Objects === */
    clk_json_value* dest = clk_json_create_object();
    clk_json_object_set(dest, "a", clk_json_create_number(1));
    clk_json_value* src = clk_json_parse("{\"b\":2,\"c\":3}");
    int rc = clk_json_merge_objects(dest, src);
    TEST("merge returns 0", rc == 0);
    TEST("merge count 3", clk_json_object_count(dest) == 3);
    TEST("merge keeps 'a'",
         clk_json_is_number(clk_json_object_get(dest, "a")));
    TEST("merge adds 'b'",
         clk_json_is_number(clk_json_object_get(dest, "b")));
    /* overwrite */
    clk_json_value* src2 = clk_json_parse("{\"a\":99}");
    rc = clk_json_merge_objects(dest, src2);
    TEST("merge overwrite ok", rc == 0);
    clk_json_get_number(clk_json_object_get(dest, "a"), &num_val);
    TEST("merge overwrite value", num_val == 99.0);
    clk_json_free(src2);
    clk_json_free(dest);
    clk_json_free(src);

    /* merge non-objects fails */
    clk_json_value* num_node = clk_json_create_number(1);
    TEST("merge non-object fails", clk_json_merge_objects(num_node, num_node) == -1);
    clk_json_free(num_node);

    /* === Get By Path === */
    v = clk_json_parse("{\"users\":[{\"name\":\"Alice\",\"age\":30},{\"name\":\"Bob\",\"age\":25}],\"count\":2}");
    TEST_REQUIRE("path parse root", v != NULL);

    /* 简单键名 */
    clk_json_value* r = clk_json_get_by_path(v, "count");
    TEST("path simple key", r != NULL && clk_json_is_number(r));
    clk_json_get_number(r, &num_val);
    TEST("path simple key value", num_val == 2.0);

    /* 一层嵌套对象 */
    r = clk_json_get_by_path(v, "users");
    TEST("path nested object", r != NULL && clk_json_is_array(r));

    /* 对象 + 数组索引 */
    r = clk_json_get_by_path(v, "users[0]");
    TEST_REQUIRE("path obj+index", r != NULL && clk_json_is_object(r));
    r = clk_json_get_by_path(r, "name");
    TEST("path obj+index sub-key", r != NULL);

    /* 对象 + 数组索引 + 键名（一条路径） */
    r = clk_json_get_by_path(v, "users[0].name");
    TEST_REQUIRE("path obj+index+key", r != NULL && clk_json_is_string(r));
    clk_json_get_string(r, &str_val);
    TEST("path obj+index+key value", strcmp(str_val, "Alice") == 0);

    /* 第二个元素 */
    r = clk_json_get_by_path(v, "users[1].age");
    TEST_REQUIRE("path second element", r != NULL);
    clk_json_get_number(r, &num_val);
    TEST("path second element value", num_val == 25.0);

    /* 不存在返回 NULL */
    r = clk_json_get_by_path(v, "missing");
    TEST("path missing key", r == NULL);
    r = clk_json_get_by_path(v, "users[99]");
    TEST("path out of bounds", r == NULL);
    r = clk_json_get_by_path(v, "users[0].bad");
    TEST("path missing sub-key", r == NULL);

    /* 空路径返回 root */
    r = clk_json_get_by_path(v, "");
    TEST("path empty returns root", r == v);

    /* 从数组开始 */
    clk_json_value* arr = clk_json_parse("[10,20,30]");
    r = clk_json_get_by_path(arr, "[0]");
    TEST_REQUIRE("path array root [0]", r != NULL);
    clk_json_get_number(r, &num_val);
    TEST("path array root [0] value", num_val == 10.0);
    r = clk_json_get_by_path(arr, "[2]");
    clk_json_get_number(r, &num_val);
    TEST("path array root [2] value", num_val == 30.0);
    clk_json_free(arr);

    /* 非法路径 */
    r = clk_json_get_by_path(v, "users[abc]");
    TEST("path bad index non-numeric", r == NULL);
    r = clk_json_get_by_path(v, "users[-1]");
    TEST("path bad index negative", r == NULL);

    /* NULL 输入 */
    TEST("path NULL path", clk_json_get_by_path(v, NULL) == NULL);
    TEST("path NULL root", clk_json_get_by_path(NULL, "a") == NULL);

    clk_json_free(v);

test_cleanup:
    printf("\n%d/%d passed\n", test_total - test_failed, test_total);
    return test_failed > 0 ? 1 : 0;
}
