#define UNITY_INCLUDE_PRINT_FORMATTED

#include "unity/unity.h"
#include "../src/aprs.h"

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#ifndef RUN_TEST
#define RUN_TEST(func) UnityDefaultTestRun(func, #func, __LINE__)
#endif

void setUp(void) {}
void tearDown(void) {}
void suiteSetUp(void) {}
int suiteTearDown(int num_failures) { return num_failures; }

static void bits01(uint8_t *want, const char *s)
{
    uint16_t i;
    uint16_t n;

    n = (uint16_t)strlen(s);
    for (i = 0; i < n; i++) want[i] = (uint8_t)(s[i] == '1');


}

static void ax7(uint8_t *out, const char *call, uint8_t ssid, uint8_t last)
{
    int i;
    const char *a;

    a = call;

    for (i = 0; i < 6; i++)
    {
        if (*a) { out[i] = ((uint8_t)*a) << 1; a++; }
        else out[i] = ' ' << 1;
    }

    out[6] = 0x60 | ((ssid & 15) << 1) | (last ? 1 : 0);


}

static void test_lat_lon_and_pos(void)
{
    char a[64];
    char b[64];
    char c[160];

    /* zero */
    UNITY_SET_DETAILS("aprs", "ll");
    TEST_ASSERT_EQUAL_INT(8, aprs_lat(a, sizeof(a), "0.02"));
    TEST_ASSERT_EQUAL_STRING("0001.20N", a);
    TEST_ASSERT_EQUAL_INT(9, aprs_lon(b, sizeof(b), "-0.04"));
    TEST_ASSERT_EQUAL_STRING("00002.40W", b);

    /* cismigiu */
    TEST_ASSERT_EQUAL_INT(8, aprs_lat(a, sizeof(a), "44.437461"));
    TEST_ASSERT_EQUAL_STRING("4426.25N", a);
    TEST_ASSERT_EQUAL_INT(9, aprs_lon(b, sizeof(b), "26.090215"));
    TEST_ASSERT_EQUAL_STRING("02605.41E", b);


    TEST_ASSERT_EQUAL_INT(33, aprs_pos(c, sizeof(c), "Cismigiu Park", "44.437461", "26.090215"));
    TEST_ASSERT_EQUAL_STRING("!4426.25N/02605.41E-Cismigiu Park", c);
    TEST_ASSERT_EQUAL_INT(31, aprs_pos(c, sizeof(c), "Null Island", "0.02", "-0.04"));
    TEST_ASSERT_EQUAL_STRING("!0001.20N/00002.40W-Null Island", c);
}

static void test_aprs101_pair(void)
{
    char a[64];
    char b[64];

    UNITY_SET_DETAILS("aprs", "101");
    TEST_ASSERT_EQUAL_INT(8, aprs_lat(a, sizeof(a), "49.058333333"));
    TEST_ASSERT_EQUAL_STRING("4903.50N", a);


    TEST_ASSERT_EQUAL_INT(9, aprs_lon(b, sizeof(b), "-72.029166667"));
    TEST_ASSERT_EQUAL_STRING("07201.75W", b);
}

static void test_clamps_and_invalid(void)
{
    char a[64];
    char b[64];

    /* clamp */
    UNITY_SET_DETAILS("aprs", "clamp");
    TEST_ASSERT_TRUE(aprs_ll_clamp(a, sizeof(a), "123.45", 0) > 0);
    TEST_ASSERT_EQUAL_STRING("90.00000", a);
    TEST_ASSERT_TRUE(aprs_ll_clamp(b, sizeof(b), "-222.75", 1) > 0);
    TEST_ASSERT_EQUAL_STRING("-180.00000", b);
    TEST_ASSERT_TRUE(aprs_ll_clamp(a, sizeof(a), "-91.00", 0) > 0);
    TEST_ASSERT_EQUAL_STRING("-90.00000", a);
    TEST_ASSERT_TRUE(aprs_ll_clamp(b, sizeof(b), "181.00", 1) > 0);
    TEST_ASSERT_EQUAL_STRING("180.00000", b);


    TEST_ASSERT_FALSE(aprs_lat(a, sizeof(a), "abc"));
    TEST_ASSERT_FALSE(aprs_lon(b, sizeof(b), "181.01"));
}

