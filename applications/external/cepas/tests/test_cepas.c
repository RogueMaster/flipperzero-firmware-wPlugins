#include "../cepas.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
static int fails = 0;
#define CHECK(c)                                        \
    do {                                                \
        if(!(c)) {                                      \
            printf("FAIL line %d: %s\n", __LINE__, #c); \
            fails++;                                    \
        }                                               \
    } while(0)
int main(void) {
    uint8_t purse[64] = {0};
    purse[0] = 2;
    purse[1] = 1;
    purse[2] = 0x00;
    purse[3] = 0x04;
    purse[4] = 0xD2; /* 1234 cents */
    purse[5] = 0x00;
    purse[6] = 0x07;
    purse[7] = 0xD0; /* autoload 2000 */
    uint8_t can[8] = {0x10, 0x09, 0x12, 0x34, 0x56, 0x78, 0x90, 0x12};
    memcpy(&purse[8], can, 8);
    purse[24] = 0x34;
    purse[25] = 0x8E; /* expiry days */
    purse[40] = 5;
    /* last txn: MRT, -1.23 */
    purse[46] = 48;
    purse[47] = 0xFF;
    purse[48] = 0xFF;
    purse[49] = 0x85;
    uint32_t t = 974000000u;
    purse[50] = t >> 24;
    purse[51] = t >> 16;
    purse[52] = t >> 8;
    purse[53] = t;
    memcpy(&purse[54], "BGB-TPY\0", 8);

    CepasPurse p;
    CHECK(cepas_parse_purse(purse, sizeof(purse), &p));
    CHECK(p.balance == 1234);
    CHECK(p.autoload == 2000);
    CHECK(p.log_count == 5);
    CHECK(cepas_issuer(&p) == CepasIssuerEzLink);
    CHECK(p.has_last_txn && p.last_txn.type == 48 && p.last_txn.amount == -123);
    CHECK(strcmp(p.last_txn.user_data, "BGB-TPY") == 0);
    CHECK(!cepas_parse_purse(purse, 20, &p));

    char buf[32];
    cepas_format_amount(-123, true, buf, sizeof buf);
    CHECK(strcmp(buf, "-$1.23") == 0);
    cepas_format_amount(1000, true, buf, sizeof buf);
    CHECK(strcmp(buf, "+$10.00") == 0);
    cepas_format_amount(5, false, buf, sizeof buf);
    CHECK(strcmp(buf, "$0.05") == 0);
    cepas_format_can(can, buf, sizeof buf);
    CHECK(strcmp(buf, "1009 1234 5678 9012") == 0);

    uint8_t nets[8] = {0x11, 0x18, 0, 0, 0, 0, 0, 0};
    memcpy(p.can, nets, 8);
    CHECK(cepas_issuer(&p) == CepasIssuerNets);

    /* Epoch: 0 s -> 1995-01-01 00:00 wall clock */
    time_t w = (time_t)cepas_time_to_wallclock(0);
    struct tm* g = gmtime(&w);
    CHECK(g->tm_year == 95 && g->tm_mon == 0 && g->tm_mday == 1 && g->tm_hour == 0);
    w = (time_t)cepas_days_to_wallclock(365);
    g = gmtime(&w);
    CHECK(g->tm_year == 96 && g->tm_mon == 0 && g->tm_mday == 1);

    uint8_t hist[40] = {0};
    hist[0] = 117;
    hist[3] = 0x10;
    hist[16] = 49;
    CepasTxn tx[4];
    size_t n = cepas_parse_history(hist, sizeof hist, tx, 4);
    CHECK(n == 2 && tx[0].type == 117 && tx[0].amount == 16 && tx[1].type == 49);
    CHECK(strcmp(cepas_txn_type_name(0xF0), "Card created") == 0);
    CHECK(cepas_txn_type_name(0x99) == NULL);

    /* EZ-Link purse response based on a real card; card number, serial, balance and last trip replaced with fake values. */
    {
        static const char* hex = "02010004D20007D080090000000000010000000000000001247E1E10001211FF"
                                 "750009C42F45D7811E20009B010131FFFFC12F6E997B42555339363020000200"
                                 "80202900000101000000000000000000000000000000000000000000000001";
        uint8_t raw[128];
        size_t rl = 0;
        for(const char* h = hex; *h;) {
            if(*h == ' ') {
                h++;
                continue;
            }
            unsigned v;
            sscanf(h, "%2x", &v);
            raw[rl++] = (uint8_t)v;
            h += 2;
        }
        CHECK(rl == 95);
        CepasPurse r;
        CHECK(cepas_parse_purse(raw, rl, &r));
        CHECK(r.balance == 1234);
        CHECK(r.autoload == 2000);
        CHECK(r.log_count == 30);
        CHECK(cepas_issuer(&r) == CepasIssuerEzLink);
        CHECK(r.last_txn.type == 49 && r.last_txn.amount == -63);
        CHECK(strcmp(r.last_txn.user_data, "BUS960") == 0);
        time_t e = (time_t)cepas_days_to_wallclock(r.expiry_days);
        struct tm* g2 = gmtime(&e);
        CHECK(g2->tm_year == 120 && g2->tm_mon == 6 && g2->tm_mday == 30);
        char cb[24];
        cepas_format_can(r.can, cb, sizeof cb);
        CHECK(strcmp(cb, "8009 0000 0000 0001") == 0);
    }

    /* NETS FlashPay purse response based on a real card; card number and serial replaced with fake values. */
    {
        static const char* hex = "02000000000000001111000000000002000000000000000223F219F6001311FF"
                                 "750007D02E11836A1E200000D98F110000002F66F4D2544C2020202000000200"
                                 "000A0500000000223000000000000000000000000000000000000000000083";
        uint8_t raw[128];
        size_t rl = 0;
        char clean[400];
        size_t cl = 0;
        for(const char* h = hex; *h; h++)
            if(*h != ' ') clean[cl++] = *h;
        clean[cl] = 0;
        for(size_t i = 0; i + 1 < cl; i += 2) {
            unsigned v;
            sscanf(&clean[i], "%2x", &v);
            raw[rl++] = (uint8_t)v;
        }
        CHECK(rl == 95);
        CepasPurse r;
        CHECK(cepas_parse_purse(raw, rl, &r));
        CHECK(r.balance == 0);
        CHECK(r.log_count == 30);
        CHECK(cepas_issuer(&r) == CepasIssuerNets);
        char cb[24];
        cepas_format_can(r.can, cb, sizeof cb);
        CHECK(strcmp(cb, "1111 0000 0000 0002") == 0);
        CHECK(r.last_txn.type == 0x11 && strcmp(r.last_txn.user_data, "TL") == 0);
    }

    /* Descriptions, using records seen on the real cards. */
    {
        struct {
            uint8_t type;
            const char* ud;
            const char* kind;
            const char* detail;
        } cases[] = {
            {48, "JUR-ORC", "MRT", "Jurong East > Orchard"},
            {48, "BP2-BP3", "LRT", "South View > Keat Hong"},
            {48, "TAM-RFP", "MRT", "Tampines > Raffles Place"},
            {48, "XYZ-ABC", "MRT", "XYZ > ABC"},
            {49, "BUS960", "Bus", "Service 960"},
            {49, "BUS 190", "Bus", "Service 190"},
            {49, "BUS  31", "Bus", "Service 31"},
            {118, "BUS12", "Bus refund", "Service 12"},
            {117, "ORC GTM", "Top-up", "Orchard (machine)"},
            {0x11, "TL", "Ticket office", "TransitLink office"},
            {0x10, "TL", "Ticket office", "TransitLink office"},
            {0x09, "", "Other", ""},
        };
        for(size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
            CepasTxn t = {0};
            t.type = cases[i].type;
            snprintf(t.user_data, sizeof t.user_data, "%s", cases[i].ud);
            CepasTxnText tx;
            cepas_txn_describe(&t, &tx);
            if(strcmp(tx.kind, cases[i].kind) || strcmp(tx.detail, cases[i].detail)) {
                printf("FAIL describe %s: got '%s' / '%s'\n", cases[i].ud, tx.kind, tx.detail);
                fails++;
            }
        }
        CepasTxn t = {.type = 48};
        snprintf(t.user_data, sizeof t.user_data, "JUR-ORC");
        CepasTxnText tx;
        cepas_txn_describe(&t, &tx);
        CHECK(strcmp(tx.from, "Jurong East") == 0 && strcmp(tx.to, "Orchard") == 0);
    }

    printf(fails ? "%d FAILED\n" : "all tests passed\n", fails);
    return fails != 0;
}
