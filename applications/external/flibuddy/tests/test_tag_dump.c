#include "minitest.h"
#include "fixture.h"
#include "dump_loaders.h"

#include "proto_encode.h"
#include "tag_dump.h"

#define FIXTURE_CAP 16384

static TagDump dump;
static char line[PROTO_LINE_MAX];
static char expected[FIXTURE_CAP];

static void test_init_clears_everything(void) {
    memset(&dump, 0xAB, sizeof(dump));
    tag_dump_init(&dump, TagTypeNtag);
    CHECK(dump.type == TagTypeNtag);
    CHECK_EQ_INT(dump.uid_len, 0);
    CHECK_EQ_INT(dump.block_present, 0);
    CHECK_EQ_INT(dump.page_count, 0);
    CHECK_EQ_INT(dump.sak, 0);
}

static void test_uid_lengths(void) {
    static const uint8_t uid[11] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
    tag_dump_init(&dump, TagTypeMfc);
    CHECK(tag_dump_set_uid(&dump, uid, 4));
    CHECK(tag_dump_set_uid(&dump, uid, 7));
    CHECK(tag_dump_set_uid(&dump, uid, 10));
    CHECK_EQ_INT(dump.uid_len, 10);
    CHECK_MEM_EQ(dump.uid, uid, 10);
    CHECK(!tag_dump_set_uid(&dump, uid, 0));
    CHECK(!tag_dump_set_uid(&dump, uid, 5));
    CHECK(!tag_dump_set_uid(&dump, uid, 11));
    CHECK_EQ_INT(dump.uid_len, 10);
}

static void test_blocks_and_missing_sectors(void) {
    static const uint8_t data[16] = {0xFF};
    tag_dump_init(&dump, TagTypeMfc);
    CHECK(!tag_dump_block_present(&dump, 0));
    CHECK(tag_dump_set_block(&dump, 63, data));
    CHECK(tag_dump_block_present(&dump, 63));
    CHECK_MEM_EQ(dump.blocks[63], data, 16);
    CHECK(!tag_dump_set_block(&dump, 64, data));
    CHECK(!tag_dump_block_present(&dump, 64));

    /* A sector is missing unless all three of its data blocks are present;
     * the trailer does not count. */
    tag_dump_init(&dump, TagTypeMfc);
    CHECK(tag_dump_sector_missing(&dump, 1));
    CHECK(tag_dump_set_block(&dump, 4, data));
    CHECK(tag_dump_set_block(&dump, 5, data));
    CHECK(tag_dump_sector_missing(&dump, 1));
    CHECK(tag_dump_set_block(&dump, 6, data));
    CHECK(!tag_dump_sector_missing(&dump, 1));
    CHECK(tag_dump_sector_missing(&dump, 16));
}

static void test_pages_fill_up_to_capacity(void) {
    static const uint8_t page[4] = {1, 2, 3, 4};
    tag_dump_init(&dump, TagTypeNtag);
    for(int i = 0; i < TAG_DUMP_MAX_PAGES; i++)
        CHECK(tag_dump_append_page(&dump, page));
    CHECK_EQ_INT(dump.page_count, TAG_DUMP_MAX_PAGES);
    CHECK(!tag_dump_append_page(&dump, page));
    CHECK_EQ_INT(dump.page_count, TAG_DUMP_MAX_PAGES);
    CHECK_MEM_EQ(dump.pages[TAG_DUMP_MAX_PAGES - 1], page, 4);
}

/* The blocks and the pages share their storage (a dump is one or the
 * other), so each kind of data goes only into its own kind of dump. */
static void test_blocks_and_pages_share_storage(void) {
    static const uint8_t data[16] = {0xAB};
    static const uint8_t page[4] = {1, 2, 3, 4};
    CHECK(sizeof(TagDump) < TAG_DUMP_MFC_BLOCKS * TAG_DUMP_BLOCK_LEN + 64);
    tag_dump_init(&dump, TagTypeNtag);
    CHECK(!tag_dump_set_block(&dump, 0, data));
    CHECK_EQ_INT(dump.block_present, 0);
    tag_dump_init(&dump, TagTypeMfc);
    CHECK(!tag_dump_append_page(&dump, page));
    CHECK_EQ_INT(dump.page_count, 0);
    CHECK(tag_dump_set_block(&dump, 0, data));
}

static void test_scan_mfc_matches_fixture(void) {
    load_mfc(&dump);
    size_t len = proto_encode_scan(&dump, MFC_SCAN_ID, false, line, sizeof(line));
    CHECK(fixture_read_compact("fixtures/messages/scan_mfc.json", expected, sizeof(expected)));
    CHECK_STR_EQ(line, expected);
    CHECK_EQ_INT(len, strlen(expected));
}

