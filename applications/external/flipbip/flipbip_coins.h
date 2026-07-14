#pragma once
#include <stdint.h>

#define NUM_COINS 4

typedef enum {
    CoinTypeBTC0,
    CoinTypeETH60,
    CoinTypeDOGE3,
    CoinTypeZEC133,
} CoinType;

#define COIN_INFO_SIZE       6
#define COIN_INFO_BIP44_COIN 0
#define COIN_INFO_XPRV_VERS  1
#define COIN_INFO_XPUB_VERS  2
#define COIN_INFO_ADDR_VERS  3
#define COIN_INFO_WIF_VERS   4
#define COIN_INFO_ADDR_FMT   5

// bip44_coin, xprv_version, xpub_version, addr_version, wif_version, addr_format
extern const uint32_t COIN_INFO_ARRAY[NUM_COINS][COIN_INFO_SIZE];

#define COIN_TEXT_SIZE   4
#define COIN_TEXT_LABEL  0
#define COIN_TEXT_DERIV  1
#define COIN_TEXT_NAME   2
#define COIN_TEXT_PREFIX 3

// coin_label, derivation_path, coin_name, static_prefix ("_" for none)
extern const char* COIN_TEXT_ARRAY[NUM_COINS][COIN_TEXT_SIZE];
