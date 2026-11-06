#include "minitest.h"

#include <limits.h>

#include "json_reader.h"

static JsonReader reader;

static JsonReader* open_text(const char* text) {
    json_reader_init(&reader, text, strlen(text));
    return &reader;
}

static void test_flat_object(void) {
    JsonReader* r = open_text(" { \"s\" : \"abc\" , \"n\":-42,\"b\":true,\"f\":false } \r\n");
    char key[16];
    char s[8];
    long n = 0;
    bool b = false;
    bool f = true;
    CHECK(json_object_begin(r));
    CHECK(json_object_next(r, key, sizeof(key)));
    CHECK_STR_EQ(key, "s");
    CHECK(json_read_string(r, s, sizeof(s)));
    CHECK_STR_EQ(s, "abc");
    CHECK(json_object_next(r, key, sizeof(key)));
    CHECK_STR_EQ(key, "n");
    CHECK(json_read_int(r, &n));
    CHECK_EQ_INT(n, -42);
    CHECK(json_object_next(r, key, sizeof(key)));
    CHECK_STR_EQ(key, "b");
    CHECK(json_read_bool(r, &b));
    CHECK(b);
    CHECK(json_object_next(r, key, sizeof(key)));
    CHECK_STR_EQ(key, "f");
    CHECK(json_read_bool(r, &f));
    CHECK(!f);
    CHECK(!json_object_next(r, key, sizeof(key)));
    CHECK(json_reader_ok(r));
    CHECK(json_reader_finish(r));
}

static void test_empty_containers(void) {
    char key[4];
    JsonReader* r = open_text("{}");
    CHECK(json_object_begin(r));
    CHECK(!json_object_next(r, key, sizeof(key)));
    CHECK(json_reader_finish(r));

    r = open_text("[ ]");
    CHECK(json_array_begin(r));
    CHECK(!json_array_next(r));
    CHECK(json_reader_finish(r));
}

static void test_array_of_strings(void) {
    JsonReader* r = open_text("[\"a\", \"bc\",\"\"]");
    char s[4];
    CHECK(json_array_begin(r));
    CHECK(json_array_next(r));
    CHECK(json_read_string(r, s, sizeof(s)));
    CHECK_STR_EQ(s, "a");
    CHECK(json_array_next(r));
    CHECK(json_read_string(r, s, sizeof(s)));
    CHECK_STR_EQ(s, "bc");
    CHECK(json_array_next(r));
    CHECK(json_read_string(r, s, sizeof(s)));
    CHECK_STR_EQ(s, "");
    CHECK(!json_array_next(r));
    CHECK(json_reader_finish(r));
}

static void test_peek(void) {
    CHECK(json_peek(open_text("  {")) == JsonTypeObject);
    CHECK(json_peek(open_text("[")) == JsonTypeArray);
    CHECK(json_peek(open_text("\"x\"")) == JsonTypeString);
    CHECK(json_peek(open_text("-1")) == JsonTypeNumber);
    CHECK(json_peek(open_text("7")) == JsonTypeNumber);
    CHECK(json_peek(open_text("true")) == JsonTypeBool);
    CHECK(json_peek(open_text("false")) == JsonTypeBool);
    CHECK(json_peek(open_text("null")) == JsonTypeNull);
    CHECK(json_peek(open_text("")) == JsonTypeInvalid);
    CHECK(json_peek(open_text("  ")) == JsonTypeInvalid);
    CHECK(json_peek(open_text("x")) == JsonTypeInvalid);
    /* Peeking never puts the reader into the error state. */
    CHECK(json_reader_ok(&reader));
}

static void test_string_escapes(void) {
    char s[32];
    CHECK(json_read_string(open_text("\"q\\\"b\\\\s\\/\""), s, sizeof(s)));
    CHECK_STR_EQ(s, "q\"b\\s/");
    CHECK(json_read_string(open_text("\"\\u0041\\u007a\""), s, sizeof(s)));
    CHECK_STR_EQ(s, "Az");
}

