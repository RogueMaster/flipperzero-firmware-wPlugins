# SPDX-License-Identifier: GPL-2.0-or-later
idf_component_register(SRCS "main.c" REQUIRES tiny_crypto_c esp_timer esp_system heap)
target_compile_definitions(${COMPONENT_LIB} PRIVATE TC_APPLICATION_TARGET="${TINY_CRYPTO_TARGET}")
if(CONFIG_SECURE_SIGNED_ON_UPDATE)
  # Check the complete verifier link even though this benchmark does not install updates.
  target_link_options(${COMPONENT_LIB} INTERFACE
    "-Wl,--undefined=esp_secure_boot_verify_signature"
    "-Wl,--undefined=esp_ota_end"
    "-Wl,--undefined=esp_ota_set_boot_partition")
endif()
