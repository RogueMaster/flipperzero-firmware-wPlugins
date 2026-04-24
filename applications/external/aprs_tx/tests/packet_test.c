#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    uint8_t payload[96];
    uint16_t payload_len;

    uint8_t ax25[192];
    uint16_t ax25_len;

    uint8_t fcs[194];
    uint16_t fcs_len;

    uint8_t stuffed[1800];
    uint16_t stuffed_len;

    uint8_t nrzi[1800];


    uint16_t nrzi_len;
} Packet;

void packet_init(Packet* p);
void packet_make_payload(Packet* p, const char* s);
void packet_make_ax25(Packet* p, const char* from, uint8_t from_ssid, const char* to, uint8_t to_ssid, const char* path);
void packet_add_fcs(Packet* p);
void packet_stuff(Packet* p);


void packet_nrzi(Packet* p);



/* ---------------------- */
void packet_do_all(Packet* p, const char* from, uint8_t from_ssid, const char* to, uint8_t to_ssid, const char* s, const char* path);

typedef struct 
{
    const char* name;
    void (*fn)(void);
} PacketTest;

static int testok = 0;
static int testbad = 0;

static void packet_test_u16(const char* name, uint16_t got, uint16_t want)
{
    if(got == want) 
    {
        testok++;
        printf("ok %s\n", name);
    }
    else
    {
        testbad++;
        printf("bad %s got=%u want=%u\n", name, got, want);
    }
}

static void packet_test_bytes(const char* name, const uint8_t* data, uint16_t len, const uint8_t* want, uint16_t wantlen)
{
    uint16_t i;

    if(len != wantlen) 
    {
        testbad++;
        printf("bad %s len=%u want=%u\n", name, len, wantlen);
        return;
    }

    for(i = 0; i < len; i++) 
    {
        if(data[i] != want[i]) 
        {
            testbad++;
            printf("bad %s la=%u got=%02X want=%02X\n", name, i, data[i], want[i]);
            return;
        }
    }

    testok++;
    printf("ok %s\n", name);
}



static void packet_test_bits(const char* name, const uint8_t* data, uint16_t len, const char* want)
{
    uint16_t i;
    uint16_t wantlen = (uint16_t)strlen(want);

    if(len != wantlen) {
        testbad++;
        printf("bad %s len=%u want=%u\n", name, len, wantlen);
        return;
    }

    for(i = 0; i < len; i++) {
        if(data[i] != (uint8_t)(want[i] == '1')) {
            testbad++;
            printf("bad %s la=%u got=%u want=%c\n", name, i, data[i], want[i]);
            return;
        }
    }

    testok++;
    printf("ok %s\n", name);
}

static void packet_test_payload_mockup(void)
{
    static const uint8_t want[] = { 0x3E, 0x54, 0x45, 0x53, 0x54, 0x31, 0x32, 0x33, };
    Packet p;

    packet_init(&p);
    packet_make_payload(&p, "TEST123");

    packet_test_u16("payload lungime", p.payload_len, sizeof(want));
    packet_test_bytes("payload bytes", p.payload, p.payload_len, want, sizeof(want));
}

static void testgol(void)
{
    static const uint8_t a[] = ">";
    Packet p;

    packet_init(&p);
    packet_make_payload(&p, "");

    packet_test_u16("payload gol len", p.payload_len, sizeof(a) - 1);
    packet_test_bytes("payload gol bytes", p.payload, p.payload_len, a, sizeof(a) - 1);
}

static void packet_test_payload_truncate_mockup(void)
{
        static const uint8_t a[] = ">ABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQ";
        Packet p;

        packet_init(&p);
        packet_make_payload(&p, "ABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZ");

        packet_test_u16("payload trunc len", p.payload_len, sizeof(a) - 1);
        packet_test_bytes("payload trunc bytes", p.payload, p.payload_len, a, sizeof(a) - 1);
}

static void packet_test_ax25_mockup(void)
{
    static const uint8_t want[] = {
        0xAE, 0x60, 0xA4, 0x98, 0x88, 0x40, 0x60, 0xB2,
        0x9E, 0x60, 0x8C, 0x98, 0xA0, 0x61, 0x03, 0xF0,
        0x3E, 0x54, 0x45, 0x53, 0x54, 0x31, 0x32, 0x33,
    };
    Packet p;

    packet_init(&p);
    packet_make_payload(&p, "TEST123");
    packet_make_ax25(&p, "YO0FLP", 0, "W0RLD", 0, NULL);

    packet_test_u16("ax25 len", p.ax25_len, sizeof(want));
    packet_test_bytes("ax25 bytes", p.ax25, p.ax25_len, want, sizeof(want));
}

