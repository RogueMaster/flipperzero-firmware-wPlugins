#include <flipbip_coins.h>

// bip44_coin, xprv_version, xpub_version, addr_version, wif_version, addr_format, purpose
const uint32_t COIN_INFO_ARRAY[NUM_COINS][COIN_INFO_SIZE] = {
    {0, 0x0488ade4, 0x0488b21e, 0x00, 0x80, CoinTypeBTC0, 44},
    // BIP84: zprv / zpub
    {0, 0x04b2430c, 0x04b24746, 0x00, 0x80, CoinTypeBTC84, 84},
    {60, 0x0488ade4, 0x0488b21e, 0x00, 0x80, CoinTypeETH60, 44},
    {3, 0x02fac398, 0x02facafd, 0x1e, 0x9e, CoinTypeBTC0, 44},
    {133, 0x0488ade4, 0x0488b21e, 0x1cb8, 0x80, CoinTypeBTC0, 44},
    // XMR: addr_version = primary address network byte (subaddresses use 42)
    {128, 0x0488ade4, 0x0488b21e, 0x12, 0x80, CoinTypeXMR128, 44},
};

// coin_label, derivation_path, uri_scheme, qr_file_prefix, menu_name
const char* COIN_TEXT_ARRAY[NUM_COINS][COIN_TEXT_SIZE] = {
    {"BTC", "m/44'/0'/0'/0", "bitcoin:", "BTC", "BTC"},
    {"BTC", "m/84'/0'/0'/0", "bitcoin:", "BTCSW", "BTC SegWit"},
    {"ETH", "m/44'/60'/0'/0", "ethereum:", "ETH", "ETH"},
    {"DOGE", "m/44'/3'/0'/0", "dogecoin:", "DOGE", "DOGE"},
    {"XMR", "m/44'/128'/0'/0", "monero:", "XMR", "XMR"},
    {"ZEC", "m/44'/133'/0'/0", "zcash:", "ZEC", "ZEC"}};
