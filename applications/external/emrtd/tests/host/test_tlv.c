/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Filip Sedivy
 *
 * The BER-TLV parser.
 *
 * This is the layer a hostile or simply broken chip reaches first, so half of
 * this suite is malformed input. The adversarial cases are parsed out of heap
 * blocks allocated to the exact length of the input: a parser that reads one
 * byte past what it was given is then a sanitizer failure rather than a test
 * that happens to pass on whatever followed in memory.
 *
 * The well formed vectors are the EF.COM of ICAO Doc 9303 part 11 appendix D.4
 * and the cases from the Python reference implementation's own test suite.
 */

#include "emrtd_test.h"

#include <stdlib.h>

#include "../../protocol/emrtd_tlv.h"

/** The ICAO EF.COM example: LDS 1.6, Unicode 4.0.0, DG1 and DG2 present. */
static const char* const icao_ef_com = "60145F0104303130365F36063034303030305C026175";

/** Copy a hex literal into a block of exactly its own length. */
static uint8_t* exact_alloc(const char* hex, size_t* out_len) {
    uint8_t scratch[512];
    const size_t n = emrtd_test_hex(hex, scratch, sizeof(scratch));
    uint8_t* buf = malloc(n == 0 ? 1 : n);
    if(buf != NULL && n > 0) {
        memcpy(buf, scratch, n);
    }
    *out_len = n;
    return buf;
}

static void test_parse_ef_com(void) {
    emrtd_test_begin("the ICAO EF.COM example parses field by field");

    uint8_t data[64];
    const size_t len = emrtd_test_hex(icao_ef_com, data, sizeof(data));
    TEST_EQ_INT(len, 22);

    EmrtdTlv root;
    TEST_CHECK(emrtd_tlv_parse_first(data, len, &root));
    TEST_EQ_INT(root.tag, 0x60);
    TEST_CHECK(root.constructed);
    TEST_EQ_INT(root.value_len, 20);
    TEST_EQ_INT(root.total_len, 22);
    TEST_CHECK(root.start == data);
    TEST_CHECK(root.value == data + 2);

    EmrtdTlv node;
    TEST_CHECK(emrtd_tlv_find(root.value, root.value_len, 0x5F01, &node));
    TEST_EQ_HEX(node.value, node.value_len, "30313036");
    TEST_CHECK(!node.constructed);

    TEST_CHECK(emrtd_tlv_find(root.value, root.value_len, 0x5F36, &node));
    TEST_EQ_HEX(node.value, node.value_len, "303430303030");

    TEST_CHECK(emrtd_tlv_find(root.value, root.value_len, 0x5C, &node));
    TEST_EQ_HEX(node.value, node.value_len, "6175");

    emrtd_test_begin("a tag that is not there is reported, not invented");
    TEST_CHECK(!emrtd_tlv_find(root.value, root.value_len, 0x5F1F, &node));
}

static void test_iteration(void) {
    emrtd_test_begin("iteration walks siblings and ends exhausted");

    uint8_t data[64];
    const size_t len = emrtd_test_hex(icao_ef_com, data, sizeof(data));

    EmrtdTlv root;
    TEST_CHECK(emrtd_tlv_parse_first(data, len, &root));

    EmrtdTlvIter iter;
    EmrtdTlv node;
    emrtd_tlv_iter_children(&iter, &root);

    uint32_t tags[4] = {0, 0, 0, 0};
    size_t count = 0;
    while(emrtd_tlv_iter_next(&iter, &node) && count < 4) {
        tags[count++] = node.tag;
    }
    TEST_EQ_INT(count, 3);
    TEST_EQ_INT(tags[0], 0x5F01);
    TEST_EQ_INT(tags[1], 0x5F36);
    TEST_EQ_INT(tags[2], 0x5C);
    TEST_CHECK(emrtd_tlv_iter_exhausted(&iter));

    emrtd_test_begin("an empty buffer is exhausted rather than broken");
    emrtd_tlv_iter_init(&iter, data, 0);
    TEST_CHECK(!emrtd_tlv_iter_next(&iter, &node));
    TEST_CHECK(emrtd_tlv_iter_exhausted(&iter));

    emrtd_test_begin("a primitive node has no children");
    EmrtdTlv leaf;
    TEST_CHECK(emrtd_tlv_find(root.value, root.value_len, 0x5C, &leaf));
    emrtd_tlv_iter_children(&iter, &leaf);
    TEST_CHECK(!emrtd_tlv_iter_next(&iter, &node));
    TEST_CHECK(emrtd_tlv_iter_exhausted(&iter));
}