static void packet_test_ax25_ssid_mockup(void)
{
    static const uint8_t a[] = {
        0x82, 0xA0, 0x40, 0x40, 0x40, 0x40, 0x7E, 0x9C,
        0x60, 0x86, 0x82, 0x98, 0x98, 0x67, 0x03, 0xF0,
        0x3E,
    };
    Packet p;

    packet_init(&p);
    packet_make_payload(&p, "");
    packet_make_ax25(&p, "N0CALL", 3, "AP", 15, NULL);

    packet_test_u16("ax25 ssid len", p.ax25_len, sizeof(a));
    packet_test_bytes("ax25 ssid bytes", p.ax25, p.ax25_len, a, sizeof(a));
}

static void packet_test_fcs_mockup(void)
{
    static const uint8_t want[] = {
        0xAE, 0x60, 0xA4, 0x98, 0x88, 0x40, 0x60, 0xB2,
        0x9E, 0x60, 0x8C, 0x98, 0xA0, 0x61, 0x03, 0xF0,
        0x3E, 0x54, 0x45, 0x53, 0x54, 0x31, 0x32, 0x33,
        0x57, 0x51,
    };
    Packet p;

    packet_init(&p);
    packet_make_payload(&p, "TEST123");
    packet_make_ax25(&p, "YO0FLP", 0, "W0RLD", 0, NULL);
    packet_add_fcs(&p);

    packet_test_u16("fcs lungime", p.fcs_len, sizeof(want));
    packet_test_bytes("fcs bytes", p.fcs, p.fcs_len, want, sizeof(want));
}

static void packet_test_stuff_mockup(void)
{
    static const char* want =
        "011111100111010100000110001001010001100100010001000000100000011001001101"
        "011110010000011000110001000110010000010110000110110000000000111101111100"
        "000101010101000101100101000101010100011000100110011001100111010101000101"
        "001111110";
    Packet p;

    packet_init(&p);
    packet_make_payload(&p, "TEST123");
    packet_make_ax25(&p, "YO0FLP", 0, "W0RLD", 0, NULL);
    packet_add_fcs(&p);
    packet_stuff(&p);

    packet_test_u16("stuffed bits", p.stuffed_len, 225);
    packet_test_bits("stuffed data", p.stuffed, p.stuffed_len, want);
}

static void packet_test_stuff_ff_mockup(void)
{
    static const char* a = "0111111011111011101111110";
    Packet p;

    packet_init(&p);
    p.fcs[0] = 0xFF;
    p.fcs_len = 1;
    packet_stuff(&p);

    packet_test_u16("stuffed ff bits", p.stuffed_len, 25);
    packet_test_bits("stuffed ff data", p.stuffed, p.stuffed_len, a);
}

static void packet_test_nrzi_mockup(void)
{
    static const char* want =
        "000000010000110010101110100100110100010010110100101010010101000100100011"
        "000001001010111010001011010001001010110001010001110101010101111100000010"
        "101100110011010011101100101100110010111010010001000100010000110011010011"
        "011111110";
    Packet p;

    packet_init(&p);
    packet_make_payload(&p, "TEST123");
    packet_make_ax25(&p, "YO0FLP", 0, "W0RLD", 0, NULL);
    packet_add_fcs(&p);
    packet_stuff(&p);
    packet_nrzi(&p);

    packet_test_u16("nrzi biti", p.nrzi_len, 225);
    packet_test_bits("nrzi data", p.nrzi, p.nrzi_len, want);
}

static void packet_test_nrzi_ff_mockup(void)
{
    static const char* a = "0000000111111000011111110";
    Packet p;

    packet_init(&p);
    p.fcs[0] = 0xFF;
    p.fcs_len = 1;
    packet_stuff(&p);
    packet_nrzi(&p);

    packet_test_u16("nrzi ff biti", p.nrzi_len, 25);
    packet_test_bits("nrzi ff data", p.nrzi, p.nrzi_len, a);
}

/*--- cap coada ----*/

