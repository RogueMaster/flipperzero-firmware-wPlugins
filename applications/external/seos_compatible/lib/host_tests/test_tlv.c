/* BER-TLV reading and writing.
 *
 * Encoding rules follow ITU-T X.690: a tag whose low five bits are all set
 * continues into a second octet, and a length of 128 or more takes the long
 * form with a leading count of length octets.
 */
#include "munit.h"
#include "test_helpers.h"

#include <seos_tlv.h>

#include <string.h>

/* A value long enough to need each length form in turn. */
static void fill_pattern(uint8_t* out, size_t len) {
    for(size_t i = 0; i < len; i++) {
        out[i] = (uint8_t)(i * 7 + 1);
    }
}

/* ---- reading ---- */

static MunitResult test_read_short_form(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    const uint8_t raw[] = {0x85, 0x03, 0xaa, 0xbb, 0xcc};
    SeosTlvCursor cursor;
    SeosTlvObject object;

    seos_tlv_cursor_init(&cursor, raw, sizeof(raw));
    munit_assert_true(seos_tlv_read(&cursor, &object));
    munit_assert_uint16(object.tag, ==, 0x85);
    munit_assert_size(object.value_len, ==, 3);
    munit_assert_size(object.value_offset, ==, 2);
    munit_assert_memory_equal(3, object.value, raw + 2);
    /* The cursor has stepped past the value, which is the point of it. */
    munit_assert_true(seos_tlv_cursor_done(&cursor));

    return MUNIT_OK;
}

static MunitResult test_read_long_form_one_octet(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    uint8_t raw[3 + 200];
    raw[0] = 0x85;
    raw[1] = 0x81;
    raw[2] = 200;
    fill_pattern(raw + 3, 200);

    SeosTlvCursor cursor;
    SeosTlvObject object;
    seos_tlv_cursor_init(&cursor, raw, sizeof(raw));
    munit_assert_true(seos_tlv_read(&cursor, &object));
    munit_assert_uint16(object.tag, ==, 0x85);
    munit_assert_size(object.value_len, ==, 200);
    munit_assert_size(object.value_offset, ==, 3);
    munit_assert_memory_equal(200, object.value, raw + 3);

    return MUNIT_OK;
}

static MunitResult test_read_long_form_two_octets(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    uint8_t raw[4 + 300];
    raw[0] = 0x85;
    raw[1] = 0x82;
    raw[2] = 300 >> 8;
    raw[3] = 300 & 0xff;
    fill_pattern(raw + 4, 300);

    SeosTlvCursor cursor;
    SeosTlvObject object;
    seos_tlv_cursor_init(&cursor, raw, sizeof(raw));
    munit_assert_true(seos_tlv_read(&cursor, &object));
    munit_assert_size(object.value_len, ==, 300);
    munit_assert_size(object.value_offset, ==, 4);
    munit_assert_memory_equal(300, object.value, raw + 4);

    return MUNIT_OK;
}

/* 127 is the largest the short form carries and 128 the smallest it cannot;
 * 255 and 256 are the same boundary one octet up. */
static MunitResult test_read_length_boundaries(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    static const size_t lengths[] = {0, 1, 127, 128, 255, 256, 257};

    for(size_t i = 0; i < sizeof(lengths) / sizeof(lengths[0]); i++) {
        size_t len = lengths[i];
        uint8_t raw[4 + 257];
        size_t header_len = seos_tlv_write_header(raw, 0x85, len);
        fill_pattern(raw + header_len, len);

        SeosTlvCursor cursor;
        SeosTlvObject object;
        seos_tlv_cursor_init(&cursor, raw, header_len + len);
        munit_assert_true(seos_tlv_read(&cursor, &object));
        munit_assert_size(object.value_len, ==, len);
        munit_assert_size(object.value_offset, ==, header_len);
        if(len > 0) munit_assert_memory_equal(len, object.value, raw + header_len);
    }

    return MUNIT_OK;
}

