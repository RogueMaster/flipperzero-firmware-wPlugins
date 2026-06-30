#pragma once

#include <stdint.h>
#include <stdlib.h>

#include <mbedtls/des.h>
#include <mbedtls/sha1.h>

#include <furi.h>
#include <lib/toolbox/bit_buffer.h>

#include "passy_common.h"

#define SECURE_MESSAGING_MAX_SIZE 128

typedef struct {
    uint8_t KENC[16];
    uint8_t KMAC[16];

    uint8_t rndICC[8];
    uint8_t rndIFD[8];

    uint8_t Kifd[16];
    uint8_t Kicc[16];

    uint8_t KSenc[16];
    uint8_t KSmac[16];
    uint8_t SSC[8];

} SecureMessaging;

SecureMessaging* passy_secure_messaging_alloc(
    uint8_t* passport_number,
    uint8_t* date_of_birth,
    uint8_t* date_of_expiry);

void passy_secure_messaging_free(SecureMessaging* secure_messaging);

void passy_secure_messaging_calculate_session_keys(SecureMessaging* secure_messaging);

void passy_secure_messaging_wrap_apdu(SecureMessaging* secure_messaging, BitBuffer* tx_buffer);

void passy_secure_messaging_unwrap_rapdu(SecureMessaging* secure_messaging, BitBuffer* rx_buffer);
