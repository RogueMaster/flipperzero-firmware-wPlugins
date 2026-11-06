/* Minimal assert-style test runner for host-compiled unit tests.
 *
 *   static void test_something(void) { CHECK(1 + 1 == 2); }
 *   int main(void) { RUN_TEST(test_something); return MINITEST_RESULT(); }
 *
 * A failed CHECK reports file:line and keeps going; MINITEST_RESULT prints
 * the totals and returns the process exit status.
 */
#pragma once

#include <stdio.h>
#include <string.h>

static int minitest_checks;
static int minitest_failures;
static int minitest_tests;

#define MINITEST_FAIL(fmt, ...)                                               \
    do {                                                                      \
        minitest_failures++;                                                  \
        fprintf(stderr, "%s:%d: " fmt "\n", __FILE__, __LINE__, __VA_ARGS__); \
    } while(0)

#define CHECK(cond)                                           \
    do {                                                      \
        minitest_checks++;                                    \
        if(!(cond)) MINITEST_FAIL("CHECK(%s) failed", #cond); \
    } while(0)

#define CHECK_EQ_INT(actual, expected)                                                   \
    do {                                                                                 \
        long long minitest_a = (long long)(actual);                                      \
        long long minitest_e = (long long)(expected);                                    \
        minitest_checks++;                                                               \
        if(minitest_a != minitest_e)                                                     \
            MINITEST_FAIL("%s == %lld, expected %lld", #actual, minitest_a, minitest_e); \
    } while(0)

#define CHECK_STR_EQ(actual, expected)                                                       \
    do {                                                                                     \
        const char* minitest_a = (actual);                                                   \
        const char* minitest_e = (expected);                                                 \
        minitest_checks++;                                                                   \
        if(strcmp(minitest_a, minitest_e) != 0)                                              \
            MINITEST_FAIL("%s == \"%s\", expected \"%s\"", #actual, minitest_a, minitest_e); \
    } while(0)

#define CHECK_MEM_EQ(actual, expected, len)                                       \
    do {                                                                          \
        minitest_checks++;                                                        \
        if(memcmp((actual), (expected), (len)) != 0) {                            \
            MINITEST_FAIL("%s differs from %s", #actual, #expected);              \
            minitest_dump("  actual  ", (const unsigned char*)(actual), (len));   \
            minitest_dump("  expected", (const unsigned char*)(expected), (len)); \
        }                                                                         \
    } while(0)

static inline void minitest_dump(const char* label, const unsigned char* bytes, size_t len) {
    fprintf(stderr, "%s ", label);
    for(size_t i = 0; i < len; i++)
        fprintf(stderr, "%02x", bytes[i]);
    fprintf(stderr, "\n");
}

#define RUN_TEST(fn)                                                                    \
    do {                                                                                \
        int minitest_before = minitest_failures;                                        \
        minitest_tests++;                                                               \
        fn();                                                                           \
        printf("%s %s\n", minitest_failures == minitest_before ? "ok  " : "FAIL", #fn); \
    } while(0)

#define MINITEST_RESULT()                          \
    (printf(                                       \
         "%s: %d tests, %d checks, %d failures\n", \
         __FILE__,                                 \
         minitest_tests,                           \
         minitest_checks,                          \
         minitest_failures),                       \
     minitest_failures == 0 ? 0 : 1)