static MunitResult test_read_two_octet_tag(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    /* Low five bits of 0xff are all set, so the tag continues into 0x00. */
    const uint8_t raw[] = {0xff, 0x00, 0x02, 0x11, 0x22};
    SeosTlvCursor cursor;
    SeosTlvObject object;

    seos_tlv_cursor_init(&cursor, raw, sizeof(raw));
    munit_assert_true(seos_tlv_read(&cursor, &object));
    munit_assert_uint16(object.tag, ==, 0xff00);
    munit_assert_size(object.value_len, ==, 2);
    munit_assert_size(object.value_offset, ==, 3);

    return MUNIT_OK;
}

/* A tag needing a third octet is refused rather than truncated to two. */
static MunitResult test_read_rejects_three_octet_tag(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    const uint8_t raw[] = {0xff, 0x81, 0x00, 0x01, 0x00};
    SeosTlvCursor cursor;
    SeosTlvObject object;

    seos_tlv_cursor_init(&cursor, raw, sizeof(raw));
    munit_assert_false(seos_tlv_read(&cursor, &object));

    return MUNIT_OK;
}

/* An indefinite length has no octet count and never terminates here. */
static MunitResult test_read_rejects_indefinite_length(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    const uint8_t raw[] = {0x85, 0x80, 0xaa, 0xbb, 0x00, 0x00};
    SeosTlvCursor cursor;
    SeosTlvObject object;

    seos_tlv_cursor_init(&cursor, raw, sizeof(raw));
    munit_assert_false(seos_tlv_read(&cursor, &object));

    return MUNIT_OK;
}

/* Three length octets or more would exceed anything this carries. */
static MunitResult test_read_rejects_oversized_length_form(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    for(uint8_t first = 0x83; first != 0x00; first++) {
        uint8_t raw[8] = {0x85, first, 0x00, 0x00, 0x00, 0x01, 0x02, 0x03};
        SeosTlvCursor cursor;
        SeosTlvObject object;

        seos_tlv_cursor_init(&cursor, raw, sizeof(raw));
        munit_assert_false(seos_tlv_read(&cursor, &object));
    }

    return MUNIT_OK;
}

static MunitResult test_read_rejects_length_past_end(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    /* Claims eight bytes of value with three present. */
    const uint8_t raw[] = {0x85, 0x08, 0xaa, 0xbb, 0xcc};
    SeosTlvCursor cursor;
    SeosTlvObject object;

    seos_tlv_cursor_init(&cursor, raw, sizeof(raw));
    munit_assert_false(seos_tlv_read(&cursor, &object));

    return MUNIT_OK;
}

/* Every prefix of a well-formed object is incomplete, so every one must be
 * refused. A parser that reads past its buffer would find a length here. */
static MunitResult test_read_truncated_at_every_offset(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    uint8_t raw[SEOS_TLV_HEADER_MAX + 300];
    static const size_t lengths[] = {3, 200, 300};

    for(size_t i = 0; i < sizeof(lengths) / sizeof(lengths[0]); i++) {
        size_t len = lengths[i];
        size_t header_len = seos_tlv_write_header(raw, 0xff00, len);
        fill_pattern(raw + header_len, len);
        size_t full = header_len + len;

        for(size_t cut = 0; cut < full; cut++) {
            SeosTlvCursor cursor;
            SeosTlvObject object;
            seos_tlv_cursor_init(&cursor, raw, cut);
            munit_assert_false(seos_tlv_read(&cursor, &object));
        }

        /* The untruncated object still reads, so the sweep above is not
         * passing by refusing everything. */
        SeosTlvCursor cursor;
        SeosTlvObject object;
        seos_tlv_cursor_init(&cursor, raw, full);
        munit_assert_true(seos_tlv_read(&cursor, &object));
        munit_assert_size(object.value_len, ==, len);
    }

    return MUNIT_OK;
}

/* Several objects one after another, which is how a protected message is
 * laid out. */