static void test_scan_mfc_autosave_matches_fixture(void) {
    load_mfc(&dump);
    size_t len = proto_encode_scan(&dump, "s-000015", true, line, sizeof(line));
    CHECK(fixture_read_compact(
        "fixtures/messages/scan_mfc_autosave.json", expected, sizeof(expected)));
    CHECK_STR_EQ(line, expected);
    CHECK_EQ_INT(len, strlen(expected));
    CHECK(
        strncmp(line, "{\"t\":\"scan\",\"v\":2,\"scan_id\":\"s-000015\",\"autosave\":true,", 55) ==
        0);
}

static void test_scan_mfc_without_missing_sectors_omits_missing(void) {
    load_mfc(&dump);
    for(uint8_t b = 0; b < TAG_DUMP_MFC_BLOCKS; b++)
        CHECK(tag_dump_set_block(&dump, b, MFC_BLOCKS[b]));
    CHECK(proto_encode_scan(&dump, MFC_SCAN_ID, false, line, sizeof(line)) > 0);
    CHECK(strstr(line, "\"missing\"") == NULL);
    CHECK(strstr(line, "\"28\":") != NULL);
    CHECK(strstr(line, "\"3\":") == NULL); /* trailers are never sent */
    CHECK(strstr(line, "\"63\":") == NULL);
    CHECK(strstr(line, "\"62\":") != NULL);
    CHECK(strcmp(line + strlen(line) - 4, "\"}}}") == 0);
}

static void test_scan_mfc_with_every_sector_missing(void) {
    tag_dump_init(&dump, TagTypeMfc);
    CHECK(tag_dump_set_uid(&dump, MFC_UID, sizeof(MFC_UID)));
    dump.sak = MFC_SAK;
    memcpy(dump.atqa, MFC_ATQA, sizeof(MFC_ATQA));
    size_t len = proto_encode_scan(&dump, "s-000014", false, line, sizeof(line));
    CHECK(fixture_read_compact(
        "fixtures/messages/scan_mfc_unreadable.json", expected, sizeof(expected)));
    CHECK_STR_EQ(line, expected);
    CHECK_EQ_INT(len, strlen(expected));
}

static void test_scan_mfc_real_dump(void) {
    /* the real PLA Pure dump: data blocks of sectors 0-2 without trailers,
     * block 9 raw, sectors 3-15 listed as missing */
    static const char want[] =
        "{\"t\":\"scan\",\"v\":2,\"scan_id\":\"s-000099\",\"autosave\":false,\"tag\":{\"type\":\"mfc\","
        "\"uid\":\"7EF1AB04\",\"sak\":8,\"atqa\":\"0400\",\"blocks\":{"
        "\"0\":\"7EF1AB042008040005AAC8D5839ED190\","
        "\"1\":\"4131392D423030004746413139000000\","
        "\"2\":\"504C4100000000000000000000000000\","
        "\"4\":\"504C4120507572650000000000000000\","
        "\"5\":\"A4DBE8FFE80300000000E03F00000000\","
        "\"6\":\"3700080000000000E600BE0000000000\","
        "\"8\":\"00000000000000000000000000000000\","
        "\"9\":\"F1C8FDF0BC1643C0B37189ADB0687992\","
        "\"10\":\"010000009A0200000000000000000000\"},"
        "\"missing\":[3,4,5,6,7,8,9,10,11,12,13,14,15]}}";
    load_mfc_real(&dump);
    size_t len = proto_encode_scan(&dump, "s-000099", false, line, sizeof(line));
    CHECK_STR_EQ(line, want);
    CHECK_EQ_INT(len, strlen(want));
}

static void test_scan_ntag_matches_fixture(void) {
    load_ntag(&dump);
    size_t len = proto_encode_scan(&dump, NTAG_SCAN_ID, false, line, sizeof(line));
    CHECK(fixture_read_compact("fixtures/messages/scan_ntag.json", expected, sizeof(expected)));
    CHECK_STR_EQ(line, expected);
    CHECK_EQ_INT(len, strlen(expected));
}

static void test_largest_ntag_fits_in_a_line(void) {
    static const uint8_t page[4] = {0xFF, 0xFF, 0xFF, 0xFF};
    static const uint8_t uid[10] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    tag_dump_init(&dump, TagTypeNtag);
    CHECK(tag_dump_set_uid(&dump, uid, sizeof(uid)));
    dump.sak = 255;
    for(int i = 0; i < TAG_DUMP_MAX_PAGES; i++)
        CHECK(tag_dump_append_page(&dump, page));
    size_t len = proto_encode_scan(&dump, "s-999999", false, line, sizeof(line));
    CHECK(len > 0);
    CHECK(len + 1 <= PROTO_LINE_MAX); /* room for the '\n' */
    CHECK(strstr(line, "\"230\":\"FFFFFFFF\"}}}") != NULL);
}

