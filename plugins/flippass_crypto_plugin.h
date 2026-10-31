#pragma once

#include <furi.h>
#include <flipper_application/flipper_application.h>

#include "../flippass_crypto.h"

#ifdef __cplusplus
extern "C" {
#endif

#define FLIPPASS_CRYPTO_PLUGIN_APP_ID      "flippass_crypto"
#define FLIPPASS_CRYPTO_PLUGIN_API_VERSION FLIPPASS_CRYPTO_API_VERSION

typedef FlipPassCryptoApiV1 FlipPassCryptoPluginV1;

const FlipperAppPluginDescriptor* flippass_crypto_plugin_ep(void);
const FlipPassCryptoApiV1* flippass_crypto_plugin_api(void);

#ifdef __cplusplus
}
#endif
