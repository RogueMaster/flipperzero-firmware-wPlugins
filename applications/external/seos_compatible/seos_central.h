#pragma once

#include <furi.h>
#include <lib/toolbox/bit_buffer.h>

#include <mbedtls/des.h>
#include <mbedtls/aes.h>

#include "secure_messaging.h"
#include "seos_common.h"
#include "seos_credential.h"
#include "seos.h"
#include "seos_att.h"
#include "keys.h"

typedef struct {
    Seos* seos;
    SeosAtt* seos_att;
    BitBuffer* rx_buffer;

    SeosPhase phase;

    AuthParameters params;
    SecureMessaging* secure_messaging;

    SeosCredential* credential;
} SeosCentral;

SeosCentral* seos_central_alloc(Seos* seos);
void seos_central_free(SeosCentral* seos_central);
void seos_central_start(SeosCentral* seos_central, FlowMode mode);
void seos_central_stop(SeosCentral* seos_central);
void seos_central_notify(void* context, const uint8_t* buffer, size_t buffer_len);
