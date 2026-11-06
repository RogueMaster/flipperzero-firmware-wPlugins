#include "minitest.h"
#include "dump_loaders.h"

#include "nfc_classify.h"

static TagDump mfc;
static TagDump ntag;
static char text[TAG_BLOCK_TEXT_MAX];

static void test_sak_mifare_classic_1k_and_4k(void) {
    TagType type = TagTypeNtag;
    CHECK(tag_type_from_sak(0x08, &type));
    CHECK_EQ_INT(type, TagTypeMfc);
    type = TagTypeNtag;
    CHECK(tag_type_from_sak(0x18, &type));
    CHECK_EQ_INT(type, TagTypeMfc);
}

static void test_sak_ntag(void) {
    TagType type = TagTypeMfc;
    CHECK(tag_type_from_sak(0x00, &type));
    CHECK_EQ_INT(type, TagTypeNtag);
}

static void test_sak_unsupported(void) {
    /* MIFARE Mini, Plus SL2, ISO 14443-4 (DESFire, bank cards), SAK with the
     * "UID not complete" bit */
    const uint8_t others[] = {0x09, 0x10, 0x11, 0x20, 0x28, 0x38, 0x04, 0x88};
    for(size_t i = 0; i < sizeof(others); i++) {
        TagType type = TagTypeNtag;
        CHECK(!tag_type_from_sak(others[i], &type));
        CHECK_EQ_INT(type, TagTypeNtag); /* untouched */
    }
}

static void test_ntag_read_result(void) {
    CHECK_EQ_INT(tag_ntag_read_result(135, false, true), TagReadOk);
    CHECK_EQ_INT(tag_ntag_read_result(1, false, false), TagReadOk); /* what was read counts */
    /* no page and a password: retrying cannot help, the tag stays */
    CHECK_EQ_INT(tag_ntag_read_result(0, true, false), TagReadUnsupported);
    /* no page, the tag still answers: it stays remembered, so a "retry" would
     * never happen; call it unreadable */
    CHECK_EQ_INT(tag_ntag_read_result(0, false, true), TagReadUnsupported);
    /* no page and the tag is gone: a real, retryable failure */
    CHECK_EQ_INT(tag_ntag_read_result(0, false, false), TagReadFailed);
}

static void test_block_text_of_a_bambu_tag(void) {
    load_mfc(&mfc);
    CHECK(tag_block_text(&mfc, TAG_BLOCK_MATERIAL, text));
    CHECK_STR_EQ(text, "PLA");
    CHECK(tag_block_text(&mfc, TAG_BLOCK_DETAILED_TYPE, text));
    CHECK_STR_EQ(text, "PLA Basic");

    load_mfc_real(&mfc);
    CHECK(tag_block_text(&mfc, TAG_BLOCK_DETAILED_TYPE, text));
    CHECK_STR_EQ(text, "PLA Pure");
}

static void test_block_text_of_a_missing_block(void) {
    /* sector 1 locked: no detailed type */
    load_mfc(&mfc);
    for(uint8_t b = 4; b < 8; b++)
        mfc.block_present &= ~(1ULL << b);
    CHECK(!tag_block_text(&mfc, TAG_BLOCK_DETAILED_TYPE, text));
    CHECK_STR_EQ(text, "");
    CHECK(!tag_block_text(&mfc, TAG_DUMP_MFC_BLOCKS, text));
    CHECK_STR_EQ(text, "");
}

static void test_block_text_is_sanitized(void) {
    /* control and non-ASCII bytes become '?', the text ends at the first
     * NUL, trailing spaces go; a block of spaces holds no text */
    static const uint8_t block2[16] = {'P', 'E', 'T', 0x01, 'G', 0xC3, ' ', ' ', 0, 'X'};
    static const uint8_t block4[16] = {' ', ' ', ' ', ' ', 0};
    load_mfc(&mfc);
    CHECK(tag_dump_set_block(&mfc, 2, block2));
    CHECK(tag_dump_set_block(&mfc, 4, block4));
    CHECK(tag_block_text(&mfc, TAG_BLOCK_MATERIAL, text));
    CHECK_STR_EQ(text, "PET?G?");
    CHECK(!tag_block_text(&mfc, TAG_BLOCK_DETAILED_TYPE, text));
}

static void test_block_text_of_16_characters(void) {
    static const uint8_t block4[16] = {
        'P', 'L', 'A', ' ', 'S', 'i', 'l', 'k', '+', ' ', 'R', 'a', 'i', 'n', 'b', 'w'};
    load_mfc(&mfc);
    CHECK(tag_dump_set_block(&mfc, 4, block4));
    CHECK(tag_block_text(&mfc, TAG_BLOCK_DETAILED_TYPE, text));
    CHECK_STR_EQ(text, "PLA Silk+ Rainbw");
}

static void test_ntag_has_no_block_text(void) {
    load_ntag(&ntag);
    CHECK(!tag_block_text(&ntag, TAG_BLOCK_MATERIAL, text));
    CHECK_STR_EQ(text, "");
}

static void test_sectors_read(void) {
    load_mfc(&mfc);
    CHECK_EQ_INT(tag_sectors_read(&mfc), 15);
    load_mfc_real(&mfc);
    CHECK_EQ_INT(tag_sectors_read(&mfc), 3);
    mfc.block_present = 0;
    CHECK_EQ_INT(tag_sectors_read(&mfc), 0);
    load_ntag(&ntag);
    CHECK_EQ_INT(tag_sectors_read(&ntag), 0);
}

int main(void) {
    RUN_TEST(test_sak_mifare_classic_1k_and_4k);
    RUN_TEST(test_sak_ntag);
    RUN_TEST(test_sak_unsupported);
    RUN_TEST(test_ntag_read_result);
    RUN_TEST(test_block_text_of_a_bambu_tag);
    RUN_TEST(test_block_text_of_a_missing_block);
    RUN_TEST(test_block_text_is_sanitized);
    RUN_TEST(test_block_text_of_16_characters);
    RUN_TEST(test_ntag_has_no_block_text);
    RUN_TEST(test_sectors_read);
    return MINITEST_RESULT();
}