static void test_bulletin_and_status_strings(void)
{
    char c[160];

    /* bln txt */
    UNITY_SET_DETAILS("aprs", "payloads");
    TEST_ASSERT_EQUAL_INT((int)strlen(":BLN3     :pj6y bln"), aprs_bulletin(c, sizeof(c), 3, "pj6y bln"));
    TEST_ASSERT_EQUAL_STRING(":BLN3     :pj6y bln", c);

    TEST_ASSERT_EQUAL_INT((int)strlen(">yp0ta up"), aprs_status(c, sizeof(c), "yp0ta up"));
    TEST_ASSERT_EQUAL_STRING(">yp0ta up", c);
}

static void test_bulletin_packet_pj6y(void)
{
    char c[160];
    Packet p;
    uint8_t want[355];
    uint16_t i;
    static const uint8_t ax_pj6y[] = {
        0x82, 0xA0, 0xB4, 0x8C, 0x98, 0xA0, 0x60, 0xA0, 0x94, 0x6C, 0xB2, 0x40,
        0x40, 0x61, 0x03, 0xF0, 0x3A, 0x42, 0x4C, 0x4E, 0x33, 0x20, 0x20, 0x20,
        0x20, 0x20, 0x3A, 0x70, 0x6A, 0x36, 0x79, 0x20, 0x62, 0x6C, 0x6E,
    };
    static const char *st_pj6y =
        "01111110010000010000010100101101001100010001100100000101000001100000010100101001"
        "00110110010011010000001000000010100001101100000000001111010111000100001000110010"
        "01110010110011000000010000000100000001000000010000000100010111000000111001010110"
        "011011001001111000000100010001100011011001110110100101010110011101111110";

    /* bln pkt */
    UNITY_SET_DETAILS("aprs", "pj6y");
    TEST_ASSERT_EQUAL_INT((int)strlen(":BLN3     :pj6y bln"), aprs_bulletin(c, sizeof(c), 3, "pj6y bln"));
    TEST_ASSERT_TRUE(aprs_packet(&p, "PJ6Y", 0, "APZFLP", 0, c, NULL));
    TEST_ASSERT_EQUAL_UINT16(sizeof(ax_pj6y), p.ax25_len);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(ax_pj6y, p.ax25, sizeof(ax_pj6y));
    TEST_ASSERT_EQUAL_UINT16((uint16_t)strlen(st_pj6y), p.stuffed_len);


    for (i = 0; i < (uint16_t)strlen(st_pj6y); i++) want[i] = (uint8_t)(st_pj6y[i] == '1');
    TEST_ASSERT_EQUAL_UINT8_ARRAY(want, p.stuffed, p.stuffed_len);
}

static void test_status_packet_yp0ta(void)
{
    char c[160];
    Packet p;
    static const uint8_t ax_yp0ta[] = {
        0x82, 0xA0, 0xB4, 0x8C, 0x98, 0xA0, 0x60, 0xB2, 0xA0, 0x60, 0xA8, 0x82,
        0x40, 0x61, 0x03, 0xF0, 0x3E, 0x79, 0x70, 0x30, 0x74, 0x61, 0x20, 0x75,
        0x70,
    };

    /* status pkt */
    UNITY_SET_DETAILS("aprs", "yp0ta");
    TEST_ASSERT_EQUAL_INT((int)strlen(">yp0ta up"), aprs_status(c, sizeof(c), "yp0ta up"));
    TEST_ASSERT_TRUE(aprs_packet(&p, "YP0TA", 0, "APZFLP", 0, c, NULL));
    TEST_ASSERT_EQUAL_UINT16(sizeof(ax_yp0ta), p.ax25_len);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(ax_yp0ta, p.ax25, sizeof(ax_yp0ta));
    TEST_ASSERT_EQUAL_HEX8(0x03, p.ax25[14]);
    TEST_ASSERT_EQUAL_HEX8(0xF0, p.ax25[15]);
}

static void test_status_packet_yp0ta_bits(void)
{
    char c[160];
    Packet p;
    uint8_t want[256];
    static const char *st_yp0ta =
        "01111110010000010000010100101101001100010001100100000101000001100100110100000101"
        "00000110000101010100000100000010100001101100000000001111011111000100111100000111"
        "0000011000010111010000110000001001010111000001110010011010011100101111110";

    /* status bits */
    UNITY_SET_DETAILS("aprs", "yp0ta bits");
    TEST_ASSERT_EQUAL_INT((int)strlen(">yp0ta up"), aprs_status(c, sizeof(c), "yp0ta up"));
    TEST_ASSERT_TRUE(aprs_packet(&p, "YP0TA", 0, "APZFLP", 0, c, NULL));
    TEST_ASSERT_EQUAL_UINT16((uint16_t)strlen(st_yp0ta), p.stuffed_len);
    bits01(want, st_yp0ta);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(want, p.stuffed, p.stuffed_len);
}

