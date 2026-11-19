#include "hex/intel_hex.h"

#include <stdio.h>
#include <string.h>

typedef struct {
    int count;
    uint32_t address[4];
    uint8_t data[4][INTEL_HEX_MAX_PAGE_SIZE];
    uint16_t page_size;
} PageCapture;

typedef struct {
    int count;
    char lines[8][80];
} LineCapture;

static int failures = 0;

static void check(int condition, const char* message, int line) {
    if(condition) return;
    fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, line, message);
    failures++;
}

#define CHECK(cond) check((cond), #cond, __LINE__)

static bool
    capture_page(uint32_t address, const uint8_t* data, uint16_t page_size, void* context) {
    PageCapture* pages = context;
    if(pages->count >= 4) return false;
    pages->address[pages->count] = address;
    pages->page_size = page_size;
    memcpy(pages->data[pages->count], data, page_size);
    pages->count++;
    return true;
}

static bool feed_all(const char** lines, PageCapture* pages, char* error, size_t error_size) {
    IntelHexParser* parser = intel_hex_parser_alloc(128u * 1024u, 512u, capture_page, pages);
    bool ok = parser != NULL;
    for(size_t i = 0; ok && lines[i] != NULL; i++) {
        ok = intel_hex_parser_feed(parser, lines[i], error, error_size);
    }
    if(ok) ok = intel_hex_parser_finish(parser, error, error_size);
    intel_hex_parser_free(parser);
    return ok;
}

static bool page_is_ff_except(
    const uint8_t* page,
    uint16_t page_size,
    const uint16_t* indexes,
    const uint8_t* values,
    size_t n) {
    uint8_t expect[INTEL_HEX_MAX_PAGE_SIZE];
    memset(expect, 0xFF, page_size);
    for(size_t i = 0; i < n; i++)
        expect[indexes[i]] = values[i];
    return memcmp(page, expect, page_size) == 0;
}

static bool capture_line(const char* line, void* context) {
    LineCapture* lines = context;
    if(lines->count >= 8) return false;
    snprintf(lines->lines[lines->count], sizeof(lines->lines[0]), "%s", line);
    lines->count++;
    return true;
}

static void test_page_crossing(void) {
    PageCapture pages;
    memset(&pages, 0, sizeof(pages));
    char error[64];
    const char* lines[] = {
        ":020000040000FA",
        ":0201FE000102FC",
        ":020200000304F5",
        ":00000001FF",
        NULL,
    };
    CHECK(feed_all(lines, &pages, error, sizeof(error)));
    CHECK(pages.count == 2);
    CHECK(pages.address[0] == 0);
    CHECK(pages.address[1] == 512);
    uint16_t i0[] = {0x1FE, 0x1FF};
    uint8_t v0[] = {0x01, 0x02};
    uint16_t i1[] = {0, 1};
    uint8_t v1[] = {0x03, 0x04};
    CHECK(page_is_ff_except(pages.data[0], 512, i0, v0, 2));
    CHECK(page_is_ff_except(pages.data[1], 512, i1, v1, 2));
}

static void test_sparse_page(void) {
    PageCapture pages;
    memset(&pages, 0, sizeof(pages));
    char error[64];
    const char* lines[] = {
        ":030010000A0B0CCC",
        ":00000001FF",
        NULL,
    };
    CHECK(feed_all(lines, &pages, error, sizeof(error)));
    CHECK(pages.count == 1);
    CHECK(pages.address[0] == 0);
    uint16_t indexes[] = {0x10, 0x11, 0x12};
    uint8_t values[] = {0x0A, 0x0B, 0x0C};
    CHECK(page_is_ff_except(pages.data[0], 512, indexes, values, 3));
}

static void test_high_address(void) {
    PageCapture pages;
    memset(&pages, 0, sizeof(pages));
    char error[64];
    const char* lines[] = {
        ":020000040001F9",
        ":02000000AABB99",
        ":00000001FF",
        NULL,
    };
    CHECK(feed_all(lines, &pages, error, sizeof(error)));
    CHECK(pages.count == 1);
    CHECK(pages.address[0] == 0x10000);
    uint16_t indexes[] = {0, 1};
    uint8_t values[] = {0xAA, 0xBB};
    CHECK(page_is_ff_except(pages.data[0], 512, indexes, values, 2));
}

static void test_finish_without_eof(void) {
    PageCapture pages;
    memset(&pages, 0, sizeof(pages));
    char error[64];
    const char* lines[] = {
        ":020000040001F9",
        ":01000000AB54",
        NULL,
    };
    CHECK(feed_all(lines, &pages, error, sizeof(error)));
    CHECK(pages.count == 1);
    CHECK(pages.address[0] == 0x10000);
    CHECK(pages.data[0][0] == 0xAB);
    CHECK(pages.data[0][1] == 0xFF);
}

static void test_bad_checksum(void) {
    PageCapture pages;
    memset(&pages, 0, sizeof(pages));
    char error[64];
    IntelHexParser* parser = intel_hex_parser_alloc(128u * 1024u, 512u, capture_page, &pages);
    CHECK(parser != NULL);
    CHECK(!intel_hex_parser_feed(parser, ":01000000AB00", error, sizeof(error)));
    intel_hex_parser_free(parser);

    memset(&pages, 0, sizeof(pages));
    parser = intel_hex_parser_alloc(128u * 1024u, 512u, capture_page, &pages);
    CHECK(parser != NULL);
    CHECK(!intel_hex_parser_feed(parser, "01000000AB52", error, sizeof(error)));
    intel_hex_parser_free(parser);
}

static void test_writer_roundtrip(void) {
    LineCapture lines;
    memset(&lines, 0, sizeof(lines));
    char error[64];
    IntelHexWriter* writer = intel_hex_writer_alloc(capture_line, &lines);
    CHECK(writer != NULL);
    uint8_t data[] = {0xDE, 0xAD};
    CHECK(intel_hex_writer_feed(writer, 0x10000, data, sizeof(data), error, sizeof(error)));
    CHECK(intel_hex_writer_finish(writer, error, sizeof(error)));
    intel_hex_writer_free(writer);
    CHECK(lines.count >= 2);
}

int main(void) {
    test_page_crossing();
    test_sparse_page();
    test_high_address();
    test_finish_without_eof();
    test_bad_checksum();
    test_writer_roundtrip();
    if(failures) {
        fprintf(stderr, "%d failures\n", failures);
        return 1;
    }
    printf("ok\n");
    return 0;
}
