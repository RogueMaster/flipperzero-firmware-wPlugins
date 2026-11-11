/* Host: cc -std=c11 -Wall -Wextra -Werror -pedantic -I. \
 * tests/uhf_frequency_test.c uhf_frequency.c uhf_protocol.c -o /tmp/uhf_frequency_test \
 * && /tmp/uhf_frequency_test */
#include "uhf_frequency.h"
#include "uhf_protocol.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void roundtrip(const uint8_t* payload, size_t size) {
    UhfFrequency frequency, reparsed;
    uint8_t encoded[7];
    size_t length = 0;
    assert(uhf_frequency_parse(payload, size, &frequency));
    assert(uhf_frequency_valid(&frequency));
    assert(uhf_frequency_encode(&frequency, encoded, sizeof(encoded), &length));
    assert(length == size && memcmp(encoded, payload, size) == 0);
    assert(uhf_frequency_parse(encoded, length, &reparsed));
    assert(uhf_frequency_equal(&frequency, &reparsed));
}

static void rejects(const uint8_t* payload, size_t size) {
    UhfFrequency frequency;
    assert(uhf_frequency_preset(UHF_FREQUENCY_REGION_CUSTOM, &frequency));
    const UhfFrequency before = frequency;
    assert(!uhf_frequency_parse(payload, size, &frequency));
    assert(uhf_frequency_equal(&frequency, &before));
}

static void test_pdf_frames(void) {
    const uint8_t payload[] = {0x04, 0x00, 0x19, 0x01, 0x0D, 0xBB, 0xA0};
    const uint8_t set_frame[] = {0xA0, 0x0A, 0x00, 0x78, 0x04, 0x00,
                                 0x19, 0x01, 0x0D, 0xBB, 0xA0, 0x58};
    const uint8_t get_frame[] = {0xA0, 0x0A, 0x00, 0x79, 0x04, 0x00,
                                 0x19, 0x01, 0x0D, 0xBB, 0xA0, 0x57};
    UhfFrequency frequency;
    uint8_t encoded[7], frame[12];
    size_t length, frame_length;
    assert(uhf_frequency_parse(get_frame + 4, sizeof(get_frame) - 5, &frequency));
    assert(frequency.start_khz == 900000 && frequency.spacing_khz == 25 && frequency.count == 1);
    assert(uhf_frequency_encode(&frequency, encoded, sizeof(encoded), &length));
    assert(length == sizeof(payload) && memcmp(encoded, payload, length) == 0);
    assert(uhf_protocol_build_ucm601_frame(0x78, encoded, length, frame, sizeof(frame), &frame_length));
    assert(frame_length == sizeof(set_frame) && memcmp(frame, set_frame, frame_length) == 0);
    assert(uhf_protocol_build_ucm601_frame(0x79, encoded, length, frame, sizeof(frame), &frame_length));
    assert(memcmp(frame, get_frame, frame_length) == 0);
    roundtrip(payload, sizeof(payload));
}

static void test_standard_tables(void) {
    const uint8_t regions[] = {1, 2, 5};
    const uint8_t first[] = {7, 0, 43};
    const uint8_t last[] = {59, 6, 53};
    const uint32_t starts[] = {902000, 865000, 920000};
    const uint32_t ends[] = {928000, 868000, 925000};
    for(size_t r = 0; r < sizeof(regions); ++r) {
        UhfFrequency frequency;
        assert(uhf_frequency_preset(regions[r], &frequency));
        assert(frequency.start_khz == starts[r] && frequency.spacing_khz == 500);
        assert(frequency.start_index == first[r] && frequency.end_index == last[r]);
        assert(frequency.count == last[r] - first[r] + 1);
        assert(frequency.start_khz + (frequency.count - 1U) * 500U == ends[r]);
        for(unsigned i = first[r]; i <= last[r]; ++i) {
            for(unsigned j = i; j <= last[r]; ++j) {
                const uint8_t payload[] = {regions[r], (uint8_t)i, (uint8_t)j};
                roundtrip(payload, sizeof(payload));
                assert(uhf_frequency_parse(payload, sizeof(payload), &frequency));
                assert(frequency.start_khz == starts[r] + (i - first[r]) * 500U);
                assert(frequency.count == j - i + 1);
            }
        }
        frequency.count++;
        assert(!uhf_frequency_valid(&frequency));
    }
    const uint8_t opaque[] = {3, 43, 53};
    UhfFrequency frequency;
    assert(uhf_frequency_parse(opaque, sizeof(opaque), &frequency));
    assert(frequency.start_khz == 0 && frequency.spacing_khz == 0 && frequency.count == 0);
    const UhfFrequency before = frequency;
    assert(!uhf_frequency_preset(3, &frequency));
    assert(!uhf_frequency_preset(0, &frequency));
    assert(uhf_frequency_equal(&before, &frequency));
    roundtrip(opaque, sizeof(opaque));
}

