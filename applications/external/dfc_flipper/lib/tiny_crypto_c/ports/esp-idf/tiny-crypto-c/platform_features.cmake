# SPDX-License-Identifier: GPL-2.0-or-later
#
# Primitives the ESP-IDF integration needs with any TINY_CRYPTO_TARGET,
# including none. cmake/Features.cmake treats each listed macro as a floor: AUTO
# selects it and an explicit OFF fails configuration.

# The vendored bootloader_support component hashes images with SHA-256,
# SHA-384 and SHA-512 through ports/esp-idf/bootloader_hash.c.
set(tc_platform_features TC_ENABLE_SHA256 TC_ENABLE_SHA384 TC_ENABLE_SHA512)
# Secure Boot v2 signed updates use RSA-3072 PSS or ECDSA over P-192/P-256.
if(CONFIG_SECURE_SIGNED_ON_UPDATE AND CONFIG_SECURE_SIGNED_APPS_RSA_SCHEME)
  list(APPEND tc_platform_features TC_ENABLE_RSA)
endif()
if(CONFIG_SECURE_SIGNED_ON_UPDATE AND CONFIG_SECURE_SIGNED_APPS_ECDSA_V2_SCHEME)
  list(APPEND tc_platform_features TC_ENABLE_EC TC_EC_ENABLE_P192 TC_EC_ENABLE_P256)
endif()