static MunitResult test_read_walks_a_sequence(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    const uint8_t raw[] = {
        0x85, 0x02, 0x11, 0x22, /* cryptogram */
        0x99, 0x02, 0x90, 0x00, /* status */
        0x8e, 0x04, 0xde, 0xad, 0xbe, 0xef, /* checksum */
    };
    const uint16_t expected_tags[] = {0x85, 0x99, 0x8e};
    const size_t expected_offsets[] = {2, 6, 10};

    SeosTlvCursor cursor;
    seos_tlv_cursor_init(&cursor, raw, sizeof(raw));

    for(size_t i = 0; i < 3; i++) {
        SeosTlvObject object;
        munit_assert_false(seos_tlv_cursor_done(&cursor));
        munit_assert_true(seos_tlv_read(&cursor, &object));
        munit_assert_uint16(object.tag, ==, expected_tags[i]);
        munit_assert_size(object.value_offset, ==, expected_offsets[i]);
    }
    munit_assert_true(seos_tlv_cursor_done(&cursor));

    return MUNIT_OK;
}

/* A constructed object carries objects, so a second cursor over its value
 * walks one level down. */
static MunitResult test_read_nested(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    const uint8_t raw[] = {
        0x7c,
        0x04,
        0x81,
        0x02,
        0xab,
        0xcd,
    };
    SeosTlvCursor outer;
    SeosTlvObject wrapper;

    seos_tlv_cursor_init(&outer, raw, sizeof(raw));
    munit_assert_true(seos_tlv_read(&outer, &wrapper));
    munit_assert_uint16(wrapper.tag, ==, 0x7c);
    munit_assert_size(wrapper.value_len, ==, 4);

    SeosTlvCursor inner;
    SeosTlvObject nested;
    seos_tlv_cursor_init(&inner, wrapper.value, wrapper.value_len);
    munit_assert_true(seos_tlv_read(&inner, &nested));
    munit_assert_uint16(nested.tag, ==, 0x81);
    munit_assert_size(nested.value_len, ==, 2);
    munit_assert_uint8(nested.value[0], ==, 0xab);
    munit_assert_true(seos_tlv_cursor_done(&inner));

    return MUNIT_OK;
}

/* Reading at an offset leaves the caller's position alone, for callers that
 * work in offsets rather than walking. */
static MunitResult test_read_at_offset(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    const uint8_t raw[] = {0x85, 0x02, 0x11, 0x22, 0x99, 0x02, 0x90, 0x00};
    SeosTlvObject object;

    munit_assert_true(seos_tlv_read_at(raw, sizeof(raw), 4, &object));
    munit_assert_uint16(object.tag, ==, 0x99);
    munit_assert_size(object.header_offset, ==, 4);
    munit_assert_size(object.value_offset, ==, 6);
    munit_assert_size(object.value_len, ==, 2);

    /* Past the end is refused rather than read. */
    munit_assert_false(seos_tlv_read_at(raw, sizeof(raw), sizeof(raw), &object));

    return MUNIT_OK;
}

/* A tag list is tags one after another with no lengths between them. */
static MunitResult test_read_bare_tags(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    const uint8_t raw[] = {0x85, 0xff, 0x00, 0x5c};
    size_t offset = 0;
    uint16_t tag = 0;

    munit_assert_true(seos_tlv_read_tag(raw, sizeof(raw), &offset, &tag));
    munit_assert_uint16(tag, ==, 0x85);
    munit_assert_size(offset, ==, 1);

    munit_assert_true(seos_tlv_read_tag(raw, sizeof(raw), &offset, &tag));
    munit_assert_uint16(tag, ==, 0xff00);
    munit_assert_size(offset, ==, 3);

    munit_assert_true(seos_tlv_read_tag(raw, sizeof(raw), &offset, &tag));
    munit_assert_uint16(tag, ==, 0x5c);
    munit_assert_size(offset, ==, 4);

    /* Nothing left to read. */
    munit_assert_false(seos_tlv_read_tag(raw, sizeof(raw), &offset, &tag));

    /* A two octet tag with its second octet missing is refused. */
    const uint8_t truncated[] = {0xff};
    offset = 0;
    munit_assert_false(seos_tlv_read_tag(truncated, sizeof(truncated), &offset, &tag));

    return MUNIT_OK;
}

/* ---- writing ---- */

