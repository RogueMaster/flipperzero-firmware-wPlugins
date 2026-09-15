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

int main(void) {
    assert(strcmp(markets_name(0), "Bitcoin BTC/USDT") == 0);
    assert(strstr(markets_url(1), "ETHUSDT") != NULL);
    assert(strstr(markets_url(2), "/XAU") != NULL);
    test_crypto_prices();
    test_gold_price();
    test_rejects_untrusted_or_incomplete_data();
    puts("markets tests: PASS");
    return 0;
}
