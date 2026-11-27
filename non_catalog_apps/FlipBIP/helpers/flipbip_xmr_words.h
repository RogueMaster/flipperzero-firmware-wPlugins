#pragma once

#include <stddef.h>
#include <stdint.h>

// Monero (CryptoNote "Electrum style") 25 word mnemonic, English only.

#define FLIPBIP_XMR_WORDS_NUM        1626
#define FLIPBIP_XMR_WORD_MAX_LEN     12
#define FLIPBIP_XMR_WORDS_PREFIX_LEN 3 // letters per word fed to the checksum

// 25 words of up to 12 letters, 24 separating spaces, null terminator
#define FLIPBIP_XMR_MNEMONIC_BUF (25 * FLIPBIP_XMR_WORD_MAX_LEN + 24 + 1)

extern const uint8_t FLIPBIP_XMR_WORDS_PACKED[];

// Encode a 32 byte private spend key as 25 space separated words into `out`
// (at least FLIPBIP_XMR_MNEMONIC_BUF bytes).
void flipbip_xmr_mnemonic(const uint8_t spend_key[32], char* out);