static void test_custom_boundaries(void) {
    UhfFrequency frequency;
    assert(uhf_frequency_preset(4, &frequency));
    frequency.start_khz = 840000;
    frequency.count = 200;
    assert(uhf_frequency_valid(&frequency));
    frequency.count = 255;
    assert(uhf_frequency_valid(&frequency));
    frequency.count = 0;
    assert(!uhf_frequency_valid(&frequency));
    frequency.count = 1;
    frequency.spacing_khz = 24;
    assert(!uhf_frequency_valid(&frequency));
    frequency.spacing_khz = 65535;
    assert(uhf_frequency_valid(&frequency));
    frequency.count = 2;
    assert(uhf_frequency_valid(&frequency));
    frequency.count = 3;
    assert(!uhf_frequency_valid(&frequency));
    frequency.count = 1;
    frequency.start_khz = 839999;
    assert(!uhf_frequency_valid(&frequency));
    frequency.start_khz = 960001;
    assert(!uhf_frequency_valid(&frequency));
    frequency.start_khz = 960000;
    assert(uhf_frequency_valid(&frequency));
    frequency.spacing_khz = 25;
    frequency.count = 2;
    assert(!uhf_frequency_valid(&frequency));
    frequency.start_khz = 959975;
    assert(uhf_frequency_valid(&frequency));
    uint8_t encoded[7];
    size_t length;
    assert(uhf_frequency_encode(&frequency, encoded, sizeof(encoded), &length));
    roundtrip(encoded, length);
    UhfFrequency other = frequency;
    other.start_index = 99;
    other.end_index = 254;
    assert(uhf_frequency_equal(&frequency, &other));
    other.spacing_khz++;
    assert(!uhf_frequency_equal(&frequency, &other));
}

static void test_malformed(void) {
    const uint8_t invalid[][7] = {
        {0, 7, 59}, {6, 7, 59}, {1, 6, 59}, {1, 7, 60}, {1, 59, 7},
        {2, 0, 7}, {5, 42, 53}, {5, 43, 54}, {3, 53, 43},
        {4, 0, 24, 1, 0x0D, 0xBB, 0xA0},
        {4, 0, 25, 0, 0x0D, 0xBB, 0xA0},
        {4, 0, 25, 255, 0x0E, 0xA6, 0x00}, /* 960000 + channels exceeds range */
        {4, 0, 25, 1, 0xFF, 0xFF, 0xFF},
    };
    for(size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i) {
        rejects(invalid[i], i < 9 ? 3 : 7);
    }
    const uint8_t custom[] = {4, 0, 25, 1, 0x0D, 0xBB, 0xA0, 0};
    const uint8_t standard[] = {1, 7, 59, 0, 0, 0, 0, 0};
    for(size_t n = 0; n <= sizeof(custom); ++n) {
        if(n != 7) rejects(custom, n);
        if(n != 3) rejects(standard, n);
    }
    rejects(NULL, 0);
    assert(!uhf_frequency_parse(custom, 7, NULL));
    assert(!uhf_frequency_valid(NULL));
    assert(!uhf_frequency_equal(NULL, NULL));
    assert(!uhf_frequency_preset(1, NULL));
    UhfFrequency frequency;
    assert(uhf_frequency_preset(4, &frequency));
    uint8_t output[7];
    memset(output, 0xAA, sizeof(output));
    size_t length = 99;
    assert(!uhf_frequency_encode(&frequency, output, 6, &length));
    assert(length == 0);
    for(size_t i = 0; i < sizeof(output); ++i) assert(output[i] == 0xAA);
    assert(!uhf_frequency_encode(NULL, output, sizeof(output), &length));
    assert(!uhf_frequency_encode(&frequency, NULL, sizeof(output), &length));
    assert(uhf_frequency_encode(&frequency, output, sizeof(output), NULL));
    assert(strcmp(uhf_frequency_region_name(1), "FCC") == 0);
    assert(strcmp(uhf_frequency_region_name(2), "ETSI") == 0);
    assert(strcmp(uhf_frequency_region_name(3), "CHN_1") == 0);
    assert(strcmp(uhf_frequency_region_name(4), "Custom") == 0);
    assert(strcmp(uhf_frequency_region_name(5), "CHN_2") == 0);
    assert(strcmp(uhf_frequency_region_name(255), "Unknown") == 0);
}

int main(void) {
    test_pdf_frames();
    test_standard_tables();
    test_custom_boundaries();
    test_malformed();
    puts("uhf_frequency: all host tests passed");
    return 0;
}