static void test_one_message_packet(void)
{
    char msg[160];
    Packet p;
    static const uint8_t ax_yo3gnd[] = {
        0x82, 0xA0, 0xB4, 0x8C, 0x98, 0xA0, 0x60, 0xB2, 0x9E, 0x66, 0x8E, 0x9C,
        0x88, 0x63, 0x03, 0xF0, 0x3A, 0x59, 0x4F, 0x38, 0x59, 0x4C, 0x2D, 0x35,
        0x20, 0x20, 0x3A, 0x37, 0x33,
    };

    /* 73 pkt */
    UNITY_SET_DETAILS("aprs", "yo3gnd");
    TEST_ASSERT_EQUAL_INT((int)strlen(":YO8YL-5  :73"), aprs_message(msg, sizeof(msg), "YO8YL", 5, "73"));
    TEST_ASSERT_EQUAL_STRING(":YO8YL-5  :73", msg);
    TEST_ASSERT_TRUE(aprs_packet(&p, "YO3GND", 1, "APZFLP", 0, msg, NULL));
    TEST_ASSERT_EQUAL_UINT16(sizeof(ax_yo3gnd), p.ax25_len);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(ax_yo3gnd, p.ax25, sizeof(ax_yo3gnd));
    TEST_ASSERT_EQUAL_UINT16((uint16_t)strlen(":YO8YL-5  :73"), p.payload_len);
    TEST_ASSERT_EQUAL_HEX8(0x03, p.ax25[14]);
    TEST_ASSERT_EQUAL_HEX8(0xF0, p.ax25[15]);
}

static void test_one_message_packet_bits(void)
{
    char msg[160];
    Packet p;
    uint8_t want[320];
    static const char *st_yo3gnd =
        "01111110010000010000010100101101001100010001100100000101000001100100110101111001"
        "01100110011100010011100100010001110001101100000000001111010111001001101011110010"
        "00011100100110100011001010110100101011000000010000000100010111001110110011001100"
        "111011100111011001111110";

    /* 73 bits */
    UNITY_SET_DETAILS("aprs", "yo3gnd bits");
    TEST_ASSERT_EQUAL_INT((int)strlen(":YO8YL-5  :73"), aprs_message(msg, sizeof(msg), "YO8YL", 5, "73"));
    TEST_ASSERT_TRUE(aprs_packet(&p, "YO3GND", 1, "APZFLP", 0, msg, NULL));
    TEST_ASSERT_EQUAL_UINT16((uint16_t)strlen(st_yo3gnd), p.stuffed_len);
    bits01(want, st_yo3gnd);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(want, p.stuffed, p.stuffed_len);
}

static void test_jy1_message_packet(void)
{
    char msg[160];
    Packet p;
    uint8_t want[320];
    static const uint8_t ax_jy1[] = {
        0x82, 0xA0, 0xB4, 0x8C, 0x98, 0xA0, 0x60, 0x94, 0xB2, 0x62, 0x40, 0x40,
        0x40, 0x61, 0x03, 0xF0, 0x3A, 0x59, 0x4F, 0x39, 0x4C, 0x49, 0x47, 0x2D,
        0x32, 0x20, 0x3A, 0x6A, 0x79, 0x31, 0x20, 0x68, 0x69,
    };
    static const char *st_jy1 =
        "01111110010000010000010100101101001100010001100100000101000001100010100101001101"
        "01000110000000100000001000000010100001101100000000001111010111001001101011110010"
        "10011100001100101001001011100010101101000100110000000100010111000101011010011110"
        "10001100000001000001011010010110011100111001011001111110";

    /* jy1 */
    UNITY_SET_DETAILS("aprs", "jy1");
    TEST_ASSERT_EQUAL_INT((int)strlen(":YO9LIG-2 :jy1 hi"), aprs_message(msg, sizeof(msg), "YO9LIG", 2, "jy1 hi"));
    TEST_ASSERT_EQUAL_STRING(":YO9LIG-2 :jy1 hi", msg);
    TEST_ASSERT_TRUE(aprs_packet(&p, "JY1", 0, "APZFLP", 0, msg, NULL));
    TEST_ASSERT_EQUAL_UINT16(sizeof(ax_jy1), p.ax25_len);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(ax_jy1, p.ax25, sizeof(ax_jy1));
    TEST_ASSERT_EQUAL_UINT16((uint16_t)strlen(st_jy1), p.stuffed_len);
    bits01(want, st_jy1);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(want, p.stuffed, p.stuffed_len);
}

