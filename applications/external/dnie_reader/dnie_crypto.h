#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// Contadores de la KDF (TR-03110-3 / ICAO 9303-11)
#define DNIE_KDF_ENC 1
#define DNIE_KDF_MAC 2
#define DNIE_KDF_PI  3 // clave derivada de la contrasena (CAN)

// KDF para AES-128: SHA-1(secret || counter en big-endian 32 bits), primeros 16 bytes
bool dnie_kdf_aes128(const uint8_t* secret, size_t secret_len, uint32_t counter, uint8_t out[16]);

// AES-128-CBC con IV = 0. len debe ser multiplo de 16.
bool dnie_aes128_cbc_decrypt_iv0(
    const uint8_t key[16],
    const uint8_t* in,
    size_t len,
    uint8_t* out);

// Un bloque AES-128-ECB (para calcular el IV de Secure Messaging: E(KSenc, SSC))
bool dnie_aes128_ecb_encrypt(const uint8_t key[16], const uint8_t in[16], uint8_t out[16]);

// AES-128-CBC con IV arbitrario. len multiplo de 16. iv no se modifica.
bool dnie_aes128_cbc(
    const uint8_t key[16],
    const uint8_t iv[16],
    bool encrypt,
    const uint8_t* in,
    size_t len,
    uint8_t* out);

// --- 3DES (Chip Authentication v1 de este DNIe y canales BAC) ---

// KDF para 3DES: SHA-1(secret || counter)[0..16] con bits de paridad DES ajustados
bool dnie_kdf_3des(const uint8_t* secret, size_t secret_len, uint32_t counter, uint8_t out[16]);

// 3DES de dos claves (K1,K2,K1) en CBC con IV = 0. len multiplo de 8.
bool dnie_3des_cbc_iv0(
    const uint8_t key[16],
    bool encrypt,
    const uint8_t* in,
    size_t len,
    uint8_t* out);

// MAC "retail" (ISO/IEC 9797-1, algoritmo 3): DES-CBC con K1 y al final DES^-1 K2, DES K1.
// data ya debe venir rellenado a multiplo de 8.
bool dnie_retail_mac(const uint8_t key[16], const uint8_t* data, size_t len, uint8_t out[8]);

// AES-CMAC (RFC 4493 / NIST SP 800-38B). Devuelve los 16 bytes completos.
bool dnie_aes128_cmac(const uint8_t key[16], const uint8_t* data, size_t len, uint8_t out[16]);
