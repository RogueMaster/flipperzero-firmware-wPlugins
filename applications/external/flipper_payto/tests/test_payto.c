#include "../payto.h"

#include <assert.h>
#include <string.h>

static void test_uri(void) {
    PaytoWallet wallet;
    assert(payto_parse_uri("payto://xcb/cb7147879011ea207df5b35a24ca6f0859dcfb145999", &wallet));
    assert(strcmp(wallet.currency, "XCB") == 0);
    assert(strcmp(wallet.short_name, "XCB: CB71-5999") == 0);
    assert(payto_parse_uri("payto://xab/ab792215c43fc213c02182c8389f2bc32408e2c50922", &wallet));
    assert(payto_parse_uri("payto://xce/ce450000000000000000000000000000000000000000", &wallet));
    assert(!payto_parse_uri("payto://xcb/cb7147879011ea207df5b35a24ca6f0859dcfb145998", &wallet));
    assert(!payto_parse_uri("payto://xce/ab792215c43fc213c02182c8389f2bc32408e2c50922", &wallet));
    assert(!payto_parse_uri("https://example.com/wallet", &wallet));
    assert(!payto_parse_uri("payto://xcb/", &wallet));
    assert(!payto_parse_uri("payto://xcb/invalid space", &wallet));
}

static void test_ndef(void) {
    const char* uri = "payto://xcb/cb7147879011ea207df5b35a24ca6f0859dcfb145999";
    unsigned char ndef[256] = {0xD1, 0x01, 0, 0x55, 0x00};
    ndef[2] = (unsigned char)(strlen(uri) + 1);
    memcpy(&ndef[5], uri, strlen(uri));
    PaytoWallet wallet;
    assert(payto_parse_ndef(ndef, strlen(uri) + 5, &wallet));
    assert(strcmp(wallet.short_name, "XCB: CB71-5999") == 0);
    assert(!payto_parse_ndef(ndef, 3, &wallet));
}

int main(void) {
    test_uri();
    test_ndef();
    return 0;
}