static void test_lengths_and_tags(void) {
    emrtd_test_begin("a two byte length is read big endian");

    /* SEQUENCE, length 0x012C = 300, followed by that many bytes. */
    uint8_t* data = malloc(304);
    TEST_CHECK(data != NULL);
    if(data == NULL) {
        return;
    }
    data[0] = 0x30;
    data[1] = 0x82;
    data[2] = 0x01;
    data[3] = 0x2C;
    memset(data + 4, 0xAA, 300);

    EmrtdTlv node;
    TEST_CHECK(emrtd_tlv_parse_first(data, 304, &node));
    TEST_EQ_INT(node.value_len, 300);
    TEST_EQ_INT(node.total_len, 304);
    free(data);

    emrtd_test_begin("a multi byte tag is packed big endian");
    uint8_t buf[16];
    size_t len = emrtd_test_hex("5F1F025858", buf, sizeof(buf));
    TEST_CHECK(emrtd_tlv_parse_first(buf, len, &node));
    TEST_EQ_INT(node.tag, 0x5F1F);
    TEST_EQ_HEX(node.value, node.value_len, "5858");

    char text[16];
    TEST_EQ_STR(emrtd_tlv_tag_str(node.tag, text, sizeof(text)), "5F1F");
    TEST_EQ_STR(emrtd_tlv_tag_str(0x60, text, sizeof(text)), "60");
    TEST_EQ_STR(emrtd_tlv_tag_str(0x7F61, text, sizeof(text)), "7F61");
    TEST_EQ_STR(emrtd_tlv_tag_str(0x5F1F, text, 3), "5F");

    emrtd_test_begin("a one byte length of 0x7F is still the short form");
    uint8_t* long_leaf = malloc(2 + 0x7F);
    TEST_CHECK(long_leaf != NULL);
    if(long_leaf != NULL) {
        long_leaf[0] = 0x04;
        long_leaf[1] = 0x7F;
        memset(long_leaf + 2, 0x11, 0x7F);
        TEST_CHECK(emrtd_tlv_parse_first(long_leaf, 2 + 0x7F, &node));
        TEST_EQ_INT(node.value_len, 0x7F);
        free(long_leaf);
    }

    emrtd_test_begin("a zero length value is legal and empty");
    len = emrtd_test_hex("A100", buf, sizeof(buf));
    TEST_CHECK(emrtd_tlv_parse_first(buf, len, &node));
    TEST_EQ_INT(node.value_len, 0);
    TEST_EQ_INT(node.total_len, 2);
    TEST_CHECK(node.constructed);
}

static void test_recursive_search(void) {
    emrtd_test_begin("the recursive search descends into children");

    uint8_t data[64];
    const size_t len = emrtd_test_hex(icao_ef_com, data, sizeof(data));

    EmrtdTlv node;
    TEST_CHECK(emrtd_tlv_find_recursive(data, len, 0x5F36, &node));
    TEST_EQ_HEX(node.value, node.value_len, "303430303030");

    /* The outer node itself counts as part of the subtree. */
    TEST_CHECK(emrtd_tlv_find_recursive(data, len, 0x60, &node));
    TEST_EQ_INT(node.value_len, 20);

    TEST_CHECK(!emrtd_tlv_find_recursive(data, len, 0x9999, &node));

    emrtd_test_begin("a flat search does not descend");
    TEST_CHECK(!emrtd_tlv_find(data, len, 0x5F36, &node));

    emrtd_test_begin("nesting deeper than the limit is abandoned, not followed");
    /*
     * Twenty nested constructed nodes around a single leaf. The parser must
     * come back without having recursed all the way, which on a device means
     * without having eaten the worker's eight kilobyte stack.
     */
    const size_t depth = 20;
    const size_t total = depth * 2 + 3;
    uint8_t* deep = malloc(total);
    TEST_CHECK(deep != NULL);
    if(deep != NULL) {
        for(size_t i = 0; i < depth; i++) {
            deep[i * 2] = 0xA0;
            deep[i * 2 + 1] = (uint8_t)(total - (i + 1) * 2);
        }
        deep[depth * 2] = 0x80;
        deep[depth * 2 + 1] = 0x01;
        deep[depth * 2 + 2] = 0x42;
        TEST_CHECK(!emrtd_tlv_find_recursive(deep, total, 0x80, &node));

        /* The same leaf is found when it sits within the limit. */
        TEST_CHECK(emrtd_tlv_find_recursive(deep + 16, total - 16, 0x80, &node));
        free(deep);
    }
}

