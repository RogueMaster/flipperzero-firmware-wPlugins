#include "markets.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_crypto_prices(void) {
    const char* bitcoin =
        "{\"symbol\":\"BTCUSDT\",\"lastPrice\":\"76471.76000000\","
        "\"closeTime\":1789489668007}";
    const char* ethereum =
        "{\"symbol\":\"ETHUSDT\",\"lastPrice\":\"2280.12000000\","
        "\"closeTime\":1789489668007}";
    char output[192];

    assert(markets_format(0, bitcoin, output, sizeof(output)));
    assert(strstr(output, "76471.76 USDT") != NULL);
    assert(strstr(output, "Source: Binance") != NULL);

    assert(markets_format(1, ethereum, output, sizeof(output)));
    assert(strstr(output, "2280.12 USDT") != NULL);
}

static void test_gold_price(void) {
    const char* gold =
        "{\"currency\":\"USD\",\"name\":\"Gold\",\"price\":4297.700195,"
        "\"symbol\":\"XAU\",\"updatedAt\":\"2026-09-15T16:27:40Z\"}";
    char output[192];

    assert(markets_format(2, gold, output, sizeof(output)));
    assert(strstr(output, "4297.700195 USD / troy oz") != NULL);
    assert(strstr(output, "2026-09-15T16:27:40Z") != NULL);
}

static void test_oil_prices(void) {
    const char* wti =
        "{\"lastUpdated\":\"2026-09-16T11:23:05.664Z\","
        "\"priceUsd\":103.36,\"dataSource\":\"Yahoo Finance (CL=F)\"}";
    const char* brent =
        "{\"lastUpdated\":\"2026-09-16T11:23:22.494Z\","
        "\"priceUsd\":107.14,\"dataSource\":\"Yahoo Finance (BZ=F)\"}";
    char output[192];
    assert(markets_format(3, wti, output, sizeof(output)));
    assert(strstr(output, "103.36 USD / barrel") != NULL);
    assert(strstr(output, "2026-09-16T11:23:05 UTC") != NULL);
    assert(strstr(output, "Source: AmericasOilWatch") != NULL);
    assert(markets_format(4, brent, output, sizeof(output)));
    assert(strstr(output, "107.14 USD / barrel") != NULL);
    assert(!markets_format(3, "{\"priceUsd\":103.36,\"lastUpdated\":\"bad\"}",
                           output, sizeof(output)));
    assert(!markets_format(4, "{\"priceUsd\":-1,\"lastUpdated\":\"2026-09-16T11:23:22.494Z\"}",
                           output, sizeof(output)));
}

static void test_rejects_untrusted_or_incomplete_data(void) {
    char output[64];
    assert(!markets_format(0, "{\"symbol\":\"ETHUSDT\",\"lastPrice\":\"1\","
                               "\"closeTime\":1}",
                           output, sizeof(output)));
    assert(!markets_format(2, "{\"symbol\":\"XAU\",\"currency\":\"EUR\","
                               "\"price\":1,\"updatedAt\":\"now\"}",
                           output, sizeof(output)));
    assert(!markets_format(0, "{\"symbol\":\"BTCUSDT\",\"lastPrice\":\"NaN\","
                               "\"closeTime\":1}",
                           output, sizeof(output)));
    assert(!markets_format(0, "{\"symbol\":\"BTCUSDT\",\"lastPrice\":\"1\"}",
                           output, sizeof(output)));
    assert(!markets_format(0, "{\"symbol\":\"BTCUSDT\",\"lastPrice\":\"1\","
                               "\"closeTime\":1}",
                           output, 8));
    assert(!markets_format(0, "{\"symbol\":\"BTCUSDT\",\"lastPrice\":\"1\","
                               "\"closeTime\":123}",
                           output, sizeof(output)));
    assert(!markets_format(2, "{\"symbol\":\"XAU\",\"currency\":\"USD\","
                               "\"price\":1,\"updatedAt\":\"not-a-time\"}",
                           output, sizeof(output)));
}

static void test_complete_object_required(void) {
    const char* valid = "{\"symbol\":\"BTCUSDT\",\"lastPrice\":\"12.50\","
                        "\"closeTime\":1789489668007}";
    char output[192];
    char partial[160];
    for(size_t length = 0; length < strlen(valid); ++length) {
        memcpy(partial, valid, length);
        partial[length] = '\0';
        assert(!markets_format(0, partial, output, sizeof(output)));
    }
    const char* invalid[] = {
        "{\"symbol\":\"BTCUSDT\",\"symbol\":\"ETHUSDT\",\"lastPrice\":\"1\",\"closeTime\":1789489668007}",
        "{\"symbol\":\"BTCUSDT\",\"lastPrice\":\"1.\",\"closeTime\":1789489668007}",
        "{\"symbol\":\"BTCUSDT\",\"lastPrice\":\"1\",\"closeTime\":1789489668007,}",
        "{\"symbol\":\"BTCUSDT\",\"lastPrice\":\"1\",\"closeTime\":1789489668007}garbage",
        "{\"nested\":{\"symbol\":\"BTCUSDT\",\"lastPrice\":\"1\",\"closeTime\":1789489668007}}",
    };
    for(size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i) {
        assert(!markets_format(0, invalid[i], output, sizeof(output)));
    }
    assert(markets_format(0, valid, output, sizeof(output)));
    assert(!markets_format(0, valid, output, 8));
    assert(output[0] == '\0');
    assert(markets_format(0, " {\"symbol\":\"BTCUSDT\",\"lastPrice\":\"1\","
                             "\"closeTime\":1789489668007\r\n}\t", output, sizeof(output)));
}

int main(void) {
    assert(strcmp(markets_name(0), "Bitcoin BTC/USDT") == 0);
    assert(strstr(markets_url(1), "ETHUSDT") != NULL);
    assert(strstr(markets_url(2), "/XAU") != NULL);
    assert(strstr(markets_url(3), "/wti") != NULL);
    assert(strstr(markets_url(4), "/brent") != NULL);
    test_crypto_prices();
    test_gold_price();
    test_oil_prices();
    test_rejects_untrusted_or_incomplete_data();
    test_complete_object_required();
    puts("markets tests: PASS");
    return 0;
}