static MunitResult test_length_size(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    munit_assert_size(seos_tlv_length_size(0), ==, 1);
    munit_assert_size(seos_tlv_length_size(127), ==, 1);
    munit_assert_size(seos_tlv_length_size(128), ==, 2);
    munit_assert_size(seos_tlv_length_size(255), ==, 2);
    munit_assert_size(seos_tlv_length_size(256), ==, 3);
    munit_assert_size(seos_tlv_length_size(65535), ==, 3);
    return MUNIT_OK;
}

static MunitResult test_write_header_uses_shortest_form(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    uint8_t out[SEOS_TLV_HEADER_MAX];

    munit_assert_size(seos_tlv_write_header(out, 0x85, 3), ==, 2);
    munit_assert_uint8(out[0], ==, 0x85);
    munit_assert_uint8(out[1], ==, 0x03);

    munit_assert_size(seos_tlv_write_header(out, 0x85, 128), ==, 3);
    munit_assert_uint8(out[1], ==, 0x81);
    munit_assert_uint8(out[2], ==, 0x80);

    munit_assert_size(seos_tlv_write_header(out, 0x85, 300), ==, 4);
    munit_assert_uint8(out[1], ==, 0x82);
    munit_assert_uint8(out[2], ==, 0x01);
    munit_assert_uint8(out[3], ==, 0x2c);

    /* A two octet tag takes both octets ahead of the length. */
    munit_assert_size(seos_tlv_write_header(out, 0xff00, 4), ==, 3);
    munit_assert_uint8(out[0], ==, 0xff);
    munit_assert_uint8(out[1], ==, 0x00);
    munit_assert_uint8(out[2], ==, 0x04);

    return MUNIT_OK;
}

/* What is written reads back as what went in, across every length form. */
static MunitResult test_write_read_round_trip(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    static const size_t lengths[] = {0, 1, 127, 128, 129, 255, 256, 400};
    static const uint16_t tags[] = {0x85, 0xff00};
    uint8_t value[400];
    fill_pattern(value, sizeof(value));

    for(size_t t = 0; t < sizeof(tags) / sizeof(tags[0]); t++) {
        for(size_t i = 0; i < sizeof(lengths) / sizeof(lengths[0]); i++) {
            size_t len = lengths[i];
            BitBuffer* out = bit_buffer_alloc(SEOS_TLV_HEADER_MAX + sizeof(value));
            seos_tlv_append(out, tags[t], value, len);

            SeosTlvCursor cursor;
            SeosTlvObject object;
            seos_tlv_cursor_init(
                &cursor, bit_buffer_get_data(out), bit_buffer_get_size_bytes(out));
            munit_assert_true(seos_tlv_read(&cursor, &object));
            munit_assert_uint16(object.tag, ==, tags[t]);
            munit_assert_size(object.value_len, ==, len);
            if(len > 0) munit_assert_memory_equal(len, object.value, value);
            munit_assert_true(seos_tlv_cursor_done(&cursor));

            bit_buffer_free(out);
        }
    }

    return MUNIT_OK;
}

static MunitTest test_tlv_cases[] = {
    {(char*)"/read/short-form", test_read_short_form, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/read/long-form-1",
     test_read_long_form_one_octet,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/read/long-form-2",
     test_read_long_form_two_octets,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/read/length-boundaries",
     test_read_length_boundaries,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/read/two-octet-tag",
     test_read_two_octet_tag,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/read/three-octet-tag",
     test_read_rejects_three_octet_tag,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/read/indefinite-length",
     test_read_rejects_indefinite_length,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/read/oversized-length-form",
     test_read_rejects_oversized_length_form,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/read/length-past-end",
     test_read_rejects_length_past_end,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/read/truncated-sweep",
     test_read_truncated_at_every_offset,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/read/sequence", test_read_walks_a_sequence, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/read/nested", test_read_nested, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/read/at-offset", test_read_at_offset, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/read/bare-tags", test_read_bare_tags, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/write/length-size", test_length_size, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/write/shortest-form",
     test_write_header_uses_shortest_form,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/write/round-trip",
     test_write_read_round_trip,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
};

MunitSuite test_tlv_suite = {
    (char*)"/tlv",
    test_tlv_cases,
    NULL,
    1,
    MUNIT_SUITE_OPTION_NONE,
};
