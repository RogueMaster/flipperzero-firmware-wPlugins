#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The cipher-based message authentication code, over AES and over two-key
 * 3DES.
 *
 * The two differ only in block size and in the constant the subkeys are
 * derived with, so they share an implementation. Both take a sixteen byte key
 * and return a full block: sixteen bytes for AES, eight for 3DES. Callers
 * truncate to what they need.
 */

bool aes_cmac(uint8_t* key, size_t key_len, uint8_t* message, size_t message_len, uint8_t* cmac);
bool des_cmac(uint8_t* key, size_t key_len, uint8_t* message, size_t message_len, uint8_t* cmac);

#ifdef __cplusplus
}
#endif
