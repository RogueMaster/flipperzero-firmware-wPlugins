#pragma once
#include <stdint.h>

#define NUM_COINS 6

// Order matches the rows of the tables below (and the wallet menu). The same
// values double as the address format in COIN_INFO_ADDR_FMT.
typedef enum {
    CoinTypeBTC0, // legacy P2PKH (also DOGE, ZEC transparent)
    CoinTypeBTC84, // native SegWit P2WPKH, bech32
    CoinTypeETH60,
    CoinTypeDOGE3,
    CoinTypeXMR128,
    CoinTypeZEC133
} CoinType;

#define COIN_INFO_SIZE       7
#define COIN_INFO_BIP44_COIN 0
#define COIN_INFO_XPRV_VERS  1
#define COIN_INFO_XPUB_VERS  2
#define COIN_INFO_ADDR_VERS  3
#define COIN_INFO_WIF_VERS   4
#define COIN_INFO_ADDR_FMT   5
#define COIN_INFO_PURPOSE    6

// bip44_coin, xprv_version, xpub_version, addr_version, wif_version, addr_format, purpose
extern const uint32_t COIN_INFO_ARRAY[NUM_COINS][COIN_INFO_SIZE];

#define COIN_TEXT_SIZE  5
#define COIN_TEXT_LABEL 0
#define COIN_TEXT_DERIV 1
#define COIN_TEXT_NAME  2
#define COIN_TEXT_FILE  3
#define COIN_TEXT_MENU  4

// coin_label, derivation_path, uri_scheme, qr_file_prefix, menu_name
extern const char* COIN_TEXT_ARRAY[NUM_COINS][COIN_TEXT_SIZE];