static void test_malformed(void) {
    /*
     * Each case is parsed out of a block sized exactly to the input, so that
     * a read past the end is a sanitizer error rather than luck.
     */
    static const struct {
        const char* hex;
        const char* what;
    } cases[] = {
        {"60FF", "a length that runs past the end of the data"},
        {"3080FFFF", "the indefinite length form"},
        {"30FF01", "the reserved length byte FF"},
        {"3085010203", "a length field wider than four bytes"},
        {"30", "a node that is only a tag"},
        {"5F", "a multi byte tag cut off after the first byte"},
        {"5F1F", "a tag with no length at all"},
        {"5F9F9F9F1F0100", "a tag longer than four bytes"},
        {"00", "the padding byte 00 as a tag"},
        {"FF01AA", "the reserved byte FF as a tag"},
        {"6482FFFF00", "a two byte length larger than the buffer"},
        {"3003AABB", "a value one byte shorter than declared"},
    };

    for(size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        emrtd_test_begin(cases[i].what);

        size_t len = 0;
        uint8_t* data = exact_alloc(cases[i].hex, &len);
        TEST_CHECK(data != NULL);
        if(data == NULL) {
            continue;
        }

        EmrtdTlv node;
        TEST_CHECK(!emrtd_tlv_parse_first(data, len, &node));

        EmrtdTlvIter iter;
        emrtd_tlv_iter_init(&iter, data, len);
        TEST_CHECK(!emrtd_tlv_iter_next(&iter, &node));
        /* Rejected, and reported as rejected rather than as the end. */
        TEST_CHECK(!emrtd_tlv_iter_exhausted(&iter));

        /* A second call after a failure must stay where it is. */
        TEST_CHECK(!emrtd_tlv_iter_next(&iter, &node));
        TEST_CHECK(!emrtd_tlv_iter_exhausted(&iter));

        /* Neither search may walk off the block either. */
        TEST_CHECK(!emrtd_tlv_find(data, len, 0x30, &node));
        TEST_CHECK(!emrtd_tlv_find_recursive(data, len, 0x30, &node));

        free(data);
    }

    emrtd_test_begin("an empty buffer has no first node");
    EmrtdTlv node;
    TEST_CHECK(!emrtd_tlv_parse_first(NULL, 0, &node));
    uint8_t byte = 0x30;
    TEST_CHECK(!emrtd_tlv_parse_first(&byte, 0, &node));

    emrtd_test_begin("a valid node followed by rubbish yields the node, then a failure");
    size_t len = 0;
    uint8_t* data = exact_alloc("0401AA60FF", &len);
    TEST_CHECK(data != NULL);
    if(data != NULL) {
        EmrtdTlvIter iter;
        emrtd_tlv_iter_init(&iter, data, len);
        TEST_CHECK(emrtd_tlv_iter_next(&iter, &node));
        TEST_EQ_INT(node.tag, 0x04);
        TEST_CHECK(!emrtd_tlv_iter_next(&iter, &node));
        TEST_CHECK(!emrtd_tlv_iter_exhausted(&iter));
        free(data);
    }

    emrtd_test_begin("a constructed node holding rubbish is itself still valid");
    /* A biometric header does this, so it must not fail the whole file. */
    data = exact_alloc("A103FFFFFF", &len);
    TEST_CHECK(data != NULL);
    if(data != NULL) {
        TEST_CHECK(emrtd_tlv_parse_first(data, len, &node));
        TEST_CHECK(node.constructed);
        TEST_EQ_INT(node.value_len, 3);

        EmrtdTlvIter iter;
        EmrtdTlv child;
        emrtd_tlv_iter_children(&iter, &node);
        TEST_CHECK(!emrtd_tlv_iter_next(&iter, &child));
        TEST_CHECK(!emrtd_tlv_iter_exhausted(&iter));
        free(data);
    }
}

static void test_total_length(void) {
    emrtd_test_begin("the size of a file is read from its first bytes");

    uint8_t header[8];
    size_t len = emrtd_test_hex("6014", header, sizeof(header));
    TEST_EQ_INT(emrtd_tlv_total_length(header, len), 22);

    len = emrtd_test_hex("308201 2C", header, sizeof(header));
    TEST_EQ_INT(emrtd_tlv_total_length(header, len), 304);

    len = emrtd_test_hex("7782058E", header, sizeof(header));
    TEST_EQ_INT(emrtd_tlv_total_length(header, len), 1426);

    emrtd_test_begin("an incomplete header reports nothing yet");
    len = emrtd_test_hex("30", header, sizeof(header));
    TEST_EQ_INT(emrtd_tlv_total_length(header, len), 0);
    len = emrtd_test_hex("3082", header, sizeof(header));
    TEST_EQ_INT(emrtd_tlv_total_length(header, len), 0);
    len = emrtd_test_hex("308201", header, sizeof(header));
    TEST_EQ_INT(emrtd_tlv_total_length(header, len), 0);
    TEST_EQ_INT(emrtd_tlv_total_length(NULL, 4), 0);

    emrtd_test_begin("a header the parser refuses has no size");
    len = emrtd_test_hex("3080", header, sizeof(header));
    TEST_EQ_INT(emrtd_tlv_total_length(header, len), 0);
    len = emrtd_test_hex("0082", header, sizeof(header));
    TEST_EQ_INT(emrtd_tlv_total_length(header, len), 0);
}