static void test_yo9lig_message_packet(void)
{
    char msg[160];
    Packet p;
    uint8_t want[320];
    static const uint8_t ax_yo9lig[] = {
        0x82, 0xA0, 0xB4, 0x8C, 0x98, 0xA0, 0x60, 0xB2, 0x9E, 0x72, 0x98, 0x92,
        0x8E, 0x61, 0x03, 0xF0, 0x3A, 0x59, 0x4F, 0x33, 0x47, 0x4E, 0x44, 0x2D,
        0x31, 0x20, 0x3A, 0x6C, 0x69, 0x67, 0x20, 0x68, 0x69,
    };
    static const char *st_yo9lig =
        "01111110010000010000010100101101001100010001100100000101000001100100110101111001"
        "01001110000110010100100101110001100001101100000000001111010111001001101011110010"
        "11001100111000100111001000100010101101001000110000000100010111000011011010010110"
        "11100110000001000001011010010110001000110100011001111110";

    /* lig */
    UNITY_SET_DETAILS("aprs", "yo9lig");
    TEST_ASSERT_EQUAL_INT((int)strlen(":YO3GND-1 :lig hi"), aprs_message(msg, sizeof(msg), "YO3GND", 1, "lig hi"));
    TEST_ASSERT_EQUAL_STRING(":YO3GND-1 :lig hi", msg);
    TEST_ASSERT_TRUE(aprs_packet(&p, "YO9LIG", 0, "APZFLP", 0, msg, NULL));
    TEST_ASSERT_EQUAL_UINT16(sizeof(ax_yo9lig), p.ax25_len);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(ax_yo9lig, p.ax25, sizeof(ax_yo9lig));
    TEST_ASSERT_EQUAL_UINT16((uint16_t)strlen(st_yo9lig), p.stuffed_len);
    bits01(want, st_yo9lig);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(want, p.stuffed, p.stuffed_len);
}

static void test_pos_packet_yo8yl(void)
{
    char msg[160];
    Packet p;
    uint8_t want[591];
    uint16_t i;
    static const uint8_t ax_yo8yl_pos[] = {
        0x82, 0xA0, 0xB4, 0x8C, 0x98, 0xA0, 0x60, 0xB2, 0x9E, 0x70, 0xB2, 0x98,
        0x40, 0x79, 0x03, 0xF0, 0x21, 0x34, 0x34, 0x32, 0x36, 0x2E, 0x32, 0x35,
        0x4E, 0x2F, 0x30, 0x32, 0x36, 0x30, 0x35, 0x2E, 0x34, 0x31, 0x45, 0x2D,
        0x43, 0x69, 0x73, 0x6D, 0x69, 0x67, 0x69, 0x75, 0x20, 0x50, 0x61, 0x72,
        0x6B,
    };
    static const char *st_yo8yl_pos =
        "01111110010000010000010100101101001100010001100100000101000001100100110101111001"
        "00001110010011010001100100000010100111101100000000001111100000100001011000010110"
        "00100110001101100011101000100110010101100011100101111010000001100010011000110110"
        "00000110010101100011101000010110010001100101000101011010011000010100101101100111"
        "01011011010010110111001101001011010101110000001000000101010000110010011101101011"
        "0010101010010010001111110";

    /* pos pkt */
    UNITY_SET_DETAILS("aprs", "yo8yl");
    TEST_ASSERT_EQUAL_INT((int)strlen("!4426.25N/02605.41E-Cismigiu Park"),
        aprs_pos(msg, sizeof(msg), "Cismigiu Park", "44.437461", "26.090215"));
    TEST_ASSERT_TRUE(aprs_packet(&p, "YO8YL", 12, "APZFLP", 0, msg, NULL));
    TEST_ASSERT_EQUAL_UINT16(sizeof(ax_yo8yl_pos), p.ax25_len);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(ax_yo8yl_pos, p.ax25, sizeof(ax_yo8yl_pos));
    TEST_ASSERT_EQUAL_UINT16((uint16_t)strlen(st_yo8yl_pos), p.stuffed_len);
    for (i = 0; i < (uint16_t)strlen(st_yo8yl_pos); i++) want[i] = (uint8_t)(st_yo8yl_pos[i] == '1');
    TEST_ASSERT_EQUAL_UINT8_ARRAY(want, p.stuffed, p.stuffed_len);
}

