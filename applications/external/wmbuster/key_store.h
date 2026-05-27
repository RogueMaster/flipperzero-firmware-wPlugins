#pragma once
/*
 * Tiny per-meter AES key store, persisted as CSV at WMBUS_APP_KEYS_FILE.
 * Format (one record per line, '#'-comments allowed):
 *   <manuf3>,<id_hex>,<32-hex-key>
 *   KAM,12345678,000102030405060708090A0B0C0D0E0F
 */

#include <stdint.h>
#include <stdbool.h>
#include <storage/storage.h>

typedef struct KeyStore KeyStore;

KeyStore* key_store_alloc(Storage* storage);
void      key_store_free(KeyStore* ks);
bool      key_store_load(KeyStore* ks);
bool      key_store_save(KeyStore* ks);

/* Lookup by 16-bit M-field + 32-bit ID. Returns true if found. */
bool key_store_lookup(KeyStore* ks, uint16_t m, uint32_t id, uint8_t out_key[16]);

/* Add or replace a key. */
bool key_store_set(KeyStore* ks, uint16_t m, uint32_t id, const uint8_t key[16]);

/* Iterate. Calls cb for each entry. */
typedef void (*KeyStoreIterCb)(uint16_t m, uint32_t id, const uint8_t key[16], void* ctx);
void key_store_iter(KeyStore* ks, KeyStoreIterCb cb, void* ctx);