static void test_string_non_ascii_becomes_question_mark(void) {
    char s[32];
    /* Control characters and non-ASCII code points become one '?' each,
     * whether escaped or raw UTF-8; a surrogate pair is one code point. */
    CHECK(json_read_string(open_text("\"a\\nb\\t\\u0000\""), s, sizeof(s)));
    CHECK_STR_EQ(s, "a?b??");
    CHECK(json_read_string(open_text("\"caf\\u00e9 \\ud83d\\ude00!\""), s, sizeof(s)));
    CHECK_STR_EQ(s, "caf? ?!");
    CHECK(json_read_string(open_text("\"caf\xC3\xA9 \xF0\x9F\x98\x80!\""), s, sizeof(s)));
    CHECK_STR_EQ(s, "caf? ?!");
    CHECK(json_read_string(open_text("\"\\ud83d\\u0041\""), s, sizeof(s)));
    CHECK_STR_EQ(s, "?A");
    CHECK(json_read_string(open_text("\"\\ude00\""), s, sizeof(s)));
    CHECK_STR_EQ(s, "?");
}

static void test_string_errors(void) {
    char s[4];
    /* "abc" fits a 4-byte buffer, "abcd" does not. */
    CHECK(json_read_string(open_text("\"abc\""), s, sizeof(s)));
    CHECK(!json_read_string(open_text("\"abcd\""), s, sizeof(s)));
    CHECK(!json_reader_ok(&reader));
    /* Multi-byte UTF-8 counts as one char. */
    CHECK(json_read_string(open_text("\"\xC3\xA9\xC3\xA9\xC3\xA9\""), s, sizeof(s)));
    CHECK_STR_EQ(s, "???");

    CHECK(!json_read_string(open_text("\"abc"), s, sizeof(s)));
    CHECK(!json_read_string(open_text("\"a\\"), s, sizeof(s)));
    CHECK(!json_read_string(open_text("\"\\x\""), s, sizeof(s)));
    CHECK(!json_read_string(open_text("\"\\u12\""), s, sizeof(s)));
    CHECK(!json_read_string(open_text("\"\\u12G4\""), s, sizeof(s)));
    CHECK(!json_read_string(open_text("\"a\nb\""), s, sizeof(s)));
    CHECK(!json_read_string(open_text("abc"), s, sizeof(s)));
    CHECK(!json_read_string(open_text("12"), s, sizeof(s)));
}

static void test_string_respects_length(void) {
    /* The text length bounds the reader, not a NUL terminator. */
    char s[8];
    json_reader_init(&reader, "\"abc\"", 4);
    CHECK(!json_read_string(&reader, s, sizeof(s)));
}

static void test_ints(void) {
    long n = 1;
    CHECK(json_read_int(open_text("0"), &n));
    CHECK_EQ_INT(n, 0);
    CHECK(json_read_int(open_text("-0"), &n));
    CHECK_EQ_INT(n, 0);
    CHECK(json_read_int(open_text("9999"), &n));
    CHECK_EQ_INT(n, 9999);
    CHECK(json_read_int(open_text("2147483647"), &n));
    CHECK_EQ_INT(n, 2147483647L);
    CHECK(json_read_int(open_text("-2147483648"), &n));
    CHECK_EQ_INT(n, -2147483647L - 1);

    CHECK(!json_read_int(open_text("99999999999999999999999"), &n));
    CHECK(!json_read_int(open_text("-99999999999999999999999"), &n));
    CHECK(!json_read_int(open_text("01"), &n));
    CHECK(!json_read_int(open_text("-"), &n));
    CHECK(!json_read_int(open_text("+1"), &n));
    CHECK(!json_read_int(open_text("1.5"), &n));
    CHECK(!json_read_int(open_text("1e3"), &n));
    CHECK(!json_read_int(open_text("\"1\""), &n));
    CHECK(!json_read_int(open_text("true"), &n));
    CHECK(!json_read_int(open_text(""), &n));
}

static void test_bools(void) {
    bool b = false;
    CHECK(json_read_bool(open_text("true"), &b));
    CHECK(b);
    CHECK(!json_read_bool(open_text("tru"), &b));
    CHECK(!json_read_bool(open_text("null"), &b));
    CHECK(!json_read_bool(open_text("1"), &b));
    CHECK(!json_read_bool(open_text("\"true\""), &b));
}

