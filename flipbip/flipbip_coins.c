#include <flipbip_coins.h>

// bip44_coin, xprv_version, xpub_version, addr_version, wif_version, addr_format
const uint32_t COIN_INFO_ARRAY[NUM_COINS][COIN_INFO_SIZE] = {
    {0, 0x0488ade4, 0x0488b21e, 0x00, 0x80, CoinTypeBTC0},
    {60, 0x0488ade4, 0x0488b21e, 0x00, 0x80, CoinTypeETH60},
    {3, 0x02fac398, 0x02facafd, 0x1e, 0x9e, CoinTypeBTC0},
    {133, 0x0488ade4, 0x0488b21e, 0x1cb8, 0x80, CoinTypeBTC0},
};

// coin_label, derivation_path, coin_name, static_prefix ("_" for none)
const char* COIN_TEXT_ARRAY[NUM_COINS][COIN_TEXT_SIZE] = {
    {"BTC", "m/44'/0'/0'/0", "bitcoin:", "_"},
    {"ETH", "m/44'/60'/0'/0", "ethereum:", "_"},
    {"DOGE", "m/44'/3'/0'/0", "dogecoin:", "_"},
    {"ZEC", "m/44'/133'/0'/0", "zcash:", "t"}};