static void testmare(void)
{
    static const uint8_t a[] = ">Hello world, I am Flipper Zero :D";
    static const uint8_t b[] = { 0xAE, 0x60, 0xA4, 0x98, 0x88, 0x40, 0x60, 0xB2, 0x9E, 0x60, 0x8C, 0x98, 0xA0, 0x61, 0x03, 0xF0, 0x3E, 0x48, 0x65, 0x6C, 0x6C, 0x6F, 0x20, 0x77, 0x6F, 0x72, 0x6C, 0x64, 0x2C, 0x20, 0x49, 0x20, 0x61, 0x6D, 0x20, 0x46, 0x6C, 0x69, 0x70, 0x70, 0x65, 0x72, 0x20, 0x5A, 0x65, 0x72, 0x6F, 0x20, 0x3A, 0x44, };


    static const uint8_t c[] = { 0xAE, 0x60, 0xA4, 0x98, 0x88, 0x40, 0x60, 0xB2, 0x9E, 0x60, 0x8C, 0x98, 0xA0, 0x61, 0x03, 0xF0, 0x3E, 0x48, 0x65, 0x6C, 0x6C, 0x6F, 0x20, 0x77, 0x6F, 0x72, 0x6C, 0x64, 0x2C, 0x20, 0x49, 0x20, 0x61, 0x6D, 0x20, 0x46, 0x6C, 0x69, 0x70, 0x70, 0x65, 0x72, 0x20, 0x5A, 0x65, 0x72, 0x6F, 0x20, 0x3A, 0x44, 0x92, 0x7D, };



    static const char* d =
        "011111100111010100000110001001010001100100010001000000100000011001001101"
        "011110010000011000110001000110010000010110000110110000000000111101111100"
        "000010010101001100011011000110110111101100000010011101110111101100100111"
        "000110110001001100011010000000100100100100000010010000110101101100000010"
        "001100010001101101001011000001110000011101010011001001110000001000101101"
        "010100110010011101111011000000100010111000010001001001001101111100011111"
        "10";
    static const char* e =
        "000000010000110010101110100100110100010010110100101010010101000100100011"
        "000001001010111010001011010001001010110001010001110101010101111100000010"
        "101001001100100010111000101110001111100010101001000011110000011101101111"
        "010001110100100010111001010101101101101101010110110101110011100010101001"
        "011101001011100011011000101011110101000011001000100100001010100101100011"
        "001101110110111100000111010101101001111010110100100100100011111101000000"
        "01";
    Packet p;


    packet_do_all(&p, "YO0FLP", 0, "W0RLD", 0, "Hello world, I am Flipper Zero :D", NULL);

    packet_test_u16("all payload len", p.payload_len, sizeof(a) - 1);
    packet_test_bytes("all payload bytes", p.payload, p.payload_len, a, sizeof(a) - 1);

    packet_test_u16("all ax25 len", p.ax25_len, sizeof(b));
    packet_test_bytes("all ax25 bytes", p.ax25, p.ax25_len, b, sizeof(b));

    
    packet_test_u16("all fcs len", p.fcs_len, sizeof(c));
    packet_test_bytes("all fcs bytes", p.fcs, p.fcs_len, c, sizeof(c));




    packet_test_u16("all stuffed len", p.stuffed_len, 434);
    packet_test_bits("all stuffed data", p.stuffed, p.stuffed_len, d);

    packet_test_u16("all nrzi len", p.nrzi_len, 434);
    packet_test_bits("all nrzi data", p.nrzi, p.nrzi_len, e);
}

static const PacketTest packet_tests[] = {
    { "payload", packet_test_payload_mockup },
    { "payload gol", testgol },
    { "payload trunc", packet_test_payload_truncate_mockup },
    { "ax25", packet_test_ax25_mockup },
    { "ax25 mic", packet_test_ax25_ssid_mockup },
    { "fcs", packet_test_fcs_mockup },
    { "stuff", packet_test_stuff_mockup },
    { "stuff 0xff", packet_test_stuff_ff_mockup },
    { "nrzi", packet_test_nrzi_mockup },
    { "nrzi 0xff", packet_test_nrzi_ff_mockup },
    { "mare", testmare },
};

int main(void)
{
    uint16_t i;

    for(i = 0; i < sizeof(packet_tests) / sizeof(packet_tests[0]); i++) 
    {
        printf("== %s ==\n", packet_tests[i].name);
        packet_tests[i].fn();
    }

    printf("ok=%d bad=%d\n", testok, testbad);
    if(testbad) return 1;


    return 0;
}
