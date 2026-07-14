#pragma once

#include <stdint.h>
#include <stdlib.h>

#include <mbedtls/des.h>
#include <mbedtls/aes.h>
#include <mbedtls/sha1.h>
#include <mbedtls/sha256.h>

#include "seos_common.h"
#include "aes_cmac.h"
#include "des_cmac.h"

#define SECURE_MESSAGING_MAX_SIZE 128

typedef struct {
    uint8_t cipher;
    uint8_t PrivacyKey[16];
    uint8_t CMACKey[16];
    uint8_t aesContext[16];
    uint8_t desContext[8];
} SecureMessaging;

SecureMessaging* secure_messaging_alloc(AuthParameters* params);

void secure_messaging_free(SecureMessaging* secure_messaging);

void secure_messaging_wrap_apdu(
    SecureMessaging* secure_messaging,
    uint8_t* message,
    size_t message_len,
    uint8_t* apdu_header,
    size_t apdu_header_len,
    BitBuffer* tx_buffer);

void secure_messaging_unwrap_apdu(SecureMessaging* secure_messaging, BitBuffer* rx_buffer);

void secure_messaging_unwrap_rapdu(SecureMessaging* secure_messaging, BitBuffer* rx_buffer);
void secure_messaging_wrap_rapdu(
    SecureMessaging* secure_messaging,
    uint8_t* message,
    size_t message_len,
    BitBuffer* tx_buffer);