static void test_loopish_calls(void)
{
    const char *win[] = {
        "YO3GND", "YO8YL", "YO9LIG", "YO3KRM", "PJ6Y", "YP0TA",

        "DL0LOL", "GB0OTY", "GB4NGS", "HA6RID", "SO2NIC", "3Z3Z3Z", "YO0F",
        "YP3PE", "VB2CAP", "K0K", "K4B", "K3T", "DU0W0",

        "OO0F", "OR4L", "GB0NGO", "G4SSY", "G1SUS", "TM5OON", "VA6INA",
        "VC7F", "K3K", "K4T", "N9E", "W4L", "K1D", "K1L", "W3B",

        "GB4LLS", "GB0NPC", "TM0GUS", "TM4RIO", "VB4LLS", "W4P", "N1O", "W0W",

        "TM0WO",
    };
    const char *lose[] = {
        "DC0NORRIS", "OE01MIKU", "SQ6FMBY", "HF0FURRY", "SP0FENNEC", "HF0AAAAAA",
        "SP6ZOLW", "VC2NORRIS", "VB2CATGIRL", "DU0LINGO",

        "DF4CEPALM", "PD6SENPAI", "OE51DEEYE", "7S0BRUH", "YR1CKROLL", "EG1TBDICS",
        "VB4TESTING", "VB2THEGAME", "VB2KILOWAT", "VC7ZALGO", "VC7HORSE",

        "DL0NGCAT", "PD33ZDOGE", "TM0NKAS", "VB1FLIP", "VB3BACON", "VB6WOKE",
        "VC7BEPIS", "VC9FEMBOY", "VC2STONKS", "VC3PWEOR", "DZ2NUTS",

        "TM1SSOU", "TM0RBIN", "VB4LIGMA", "VB3YEET", "VC9CATGIRL",
        "VC3DEEZ", "VB3HARAMBE", "VC3RIKROLL", "VB6DANK",
    };
    Packet p;
    char msg[160];
    char payload[160];
    int i;
    int win_n;
    int lose_n;

    /* win */
    UNITY_SET_DETAILS("aprs", "loop");
    win_n = (int)(sizeof(win) / sizeof(win[0]));
    lose_n = (int)(sizeof(lose) / sizeof(lose[0]));

    for (i = 0; i < win_n; i++)
    {
        uint8_t sa = i & 15;
        const char *src = win[i];
        const char *dst = win[(i + 7) % win_n];
        const char *txt = win[(i + 11) % win_n];
        int n;

        n = aprs_message(msg, sizeof(msg), dst, (i + 3) & 7, txt);
        TEST_ASSERT_TRUE(n > 0);
        TEST_ASSERT_TRUE(aprs_packet(&p, src, sa, "APZFLP", 0, msg, NULL));
        TEST_ASSERT_EQUAL_UINT16((uint16_t)n, p.payload_len);
        TEST_ASSERT_EQUAL_UINT8_ARRAY((uint8_t*)msg, p.payload, p.payload_len);
        TEST_ASSERT_EQUAL_HEX8(0x03, p.ax25[14]);
        TEST_ASSERT_EQUAL_HEX8(0xF0, p.ax25[15]);
        TEST_ASSERT_TRUE(p.fcs_len == p.ax25_len + 2);
        TEST_ASSERT_TRUE(p.stuffed_len > p.fcs_len);
        TEST_ASSERT_TRUE(p.nrzi_len == p.stuffed_len);
        TEST_ASSERT_EQUAL_UINT8(0, p.stuffed[0]);
        TEST_ASSERT_EQUAL_UINT8(1, p.stuffed[1]);
        TEST_ASSERT_EQUAL_UINT8(1, p.stuffed[2]);
        TEST_ASSERT_EQUAL_UINT8(0, p.stuffed[p.stuffed_len - 1]);

        TEST_ASSERT_EQUAL_HEX8(0x82, p.ax25[0]);
        TEST_ASSERT_EQUAL_HEX8(0xA0, p.ax25[1]);
    }


    /* lose */
    for (i = 0; i < lose_n; i++)
    {
        snprintf(payload, sizeof(payload), ">%s", lose[i]);
        TEST_ASSERT_EQUAL_INT(0, aprs_message(msg, sizeof(msg), lose[i], (i + 3) & 7, "NOPE"));
        TEST_ASSERT_FALSE(aprs_packet(&p, lose[i], i & 15, "APZFLP", 0, payload, NULL));
        TEST_ASSERT_FALSE(aprs_packet(&p, "YO3GND", i & 15, lose[i], 0, payload, NULL));
    }
}

