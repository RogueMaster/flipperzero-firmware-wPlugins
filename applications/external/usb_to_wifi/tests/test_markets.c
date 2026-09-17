#include "markets.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_crypto_prices(void) {
    const char* bitcoin = "{\"symbol\":\"BTCUSDT\",\"lastPrice\":\"76471.76000000\","
                          "\"closeTime\":1789489668007}";
    const char* ethereum = "{\"symbol\":\"ETHUSDT\",\"lastPrice\":\"2280.12000000\","
                           "\"closeTime\":1789489668007}";
    char output[192];

    assert(markets_format(0, bitcoin, output, sizeof(output)));
    assert(strstr(output, "76471.76 USDT") != NULL);
    assert(strstr(output, "Source: Binance") != NULL);

    assert(markets_format(1, ethereum, output, sizeof(output)));
    assert(strstr(output, "2280.12 USDT") != NULL);

    const char* solana = "{\"symbol\":\"SOLUSDT\",\"lastPrice\":\"135.42000000\","
                         "\"closeTime\":1789489668007}";
    assert(markets_format_coin("SOLUSDT", solana, output, sizeof(output)));
    assert(strstr(output, "135.42 USDT") != NULL);
    assert(!markets_format_coin("ADAUSDT", solana, output, sizeof(output)));
}

static void test_coin_request(void) {
    char symbol[MARKETS_MAX_SYMBOL_LENGTH + 1U];
    char url[160];
    assert(markets_build_coin_request("sol", symbol, sizeof(symbol), url, sizeof(url)));
    assert(strcmp(symbol, "SOLUSDT") == 0);
    assert(strstr(url, "symbol=SOLUSDT") != NULL);
    assert(markets_build_coin_request("1000shibusdt", symbol, sizeof(symbol), url, sizeof(url)));
    assert(strcmp(symbol, "1000SHIBUSDT") == 0);
    assert(!markets_build_coin_request("SOL/USDT", symbol, sizeof(symbol), url, sizeof(url)));
    assert(!markets_build_coin_request("A", symbol, sizeof(symbol), url, sizeof(url)));
    assert(!markets_build_coin_request(
        "thissymbolisfarwaytoolong", symbol, sizeof(symbol), url, sizeof(url)));
}

static void test_gold_price(void) {
    const char* gold = "{\"currency\":\"USD\",\"name\":\"Gold\",\"price\":4297.700195,"
                       "\"symbol\":\"XAU\",\"updatedAt\":\"2026-09-15T16:27:40Z\"}";
    char output[192];

    assert(markets_format(2, gold, output, sizeof(output)));
    assert(strstr(output, "4297.700195 USD / troy oz") != NULL);
    assert(strstr(output, "2026-09-15T16:27:40Z") != NULL);

    const char* silver = "{\"currency\":\"USD\",\"name\":\"Silver\",\"price\":64.624001,"
                         "\"symbol\":\"XAG\",\"updatedAt\":\"2026-09-16T14:48:42Z\"}";
    assert(markets_format(4, silver, output, sizeof(output)));
    assert(strstr(output, "64.624001 USD / troy oz") != NULL);
    assert(strstr(output, "Source: Gold API") != NULL);
    assert(!markets_format(4, gold, output, sizeof(output)));
}

static void test_oil_prices(void) {
    const char* brent = "{\"symbol\":\"BZUSDT\",\"lastPrice\":\"100.25000\","
                        "\"closeTime\":1789572303865}";
    char output[192];
    assert(markets_format(3, brent, output, sizeof(output)));
    assert(strstr(output, "100.25 USDT / barrel") != NULL);
    assert(strstr(output, "Source: Binance") != NULL);
    assert(!markets_format(
        3,
        "{\"symbol\":\"BTCUSDT\",\"lastPrice\":\"1\","
        "\"closeTime\":1789572303865}",
        output,
        sizeof(output)));
}

static void test_rejects_untrusted_or_incomplete_data(void) {
    char output[64];
    assert(!markets_format(
        0,
        "{\"symbol\":\"ETHUSDT\",\"lastPrice\":\"1\","
        "\"closeTime\":1}",
        output,
        sizeof(output)));
    assert(!markets_format(
        2,
        "{\"symbol\":\"XAU\",\"currency\":\"EUR\","
        "\"price\":1,\"updatedAt\":\"now\"}",
        output,
        sizeof(output)));
    assert(!markets_format(
        0,
        "{\"symbol\":\"BTCUSDT\",\"lastPrice\":\"NaN\","
        "\"closeTime\":1}",
        output,
        sizeof(output)));
    assert(!markets_format(
        0, "{\"symbol\":\"BTCUSDT\",\"lastPrice\":\"1\"}", output, sizeof(output)));
    assert(!markets_format(
        0,
        "{\"symbol\":\"BTCUSDT\",\"lastPrice\":\"1\","
        "\"closeTime\":1}",
        output,
        8));
    assert(!markets_format(
        0,
        "{\"symbol\":\"BTCUSDT\",\"lastPrice\":\"1\","
        "\"closeTime\":123}",
        output,
        sizeof(output)));
    assert(!markets_format(
        2,
        "{\"symbol\":\"XAU\",\"currency\":\"USD\","
        "\"price\":1,\"updatedAt\":\"not-a-time\"}",
        output,
        sizeof(output)));
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
    assert(markets_format(
        0,
        " {\"symbol\":\"BTCUSDT\",\"lastPrice\":\"1\","
        "\"closeTime\":1789489668007\r\n}\t",
        output,
        sizeof(output)));
}

int main(void) {
    assert(strcmp(markets_name(0), "Bitcoin BTC/USDT") == 0);
    assert(strstr(markets_url(1), "ETHUSDT") != NULL);
    assert(strstr(markets_url(2), "/XAU") != NULL);
    assert(strstr(markets_url(3), "symbol=BZUSDT") != NULL);
    assert(strstr(markets_url(4), "/XAG") != NULL);
    assert(strcmp(markets_name(4), "Silver USD/oz") == 0);
    test_crypto_prices();
    test_coin_request();
    test_gold_price();
    test_oil_prices();
    test_rejects_untrusted_or_incomplete_data();
    test_complete_object_required();
    puts("markets tests: PASS");
    return 0;
}