static void test_read_uint(void) {
    emrtd_test_begin("integers up to four bytes are read");

    uint8_t data[16];
    EmrtdTlv node;
    uint32_t value = 0;

    size_t len = emrtd_test_hex("02010D", data, sizeof(data));
    TEST_CHECK(emrtd_tlv_parse_first(data, len, &node));
    TEST_CHECK(emrtd_tlv_read_uint(&node, &value));
    TEST_EQ_INT(value, 13);

    len = emrtd_test_hex("020400010000", data, sizeof(data));
    TEST_CHECK(emrtd_tlv_parse_first(data, len, &node));
    TEST_CHECK(emrtd_tlv_read_uint(&node, &value));
    TEST_EQ_INT(value, 0x00010000);

    emrtd_test_begin("the DER sign byte does not make an integer too wide");
    len = emrtd_test_hex("020500FFFFFFFF", data, sizeof(data));
    TEST_CHECK(emrtd_tlv_parse_first(data, len, &node));
    TEST_CHECK(emrtd_tlv_read_uint(&node, &value));
    TEST_EQ_INT(value, 0xFFFFFFFFu);

    emrtd_test_begin("an integer wider than four bytes is refused");
    len = emrtd_test_hex("02050100000000", data, sizeof(data));
    TEST_CHECK(emrtd_tlv_parse_first(data, len, &node));
    TEST_CHECK(!emrtd_tlv_read_uint(&node, &value));

    emrtd_test_begin("an empty integer is refused");
    len = emrtd_test_hex("0200", data, sizeof(data));
    TEST_CHECK(emrtd_tlv_parse_first(data, len, &node));
    TEST_CHECK(!emrtd_tlv_read_uint(&node, &value));
}

static void test_length_encoding(void) {
    emrtd_test_begin("lengths are encoded in the fewest bytes");

    uint8_t out[4];
    TEST_EQ_INT(emrtd_tlv_encode_length(0, out), 1);
    TEST_EQ_HEX(out, 1, "00");
    TEST_EQ_INT(emrtd_tlv_encode_length(0x7F, out), 1);
    TEST_EQ_HEX(out, 1, "7F");
    TEST_EQ_INT(emrtd_tlv_encode_length(0x80, out), 2);
    TEST_EQ_HEX(out, 2, "8180");
    TEST_EQ_INT(emrtd_tlv_encode_length(0xFF, out), 2);
    TEST_EQ_HEX(out, 2, "81FF");
    TEST_EQ_INT(emrtd_tlv_encode_length(0x0100, out), 3);
    TEST_EQ_HEX(out, 3, "820100");
    TEST_EQ_INT(emrtd_tlv_encode_length(0x058E, out), 3);
    TEST_EQ_HEX(out, 3, "82058E");
    TEST_EQ_INT(emrtd_tlv_encode_length(0x010000, out), 4);
    TEST_EQ_HEX(out, 4, "83010000");

    emrtd_test_begin("the encoded size agrees with the encoder");
    const size_t lengths[] = {0, 1, 0x7F, 0x80, 0xFF, 0x100, 0xFFFF, 0x10000, 0xFFFFFF};
    for(size_t i = 0; i < sizeof(lengths) / sizeof(lengths[0]); i++) {
        TEST_EQ_INT(
            emrtd_tlv_encoded_length_size(lengths[i]), emrtd_tlv_encode_length(lengths[i], out));
    }

    emrtd_test_begin("a length beyond three bytes is refused rather than truncated");
    TEST_EQ_INT(emrtd_tlv_encoded_length_size(0x1000000), 0);
    TEST_EQ_INT(emrtd_tlv_encode_length(0x1000000, out), 0);
}

static void test_round_trip(void) {
    emrtd_test_begin("an encoded length is read back by the parser");

    for(size_t value_len = 0; value_len <= 0x200; value_len += 0x3F) {
        uint8_t* node = malloc(1 + 4 + value_len);
        TEST_CHECK(node != NULL);
        if(node == NULL) {
            continue;
        }
        node[0] = 0x04;
        const size_t length_size = emrtd_tlv_encode_length(value_len, node + 1);
        memset(node + 1 + length_size, 0x5A, value_len);

        EmrtdTlv parsed;
        TEST_CHECK(emrtd_tlv_parse_first(node, 1 + length_size + value_len, &parsed));
        TEST_EQ_INT(parsed.value_len, value_len);
        TEST_EQ_INT(parsed.total_len, 1 + length_size + value_len);
        free(node);
    }
}

void test_suite_tlv(void) {
    test_parse_ef_com();
    test_iteration();
    test_lengths_and_tags();
    test_recursive_search();
    test_malformed();
    test_total_length();
    test_read_uint();
    test_length_encoding();
    test_round_trip();
}