static void test_loopish_calls_old_pool(void)
{
    const char *calls[30] = {
        "YO3GND", "YO8YL", "PJ6Y", "YP0TA", "JY1", "YO9LIG", "FL1PER", "YO3KRM",
        "YQ4M", "FL1PPR", "N0CALL", "W1AW", "APRS", "DB1XYZ", "VK2ABC", "JA1ABC",
        "F4XYZ", "M0ABC", "KJ7ABC", "YU1AAV", "HB9ABC", "OE1XUU", "LZ1KRN", "SV1ABC",
        "ZL1XYZ", "EA7XYZ", "SP3ABC", "OK1ABC", "SM0ABC", "OH2ABC",
    };
    Packet p;
    char msg[160];
    uint8_t dst_ax[7];
    uint8_t src_ax[7];
    int i;

    /* old pool */
    UNITY_SET_DETAILS("aprs", "loop old");
    ax7(dst_ax, "APZFLP", 0, 0);

    for (i = 0; i < 30; i++)
    {
        uint8_t sa;
        uint8_t da;
        const char *src;
        const char *dst;
        const char *txt;
        int n;

        src = calls[i];
        dst = calls[(i + 7) % 30];
        txt = calls[(i + 11) % 30];
        sa = i & 15;
        da = (i + 3) & 15;

        n = aprs_message(msg, sizeof(msg), dst, da, txt);
        TEST_ASSERT_TRUE(n > 0);
        TEST_ASSERT_TRUE(aprs_packet(&p, src, sa, "APZFLP", 0, msg, NULL));
        TEST_ASSERT_EQUAL_UINT16((uint16_t)n, p.payload_len);
        TEST_ASSERT_EQUAL_UINT8_ARRAY((uint8_t*)msg, p.payload, p.payload_len);

        ax7(src_ax, src, sa, 1);
        TEST_ASSERT_EQUAL_UINT8_ARRAY(dst_ax, p.ax25, 7);
        TEST_ASSERT_EQUAL_UINT8_ARRAY(src_ax, p.ax25 + 7, 7);
        TEST_ASSERT_EQUAL_HEX8(0x03, p.ax25[14]);
        TEST_ASSERT_EQUAL_HEX8(0xF0, p.ax25[15]);
        TEST_ASSERT_EQUAL_UINT16(p.ax25_len + 2, p.fcs_len);
        TEST_ASSERT_TRUE(p.stuffed_len > p.fcs_len);
        TEST_ASSERT_EQUAL_UINT16(p.stuffed_len, p.nrzi_len);
        TEST_ASSERT_EQUAL_UINT8(0, p.stuffed[0]);
        TEST_ASSERT_EQUAL_UINT8(1, p.stuffed[1]);
        TEST_ASSERT_EQUAL_UINT8(1, p.stuffed[2]);
        TEST_ASSERT_EQUAL_UINT8(0, p.stuffed[p.stuffed_len - 1]);
    }
}

int main(void)
{
    suiteSetUp();
    UnityBegin(__FILE__);
    RUN_TEST(test_lat_lon_and_pos);
    RUN_TEST(test_aprs101_pair);
    RUN_TEST(test_clamps_and_invalid);
    RUN_TEST(test_bulletin_and_status_strings);
    RUN_TEST(test_bulletin_packet_pj6y);
    RUN_TEST(test_status_packet_yp0ta);
    RUN_TEST(test_status_packet_yp0ta_bits);
    RUN_TEST(test_one_message_packet);
    RUN_TEST(test_one_message_packet_bits);
    RUN_TEST(test_jy1_message_packet);
    RUN_TEST(test_yo9lig_message_packet);
    RUN_TEST(test_pos_packet_yo8yl);
    RUN_TEST(test_loopish_calls);
    RUN_TEST(test_loopish_calls_old_pool);
    return suiteTearDown(UnityEnd());
}
