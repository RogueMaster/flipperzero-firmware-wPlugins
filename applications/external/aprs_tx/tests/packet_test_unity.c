#ifndef UNITY_INCLUDE_PRINT_FORMATTED
#define UNITY_INCLUDE_PRINT_FORMATTED
#endif

#include "unity/unity.h"
#include "../src/aprs.h"

#include <stdint.h>
#include <string.h>

#ifndef RUN_TEST
#define RUN_TEST(func) UnityDefaultTestRun(func, #func, __LINE__)
#endif

void setUp(void) {}
void tearDown(void) {}
void suiteSetUp(void) {}
int suiteTearDown(int num_failures) { return num_failures; }

static void b01(uint8_t *out, const char *s)
{
    uint16_t i;
    uint16_t n;

    n = (uint16_t)strlen(s);
    for(i = 0; i < n; i++) out[i] = (uint8_t)(s[i] == '1');


}

static void test_payload_and_ax25(void)
{
    static const uint8_t pay[] = { 0x3E, 0x54, 0x45, 0x53, 0x54, 0x31, 0x32, 0x33, };
    static const uint8_t ax[] = {
        0xAE, 0x60, 0xA4, 0x98, 0x88, 0x40, 0x60, 0xB2,
        0x9E, 0x60, 0x8C, 0x98, 0xA0, 0x61, 0x03, 0xF0,
        0x3E, 0x54, 0x45, 0x53, 0x54, 0x31, 0x32, 0x33,
    };
    Packet p;

    /* basic */
    packet_init(&p);
    packet_make_payload(&p, "TEST123");
    TEST_ASSERT_EQUAL_UINT16(sizeof(pay), p.payload_len);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(pay, p.payload, p.payload_len);


    packet_make_ax25(&p, "YO0FLP", 0, "W0RLD", 0, NULL);
    TEST_ASSERT_EQUAL_UINT16(sizeof(ax), p.ax25_len);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(ax, p.ax25, p.ax25_len);
}

static void test_payload_trunc_noisy(void)
{
    static const uint8_t a[] = ">ABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQ";
    Packet p;

    /* trunc */
    packet_init(&p);
    packet_make_payload(&p, "ABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZ");
    TEST_ASSERT_EQUAL_UINT16(sizeof(a) - 1, p.payload_len);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(a, p.payload, p.payload_len);
}

static void test_ax25_ssid_mic(void)
{
    static const uint8_t a[] = {
        0x82, 0xA0, 0x40, 0x40, 0x40, 0x40, 0x7E, 0x9C,
        0x60, 0x86, 0x82, 0x98, 0x98, 0x67, 0x03, 0xF0,
        0x3E,
    };
    Packet p;

    /* ssid */
    packet_init(&p);
    packet_make_payload(&p, "");
    packet_make_ax25(&p, "N0CALL", 3, "AP", 15, NULL);
    TEST_ASSERT_EQUAL_UINT16(sizeof(a), p.ax25_len);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(a, p.ax25, p.ax25_len);
}

static void test_ff_pipe(void)
{
    static const char *a = "0111111011111011101111110";
    static const char *b = "0000000111111000011111110";
    uint8_t w1[40];
    uint8_t w2[40];
    Packet p;

    /* ff */
    packet_init(&p);
    p.fcs[0] = 0xFF;
    p.fcs_len = 1;

    packet_stuff(&p);
    TEST_ASSERT_EQUAL_UINT16(25, p.stuffed_len);
    b01(w1, a);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(w1, p.stuffed, p.stuffed_len);


    packet_nrzi(&p);
    TEST_ASSERT_EQUAL_UINT16(25, p.nrzi_len);
    b01(w2, b);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(w2, p.nrzi, p.nrzi_len);
}

static void testmare(void)
{
    static const uint8_t a[] = ">Hello world, I am Flipper Zero :D";
    static const uint8_t b[] = { 0xAE, 0x60, 0xA4, 0x98, 0x88, 0x40, 0x60, 0xB2, 0x9E, 0x60, 0x8C, 0x98, 0xA0, 0x61, 0x03, 0xF0, 0x3E, 0x48, 0x65, 0x6C, 0x6C, 0x6F, 0x20, 0x77, 0x6F, 0x72, 0x6C, 0x64, 0x2C, 0x20, 0x49, 0x20, 0x61, 0x6D, 0x20, 0x46, 0x6C, 0x69, 0x70, 0x70, 0x65, 0x72, 0x20, 0x5A, 0x65, 0x72, 0x6F, 0x20, 0x3A, 0x44, };
    static const uint8_t c[] = { 0xAE, 0x60, 0xA4, 0x98, 0x88, 0x40, 0x60, 0xB2, 0x9E, 0x60, 0x8C, 0x98, 0xA0, 0x61, 0x03, 0xF0, 0x3E, 0x48, 0x65, 0x6C, 0x6C, 0x6F, 0x20, 0x77, 0x6F, 0x72, 0x6C, 0x64, 0x2C, 0x20, 0x49, 0x20, 0x61, 0x6D, 0x20, 0x46, 0x6C, 0x69, 0x70, 0x70, 0x65, 0x72, 0x20, 0x5A, 0x65, 0x72, 0x6F, 0x20, 0x3A, 0x44, 0x92, 0x7D, };
    Packet p;

    /* mare */
    packet_do_all(&p, "YO0FLP", 0, "W0RLD", 0, "Hello world, I am Flipper Zero :D", NULL);
    TEST_ASSERT_EQUAL_UINT16(sizeof(a) - 1, p.payload_len);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(a, p.payload, p.payload_len);
    TEST_ASSERT_EQUAL_UINT16(sizeof(b), p.ax25_len);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(b, p.ax25, p.ax25_len);
    TEST_ASSERT_EQUAL_UINT16(sizeof(c), p.fcs_len);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(c, p.fcs, p.fcs_len);
    TEST_ASSERT_EQUAL_UINT16(434, p.stuffed_len);
    TEST_ASSERT_EQUAL_UINT16(434, p.nrzi_len);
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_payload_and_ax25);
    RUN_TEST(test_payload_trunc_noisy);
    RUN_TEST(test_ax25_ssid_mic);
    RUN_TEST(test_ff_pipe);
    RUN_TEST(testmare);


    return UNITY_END();
}
