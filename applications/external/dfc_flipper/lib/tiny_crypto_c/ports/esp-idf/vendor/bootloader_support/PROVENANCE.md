# ESP-IDF application bootloader support

Source: Espressif ESP-IDF, `components/bootloader_support`, commit
`fcae32885b0296b32044cb99ecbdc50d98dddb83` (installed ESP-IDF 5.5).
Upstream: https://github.com/espressif/esp-idf/tree/fcae32885b0296b32044cb99ecbdc50d98dddb83/components/bootloader_support

The component is vendored under Apache-2.0; see LICENSE and individual file
notices. Upstream headers are retained unchanged. CMakeLists.txt is
modified to select ../../bootloader_hash.c instead of src/bootloader_sha.c and
depend on tiny_crypto_c instead of mbedTLS. RSA signature verification selects
../../signed_update_rsa.c; ECDSA v2 selects ../../signed_update_ecdsa.c.
Unused mbedTLS includes are removed from
src/secure_boot_v2/secure_boot_signatures_app.c. Its explicit digest-index
verification rejects failed eFuse reads and missing digests before comparison;
the digest loop index matches the unsigned count. Trust selection is unchanged.
The override is application-only and rejects bootloader, TEE and RAM-app builds.

The replacement hash adapter is original project code under GPL-2.0-or-later.
It implements the application's opaque hash-handle API with SHA-256/384/512.
It allocates one context per active handle and wipes it when finished or canceled.
This allocation belongs to the IDF adapter, not the library hash API. The original
RSA adapter converts the signature block's little-endian fields and calls the
library's RSA-PSS verifier using SHA-256 and a 32-byte salt.
The original ECDSA adapter converts P-192/P-256 coordinates and signature
integers to big-endian and verifies the image digest with the library.

Only the application adds this directory to EXTRA_COMPONENT_DIRS. The separate
bootloader uses the installed IDF component, not this copy. Signature verification
is not disabled: unsupported signed-update configurations fail at configuration.
