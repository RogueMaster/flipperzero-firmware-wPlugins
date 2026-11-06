#include "minitest.h"
#include "dump_loaders.h"

#include "ui_text.h"

static TagDump dump;
static char out[64];

/* 6 px per character, like FontSecondary's digits. */
static uint16_t mono(void* ctx, const char* text) {
    (void)ctx;
    return (uint16_t)(strlen(text) * 6);
}

static void test_text_that_fits_is_kept(void) {
    strcpy(out, "PLA Basic");
    ui_text_fit(out, 54, mono, NULL);
    CHECK_STR_EQ(out, "PLA Basic");
}

static void test_long_text_is_cut_with_dots(void) {
    strcpy(out, "PLA Silk+ Rainbow Gold");
    ui_text_fit(out, 60, mono, NULL);
    CHECK_STR_EQ(out, "PLA Sil...");
    CHECK(mono(NULL, out) <= 60);
}

static void test_text_cut_to_nothing_keeps_what_fits(void) {
    strcpy(out, "Bambuddy");
    ui_text_fit(out, 12, mono, NULL);
    CHECK_STR_EQ(out, "Ba");
    strcpy(out, "Bambuddy");
    ui_text_fit(out, 0, mono, NULL);
    CHECK_STR_EQ(out, "");
}

static void test_wrap_breaks_between_words(void) {
    const char* text = "No reply in 10 s. Open FliBuddy on the Mac or iPhone.";
    size_t pos = 0;
    CHECK(ui_text_wrap(text, &pos, out, sizeof(out), 108, mono, NULL));
    CHECK_STR_EQ(out, "No reply in 10 s.");
    CHECK(ui_text_wrap(text, &pos, out, sizeof(out), 108, mono, NULL));
    CHECK_STR_EQ(out, "Open FliBuddy on");
    CHECK(ui_text_wrap(text, &pos, out, sizeof(out), 108, mono, NULL));
    CHECK_STR_EQ(out, "the Mac or iPhone.");
    CHECK(!ui_text_wrap(text, &pos, out, sizeof(out), 108, mono, NULL));
}

static void test_wrap_keeps_line_breaks_and_cuts_long_words(void) {
    const char* text = "Saved\n\nBambuddyunreachable";
    size_t pos = 0;
    CHECK(ui_text_wrap(text, &pos, out, sizeof(out), 60, mono, NULL));
    CHECK_STR_EQ(out, "Saved");
    CHECK(ui_text_wrap(text, &pos, out, sizeof(out), 60, mono, NULL));
    CHECK_STR_EQ(out, "");
    CHECK(ui_text_wrap(text, &pos, out, sizeof(out), 60, mono, NULL));
    CHECK_STR_EQ(out, "Bambuddyun");
    CHECK(ui_text_wrap(text, &pos, out, sizeof(out), 60, mono, NULL));
    CHECK_STR_EQ(out, "reachable");
    CHECK(!ui_text_wrap(text, &pos, out, sizeof(out), 60, mono, NULL));

    pos = 0;
    CHECK(!ui_text_wrap("", &pos, out, sizeof(out), 60, mono, NULL));
}

static void test_wrap_takes_a_character_even_when_none_fits(void) {
    const char* text = "abc";
    size_t pos = 0;
    for(const char* want = "abc"; *want; want++) {
        CHECK(ui_text_wrap(text, &pos, out, sizeof(out), 3, mono, NULL));
        CHECK(out[0] == *want);
        CHECK(out[1] == '\0');
    }
    CHECK(!ui_text_wrap(text, &pos, out, sizeof(out), 3, mono, NULL));
}

static void test_wrap_line_that_fits_exactly_ends_at_the_space(void) {
    const char* text = "abcdef gh";
    size_t pos = 0;
    CHECK(ui_text_wrap(text, &pos, out, sizeof(out), 36, mono, NULL));
    CHECK_STR_EQ(out, "abcdef");
    CHECK(ui_text_wrap(text, &pos, out, sizeof(out), 36, mono, NULL));
    CHECK_STR_EQ(out, "gh");
    CHECK(!ui_text_wrap(text, &pos, out, sizeof(out), 36, mono, NULL));
}

static void test_wrap_respects_the_buffer(void) {
    char small[6];
    size_t pos = 0;
    CHECK(ui_text_wrap("Waiting for Bambuddy", &pos, small, sizeof(small), 200, mono, NULL));
    CHECK_STR_EQ(small, "Waiti");
    CHECK(ui_text_wrap("Waiting for Bambuddy", &pos, small, sizeof(small), 200, mono, NULL));
    CHECK_STR_EQ(small, "ng");
}

static void test_short_uid_keeps_four_bytes_whole(void) {
    const uint8_t uid4[4] = {0x91, 0xB4, 0xE7, 0xC2};
    CHECK_EQ_INT(ui_uid_short(uid4, 4, out, sizeof(out)), 8);
    CHECK_STR_EQ(out, "91B4E7C2");
    const uint8_t uid7[7] = {0x04, 0xB7, 0xE2, 0x11, 0x6A, 0x3C, 0x80};
    ui_uid_short(uid7, 7, out, sizeof(out));
    CHECK_STR_EQ(out, "04B7...3C80");
}