static void test_scan_too_large_for_buffer_fails(void) {
    load_mfc(&dump);
    size_t full = proto_encode_scan(&dump, MFC_SCAN_ID, false, line, sizeof(line));
    CHECK(full > 0);
    static char small[1024];
    CHECK_EQ_INT(proto_encode_scan(&dump, MFC_SCAN_ID, false, small, sizeof(small)), 0);
    CHECK_EQ_INT(proto_encode_scan(&dump, MFC_SCAN_ID, false, line, full), 0);
    CHECK_EQ_INT(proto_encode_scan(&dump, MFC_SCAN_ID, false, line, full + 1), full);
    CHECK_EQ_INT(proto_encode_scan(&dump, MFC_SCAN_ID, false, line, 0), 0);
}

static void test_scan_rejects_invalid_input(void) {
    load_ntag(&dump);
    CHECK_EQ_INT(proto_encode_scan(&dump, "", false, line, sizeof(line)), 0);
    CHECK_EQ_INT(proto_encode_scan(&dump, "s 1", false, line, sizeof(line)), 0);
    CHECK_EQ_INT(proto_encode_scan(&dump, "s\"1", false, line, sizeof(line)), 0);
    CHECK_EQ_INT(
        proto_encode_scan(&dump, "s-0123456789012345678901234567890", false, line, sizeof(line)),
        0);
    CHECK(
        proto_encode_scan(&dump, "s-012345678901234567890123456789", false, line, sizeof(line)) >
        0);
    CHECK(proto_encode_scan(&dump, "Az09_.:-", false, line, sizeof(line)) > 0);

    dump.uid_len = 0;
    CHECK_EQ_INT(proto_encode_scan(&dump, NTAG_SCAN_ID, false, line, sizeof(line)), 0);
    load_ntag(&dump);
    dump.page_count = TAG_DUMP_MAX_PAGES + 1;
    CHECK_EQ_INT(proto_encode_scan(&dump, NTAG_SCAN_ID, false, line, sizeof(line)), 0);
    load_ntag(&dump);
    dump.type = (TagType)7;
    CHECK_EQ_INT(proto_encode_scan(&dump, NTAG_SCAN_ID, false, line, sizeof(line)), 0);
}

static void test_hello_matches_fixture(void) {
    size_t len = proto_encode_hello("flibuddy-fap/1.0", "1.5.1", line, sizeof(line));
    CHECK(
        fixture_read_compact("fixtures/messages/hello_flipper.json", expected, sizeof(expected)));
    CHECK_STR_EQ(line, expected);
    CHECK_EQ_INT(len, strlen(expected));
    CHECK_EQ_INT(proto_encode_hello("flibuddy-fap/1.0", "1.5.1", line, len), 0);
}

static void test_line_limit_applies_to_large_buffers(void) {
    /* An encoder never produces a line the receiver would drop, even when
     * the caller's buffer is bigger than the line limit. */
    static char big[PROTO_LINE_MAX * 2];
    static char app[PROTO_LINE_MAX];
    size_t overhead = strlen("{\"t\":\"hello\",\"v\":1,\"app\":\"\",\"fw\":\"x\"}");
    size_t fits = PROTO_LINE_MAX - 1 - overhead; /* longest app with room for '\n' */

    memset(app, 'a', fits);
    app[fits] = '\0';
    CHECK_EQ_INT(proto_encode_hello(app, "x", big, sizeof(big)), PROTO_LINE_MAX - 1);
    app[fits] = 'a';
    app[fits + 1] = '\0';
    CHECK_EQ_INT(proto_encode_hello(app, "x", big, sizeof(big)), 0);
}

int main(void) {
    RUN_TEST(test_init_clears_everything);
    RUN_TEST(test_uid_lengths);
    RUN_TEST(test_blocks_and_missing_sectors);
    RUN_TEST(test_pages_fill_up_to_capacity);
    RUN_TEST(test_blocks_and_pages_share_storage);
    RUN_TEST(test_scan_mfc_matches_fixture);
    RUN_TEST(test_scan_mfc_autosave_matches_fixture);
    RUN_TEST(test_scan_mfc_without_missing_sectors_omits_missing);
    RUN_TEST(test_scan_mfc_with_every_sector_missing);
    RUN_TEST(test_scan_mfc_real_dump);
    RUN_TEST(test_scan_ntag_matches_fixture);
    RUN_TEST(test_largest_ntag_fits_in_a_line);
    RUN_TEST(test_scan_too_large_for_buffer_fails);
    RUN_TEST(test_scan_rejects_invalid_input);
    RUN_TEST(test_hello_matches_fixture);
    RUN_TEST(test_line_limit_applies_to_large_buffers);
    return MINITEST_RESULT();
}