static void test_skip_any_value(void) {
    char key[8];
    long n = 0;
    JsonReader* r = open_text(
        "{\"x\":{\"a\":[1,-2.5e+3,0.25,1E9,null,true,false,\"s\\\"\",{},[]],\"b\":{}},\"n\":7}");
    CHECK(json_object_begin(r));
    CHECK(json_object_next(r, key, sizeof(key)));
    CHECK_STR_EQ(key, "x");
    CHECK(json_skip(r));
    CHECK(json_object_next(r, key, sizeof(key)));
    CHECK_STR_EQ(key, "n");
    CHECK(json_read_int(r, &n));
    CHECK_EQ_INT(n, 7);
    CHECK(!json_object_next(r, key, sizeof(key)));
    CHECK(json_reader_finish(r));

    CHECK(!json_skip(open_text("1.")));
    CHECK(!json_skip(open_text("1e")));
    CHECK(!json_skip(open_text(".5")));
    CHECK(!json_skip(open_text("nul")));
    CHECK(!json_skip(open_text("[1,]")));
    CHECK(!json_skip(open_text("{\"a\":1,}")));
    CHECK(!json_skip(open_text("{\"a\" 1}")));
    CHECK(!json_skip(open_text("{\"a\":1 \"b\":2}")));
    CHECK(!json_skip(open_text("[1 2]")));
    CHECK(!json_skip(open_text("{,}")));
    CHECK(!json_skip(open_text("[,1]")));
    CHECK(!json_skip(open_text("{1:2}")));
    CHECK(!json_skip(open_text("[1")));
    CHECK(!json_skip(open_text("{\"a\":")));
    CHECK(!json_skip(open_text("]")));
}

static void test_depth_limit(void) {
    char text[64];
    size_t n = 0;
    for(int i = 0; i < JSON_READER_MAX_DEPTH; i++)
        text[n++] = '[';
    for(int i = 0; i < JSON_READER_MAX_DEPTH; i++)
        text[n++] = ']';
    text[n] = '\0';
    CHECK(json_skip(open_text(text)));
    CHECK(json_reader_finish(&reader));

    n = 0;
    for(int i = 0; i <= JSON_READER_MAX_DEPTH; i++)
        text[n++] = '[';
    for(int i = 0; i <= JSON_READER_MAX_DEPTH; i++)
        text[n++] = ']';
    text[n] = '\0';
    CHECK(!json_skip(open_text(text)));
}

static void test_overlong_key_reads_as_empty(void) {
    char key[4];
    long n = 0;
    JsonReader* r = open_text("{\"abcd\":1,\"abc\":2}");
    CHECK(json_object_begin(r));
    CHECK(json_object_next(r, key, sizeof(key)));
    CHECK_STR_EQ(key, "");
    CHECK(json_read_int(r, &n));
    CHECK(json_object_next(r, key, sizeof(key)));
    CHECK_STR_EQ(key, "abc");
    CHECK(json_read_int(r, &n));
    CHECK_EQ_INT(n, 2);
    CHECK(!json_object_next(r, key, sizeof(key)));
    CHECK(json_reader_finish(r));
}

static void test_errors_are_sticky(void) {
    long n = 0;
    char key[4];
    JsonReader* r = open_text("{\"a\":x,\"b\":1}");
    CHECK(json_object_begin(r));
    CHECK(json_object_next(r, key, sizeof(key)));
    CHECK(!json_read_int(r, &n));
    CHECK(!json_reader_ok(r));
    CHECK(!json_object_next(r, key, sizeof(key)));
    CHECK(!json_skip(r));
    CHECK(!json_reader_finish(r));
}

static void test_finish_rejects_trailing_content(void) {
    long n = 0;
    CHECK(json_read_int(open_text("1 "), &n));
    CHECK(json_reader_finish(&reader));
    CHECK(json_read_int(open_text("1 2"), &n));
    CHECK(!json_reader_finish(&reader));
    CHECK(json_skip(open_text("{}x")));
    CHECK(!json_reader_finish(&reader));
    CHECK(json_skip(open_text("truex")));
    CHECK(!json_reader_finish(&reader));
}

static void test_container_type_mismatch(void) {
    CHECK(!json_object_begin(open_text("[]")));
    CHECK(!json_array_begin(open_text("{}")));
    CHECK(!json_object_begin(open_text("")));
}

int main(void) {
    RUN_TEST(test_flat_object);
    RUN_TEST(test_empty_containers);
    RUN_TEST(test_array_of_strings);
    RUN_TEST(test_peek);
    RUN_TEST(test_string_escapes);
    RUN_TEST(test_string_non_ascii_becomes_question_mark);
    RUN_TEST(test_string_errors);
    RUN_TEST(test_string_respects_length);
    RUN_TEST(test_ints);
    RUN_TEST(test_bools);
    RUN_TEST(test_skip_any_value);
    RUN_TEST(test_depth_limit);
    RUN_TEST(test_overlong_key_reads_as_empty);
    RUN_TEST(test_errors_are_sticky);
    RUN_TEST(test_finish_rejects_trailing_content);
    RUN_TEST(test_container_type_mismatch);
    return MINITEST_RESULT();
}