static void test_uid_with_colons(void) {
    const uint8_t uid4[4] = {0x91, 0xB4, 0xE7, 0xC2};
    ui_uid_colons(uid4, 4, out, sizeof(out));
    CHECK_STR_EQ(out, "91:B4:E7:C2");
    char small[6];
    ui_uid_colons(uid4, 4, small, sizeof(small));
    CHECK_STR_EQ(small, "91:B4");
}

static void test_luminance_of_rgba(void) {
    CHECK_EQ_INT(ui_luminance(0xFFFFFFFFu), 255);
    CHECK_EQ_INT(ui_luminance(0x000000FFu), 0);
    CHECK_EQ_INT(ui_luminance(0x00AE42FFu), 109); /* Bambu Green */
    CHECK_EQ_INT(ui_luminance(0xFF000000u), 76); /* alpha is ignored */
}

static int ink_in_4x4(uint8_t lum) {
    int n = 0;
    for(int y = 0; y < 4; y++)
        for(int x = 0; x < 4; x++)
            n += ui_bayer_ink(x, y, lum);
    return n;
}

static void test_bayer_dithering_follows_the_luminance(void) {
    CHECK_EQ_INT(ink_in_4x4(255), 0);
    CHECK_EQ_INT(ink_in_4x4(0), 16);
    CHECK_EQ_INT(ink_in_4x4(128), 8);
    CHECK(ink_in_4x4(64) > ink_in_4x4(192));
    /* the pattern repeats every 4 pixels, also for negative coordinates */
    CHECK_EQ_INT(ui_bayer_ink(5, 6, 100), ui_bayer_ink(1, 2, 100));
    CHECK_EQ_INT(ui_bayer_ink(-3, -2, 100), ui_bayer_ink(1, 2, 100));
}

static void test_tag_type_names(void) {
    load_mfc(&dump);
    CHECK_STR_EQ(ui_tag_type_name(&dump), "MIFARE Classic 1K");
    dump.sak = 0x18;
    CHECK_STR_EQ(ui_tag_type_name(&dump), "MIFARE Classic 4K");
    load_ntag(&dump);
    CHECK_STR_EQ(ui_tag_type_name(&dump), "NTAG215");
    dump.page_count = 45;
    CHECK_STR_EQ(ui_tag_type_name(&dump), "NTAG213");
    dump.page_count = 231;
    CHECK_STR_EQ(ui_tag_type_name(&dump), "NTAG216");
    dump.page_count = 20;
    CHECK_STR_EQ(ui_tag_type_name(&dump), "NTAG");
}

static void test_spool_names(void) {
    ui_spool_name("PLA", "Basic", out, sizeof(out));
    CHECK_STR_EQ(out, "PLA Basic");
    ui_spool_name("PLA", "PLA Silk+", out, sizeof(out));
    CHECK_STR_EQ(out, "PLA Silk+");
    ui_spool_name("PETG", "", out, sizeof(out));
    CHECK_STR_EQ(out, "PETG");
    ui_spool_name("", "Basic", out, sizeof(out));
    CHECK_STR_EQ(out, "Basic");
    ui_spool_name("", "", out, sizeof(out));
    CHECK_STR_EQ(out, "");
    /* "PLA" does not start "PLAX": a subtype that only shares letters */
    ui_spool_name("PLA", "PLAX", out, sizeof(out));
    CHECK_STR_EQ(out, "PLA PLAX");
    /* the match ignores case (PROTOCOL.md, `subtype`), as the host's does */
    ui_spool_name("PLA", "pla Silk+", out, sizeof(out));
    CHECK_STR_EQ(out, "pla Silk+");
    ui_spool_name("petg", "PETG HF", out, sizeof(out));
    CHECK_STR_EQ(out, "PETG HF");
    ui_spool_name("PLA", "plax", out, sizeof(out));
    CHECK_STR_EQ(out, "PLA plax");
}

int main(void) {
    RUN_TEST(test_text_that_fits_is_kept);
    RUN_TEST(test_long_text_is_cut_with_dots);
    RUN_TEST(test_text_cut_to_nothing_keeps_what_fits);
    RUN_TEST(test_wrap_breaks_between_words);
    RUN_TEST(test_wrap_keeps_line_breaks_and_cuts_long_words);
    RUN_TEST(test_wrap_takes_a_character_even_when_none_fits);
    RUN_TEST(test_wrap_line_that_fits_exactly_ends_at_the_space);
    RUN_TEST(test_wrap_respects_the_buffer);
    RUN_TEST(test_short_uid_keeps_four_bytes_whole);
    RUN_TEST(test_uid_with_colons);
    RUN_TEST(test_luminance_of_rgba);
    RUN_TEST(test_bayer_dithering_follows_the_luminance);
    RUN_TEST(test_tag_type_names);
    RUN_TEST(test_spool_names);
    return MINITEST_RESULT();
}
