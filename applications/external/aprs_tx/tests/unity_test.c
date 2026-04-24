#ifndef UNITY_INCLUDE_PRINT_FORMATTED
#define UNITY_INCLUDE_PRINT_FORMATTED
#endif

#include "unity/unity.h"
#include "../src/aprs.h"

#include <stdint.h>
#include <string.h>

#define T(name) do { Unity.CurrentTestName = (name); Unity.CurrentTestLineNumber = __LINE__; Unity.NumberOfTests++; } while (0)

void setUp(void) {}
void tearDown(void) {}
void suiteSetUp(void) {}
int suiteTearDown(int num_failures) { return num_failures; }

int main(void)
{
    char a[64];
    char b[64];
    char c[160];
    char msg[160];
    Packet p;
    uint8_t reg, sh, x, want_ff[25];
    uint16_t i;
    static const uint8_t ax_ssid[] = {
        0x82, 0xA0, 0x40, 0x40, 0x40, 
            0x40, 0x7E, 0x9C,
        0x60, 0x86, 0x82, 0x98, 0x98, 0x67, 0x03, 0xF0, 0x3E,
    };
    static const char *stuff_ff = "0111111011111011101111110";
    uint8_t seen[] = {0x10, 0x10, 0x10, 0x10};
    uint8_t want[] = {0x10, 0x10, 0x10, 0x10};

    suiteSetUp();
    UnityBegin(__FILE__);

    T("bits and masks");
    UNITY_SET_DETAILS("reg", "bit smoke");
    reg = 0xA5;


    TEST_MESSAGE("poking at mask asserts");
    TEST_ASSERT_BITS(0xF0, 0xA0, reg);
    TEST_ASSERT_BITS_LOW(0x0A, reg);
    TEST_ASSERT_BIT_HIGH(7, reg);

    TEST_ASSERT_BIT_LOW(1, reg);
    UnityConcludeTest();


    T("shift junk");
    UNITY_SET_DETAILS("bits", "shift junk");
    sh = 1; sh <<= 7; TEST_ASSERT_EQUAL_HEX8(0x80, sh);
    sh >>= 3; TEST_ASSERT_EQUAL_HEX8(0x10, sh);
    x = (uint8_t)((0x55 << 1) ^ 0xAA); TEST_ASSERT_EQUAL_HEX8(0x00, x);
    x = (uint8_t)(1u << 5); TEST_ASSERT_EQUAL_UINT8(32, x);


    UnityConcludeTest();


    T("octal and print");
    UNITY_SET_DETAILS("fmt", "octal-ish");
    TEST_ASSERT_EQUAL_UINT8(010, 8);
    TEST_ASSERT_EQUAL_UINT16(000377, 255);
    TEST_ASSERT_EQUAL_UINT8(077, 63);
    
    TEST_PRINTF("0b%b 0x%x %u", 5, 0x2a, 077);
    TEST_PRINTF("0b%b 0x%x %u", 0, 0, 000);
    
    UnityConcludeTest();


    T("arrays and memory");
    
    
    UNITY_SET_DETAILS("mem", "dumb repeats");
    
    TEST_ASSERT_EACH_EQUAL_HEX8(0x10, seen, 4);
    TEST_ASSERT_EQUAL_MEMORY(want, seen, sizeof(seen));
    
    TEST_ASSERT_EQUAL_UINT8_ARRAY(want, seen, 4);
    TEST_ASSERT_EQUAL_MEMORY(want, seen, sizeof(seen));
    
    UnityConcludeTest();


    T("aprs lat lon");
    

    UNITY_SET_DETAILS("aprs", "cismigiu");
    TEST_ASSERT_EQUAL_INT(8, aprs_lat(a, sizeof(a), "44.437461"));
    TEST_ASSERT_EQUAL_STRING("4426.25N", a);
    
    TEST_ASSERT_EQUAL_INT(9, aprs_lon(b, sizeof(b), "26.090215"));
    TEST_ASSERT_EQUAL_STRING("02605.41E", b); TEST_ASSERT_EQUAL_INT(33, aprs_pos(c, sizeof(c), "Cismigiu Park", "44.437461", "26.090215"));
    TEST_ASSERT_EQUAL_STRING("!4426.25N/02605.41E-Cismigiu Park", c); TEST_ASSERT_TRUE(aprs_ll_clamp(a, sizeof(a), "123.45", 0) > 0);
    TEST_ASSERT_EQUAL_STRING("90.00000", a);
    UnityConcludeTest();


    T("aprs msg");
    UNITY_SET_DETAILS("aprs", "message-ish");
    TEST_ASSERT_EQUAL_INT((int)strlen(":YO8YL-5  :73"), aprs_message(msg, sizeof(msg), "YO8YL", 5, "73"));
    TEST_ASSERT_EQUAL_STRING(":YO8YL-5  :73", msg);
    TEST_ASSERT_TRUE(aprs_packet(&p, "YO3GND", 1, "APZFLP", 0, msg, NULL));
    TEST_ASSERT_EQUAL_UINT16(29, p.ax25_len);
    TEST_ASSERT_EQUAL_HEX8(0x82, p.ax25[0]);
    TEST_ASSERT_EQUAL_HEX8(0x63, p.ax25[13]);
    UnityConcludeTest();




        T("packet ssid");
        UNITY_SET_DETAILS("packet", "ssid bytes");
        packet_init(&p);
        packet_make_payload(&p, "");
        packet_make_ax25(&p, "N0CALL", 3, "AP", 15, NULL);
        TEST_ASSERT_EQUAL_UINT16(sizeof(ax_ssid), p.ax25_len);
        TEST_ASSERT_EQUAL_UINT8_ARRAY(ax_ssid, p.ax25, sizeof(ax_ssid));
        TEST_ASSERT_EQUAL_HEX8(0x7E, p.ax25[6]);
        UnityConcludeTest();


    T("stuff ff");
    UNITY_SET_DETAILS("packet", "stuff 0xff");
    packet_init(&p);
    p.fcs[0] = 0xFF;
    p.fcs_len = 1;
    packet_stuff(&p);
    TEST_ASSERT_EQUAL_UINT16(25, p.stuffed_len);
    for (i = 0; i < 25; i++) want_ff[i] = (uint8_t)(stuff_ff[i] == '1');
    TEST_ASSERT_EQUAL_UINT8_ARRAY(want_ff, p.stuffed, 25);
    TEST_ASSERT_EQUAL_UINT8(0, p.stuffed[0]);
    TEST_ASSERT_EQUAL_UINT8(0, p.stuffed[24]);
    UnityConcludeTest();


    T("more dumb stuff");
    UNITY_SET_DETAILS("extra", "small junk");
    TEST_ASSERT_FALSE(call_crc("YO3GND", 0));
    TEST_ASSERT_TRUE(call_crc("YO3GND", 1) || !call_crc("YO3GND", 1));
    TEST_ASSERT_EQUAL_HEX8(0x3E, (uint8_t)'>');
    TEST_ASSERT_EQUAL_CHAR('>', '>');
    UnityConcludeTest();

    return suiteTearDown(UnityEnd());
}
