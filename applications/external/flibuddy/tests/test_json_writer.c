#include "minitest.h"

#include <limits.h>

#include "json_writer.h"

static void test_nested_object_and_array_commas(void) {
    char buf[128];
    JsonWriter w;
    json_init(&w, buf, sizeof(buf));
    json_object_open(&w);
    json_key(&w, "a");
    json_int(&w, 1);
    json_key(&w, "b");
    json_array_open(&w);
    json_int(&w, 2);
    json_bool(&w, true);
    json_object_open(&w);
    json_object_close(&w);
    json_array_open(&w);
    json_array_close(&w);
    json_array_close(&w);
    json_key(&w, "c");
    json_bool(&w, false);
    CHECK(json_object_close(&w));
    CHECK(json_ok(&w));
    CHECK_STR_EQ(buf, "{\"a\":1,\"b\":[2,true,{},[]],\"c\":false}");
    CHECK_EQ_INT(json_len(&w), strlen(buf));
}

static void test_integers(void) {
    char buf[128];
    JsonWriter w;
    json_init(&w, buf, sizeof(buf));
    json_array_open(&w);
    json_int(&w, 0);
    json_int(&w, 7);
    json_int(&w, -42);
    json_int(&w, 9999);
    json_int(&w, LONG_MAX);
    json_int(&w, LONG_MIN);
    json_array_close(&w);
    char expected[128];
    snprintf(expected, sizeof(expected), "[0,7,-42,9999,%ld,%ld]", LONG_MAX, LONG_MIN);
    CHECK_STR_EQ(buf, expected);
}

static void test_string_escaping(void) {
    char buf[128];
    JsonWriter w;
    json_init(&w, buf, sizeof(buf));
    json_string(&w, "q\"b\\s/n\nr\rt\t\x01\x1f\x7f");
    CHECK(json_ok(&w));
    CHECK_STR_EQ(buf, "\"q\\\"b\\\\s/n\\nr\\rt\\t\\u0001\\u001F\x7f\"");
}

static void test_non_ascii_bytes_become_question_marks(void) {
    char buf[64];
    JsonWriter w;
    json_init(&w, buf, sizeof(buf));
    json_string(&w, "caf\xc3\xa9");
    CHECK_STR_EQ(buf, "\"caf??\"");
}

static void test_keys_are_escaped(void) {
    char buf[64];
    JsonWriter w;
    json_init(&w, buf, sizeof(buf));
    json_object_open(&w);
    json_key(&w, "a\"b");
    json_string(&w, "");
    json_object_close(&w);
    CHECK_STR_EQ(buf, "{\"a\\\"b\":\"\"}");
}

static void test_hex_is_upper_case(void) {
    static const uint8_t bytes[] = {0x00, 0x0A, 0xBC, 0xFF, 0x5a};
    char buf[64];
    JsonWriter w;
    json_init(&w, buf, sizeof(buf));
    json_array_open(&w);
    json_hex(&w, bytes, sizeof(bytes));
    json_hex(&w, bytes, 0);
    json_array_close(&w);
    CHECK_STR_EQ(buf, "[\"000ABCFF5A\",\"\"]");
}

static size_t write_sample(char* buf, size_t cap, bool* ok) {
    static const uint8_t bytes[] = {0xDE, 0xAD};
    JsonWriter w;
    json_init(&w, buf, cap);
    json_object_open(&w);
    json_key(&w, "s");
    json_string(&w, "x\"y\n");
    json_key(&w, "n");
    json_int(&w, -12345);
    json_key(&w, "h");
    json_hex(&w, bytes, sizeof(bytes));
    json_key(&w, "l");
    json_array_open(&w);
    json_bool(&w, true);
    json_bool(&w, false);
    json_array_close(&w);
    *ok = json_object_close(&w);
    return json_len(&w);
}

static void test_overflow_at_every_capacity(void) {
    char full[128];
    bool ok;
    size_t len = write_sample(full, sizeof(full), &ok);
    CHECK(ok);
    CHECK_STR_EQ(full, "{\"s\":\"x\\\"y\\n\",\"n\":-12345,\"h\":\"DEAD\",\"l\":[true,false]}");

    /* Every capacity that cannot hold the text plus its NUL fails, never
     * writes past the capacity and leaves a NUL-terminated prefix. */
    for(size_t cap = 0; cap <= len; cap++) {
        char buf[130];
        memset(buf, '#', sizeof(buf));
        size_t got = write_sample(buf + 1, cap, &ok);
        CHECK(!ok);
        CHECK_EQ_INT(got, 0);
        CHECK(buf[0] == '#');
        CHECK(buf[cap + 1] == '#');
        if(cap > 0) {
            CHECK(memchr(buf + 1, '\0', cap) != NULL);
            CHECK(strncmp(buf + 1, full, strlen(buf + 1)) == 0);
        }
    }

    char exact[128];
    CHECK_EQ_INT(write_sample(exact, len + 1, &ok), len);
    CHECK(ok);
    CHECK_STR_EQ(exact, full);
}

static void test_error_is_sticky(void) {
    char buf[4];
    JsonWriter w;
    json_init(&w, buf, sizeof(buf));
    CHECK(!json_string(&w, "too long"));
    CHECK(!json_int(&w, 1));
    CHECK(!json_ok(&w));
    CHECK_EQ_INT(json_len(&w), 0);
}

int main(void) {
    RUN_TEST(test_nested_object_and_array_commas);
    RUN_TEST(test_integers);
    RUN_TEST(test_string_escaping);
    RUN_TEST(test_non_ascii_bytes_become_question_marks);
    RUN_TEST(test_keys_are_escaped);
    RUN_TEST(test_hex_is_upper_case);
    RUN_TEST(test_overflow_at_every_capacity);
    RUN_TEST(test_error_is_sticky);
    return MINITEST_RESULT();
}
