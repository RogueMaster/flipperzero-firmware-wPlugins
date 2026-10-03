<!-- SPDX-FileCopyrightText: Mistial Dev -->

<!-- SPDX-License-Identifier: GPL-2.0-or-later -->

# ESP32-P4 and PIV targets

The application uses a vendored bootloader-support component with our
SHA-256/384/512 adapter for image hashing. The separate bootloader uses the
original IDF component. See the [vendor provenance](../ports/esp-idf/vendor/bootloader_support/PROVENANCE.md).
Application builds reject mbedTLS dependencies. Secure Boot v2 signature
verification uses RSA-3072 PSS or P-192/P-256 ECDSA through the library.

The vendored component is pinned to ESP-IDF 5.5. Select `piv-acu` or `piv-pd` with
`TINY_CRYPTO_TARGET`. Resource tuning stays separate: either role can use
`micro`, `mini`, or `desktop` without enabling unrelated algorithms.

## Role selection

Both roles enable AES-128 ECB and CBC for OSDP and TWIC object encryption,
SHA-1/256/384, KMAC256 for PIV Auto, the key-challenge helpers, RSA-2048/3072/4096 and
P-256/P-384 for signature verification, and TLV, DER and BER parsing. They also enable
X.509 parsing, path validation, CRL and OCSP revocation, CMS and CMS validation, PIV/TWIC
identifiers and objects, CHUID parsing, credential validation, FASC-N and TWIC UUID
helpers, AAMVA barcode fields and TWIC privacy keys. GZIP handles compressed certificates.
CMS needs BER framing support. Signed-attribute compatibility remains an explicit
application option. The PD also enables dynamic AES, the single-step KDF, PIV CVC
parsing, and CS2/CS7 Secure Messaging cryptography for the card channel.

The role lists exclude APDU and PIV card commands, key proofs, the PIV catalog and card
check, Secure Messaging APDU framing, VCI and the TWIC CCL reader. A role build reports a
configuration error when one of these is set to `ON`.

SHA-1 supports legacy TWIC signatures described in TWIC Part 2 section 3.3.4.
Applications must enforce their accepted signature algorithms and legacy policy.
Enabling the hash supplies only the implementation.

Applications supply access policy, trust anchors and revocation data.
The component enables SHA-256, SHA-384 and SHA-512 for the vendored image hash.
Signed-update configuration adds RSA, or EC with P-192 and P-256. These
platform features apply with or without a role. `AUTO` selects them and an
explicit `OFF` fails configuration.
The native X.509 signature provider uses [ECDSA verification](ec.md) and
[RSA verification](rsa.md). [X.509 path validation](x509-path.md) accepts an
ordered chain, an explicit trust anchor and a signature provider.
[Path construction](x509-store.md) searches application-supplied certificates
and trust anchors. Revocation has a separate API. The PD must verify the CVC
chain before passing its key to Secure Messaging. The ACU must validate
certificates and authentication responses under its trust policy.

Each role fixes its algorithm list. Conflicting feature overrides fail at
configuration time. Leave `TINY_CRYPTO_TARGET` empty for custom selections.
The same role option works in host CMake builds.

## Build

Install ESP-IDF with ESP32-P4 support and activate its environment using
`export.sh`. From this repository's root:

```sh
idf.py -C examples/esp32-p4 -B /tmp/tiny-crypto-esp32p4-acu \
  -DTINY_CRYPTO_TARGET=piv-acu build
idf.py -C examples/esp32-p4 -B /tmp/tiny-crypto-esp32p4-pd \
  -DTINY_CRYPTO_TARGET=piv-pd build
```

The example defaults to `mini`, uses an 8 KiB main-task stack, and runs without
PSRAM. Its 2 MiB flash setting is a build default. Set your board's actual flash
capacity and partition table in `menuconfig` before flashing.
The configuration file stays in the selected build directory.

```sh
idf.py -C examples/esp32-p4 -B /tmp/tiny-crypto-esp32p4-acu menuconfig
idf.py -C examples/esp32-p4 -B /tmp/tiny-crypto-esp32p4-acu size
idf.py -C examples/esp32-p4 -B /tmp/tiny-crypto-esp32p4-acu size-components
idf.py -C examples/esp32-p4 -B /tmp/tiny-crypto-esp32p4-acu -p PORT flash monitor
```

The example times KMAC256 and P-256/P-384 public-key generation in both roles.
It prints internal heap availability and the main task's unused stack.
Timing requires a board. These timings cover individual operations and exclude
a complete PIV transaction. Unused library functions are removed from the linked
image.
ESP-IDF's size percentages use linker regions and configured flash, which
can differ from physical board capacity.

## Signed updates

For a signed-update build without hardware secure boot, use a fresh build
directory and one of the supplied configuration fragments:

```sh
idf.py -C examples/esp32-p4 -B /tmp/tiny-crypto-esp32p4-signed-rsa \
  '-DSDKCONFIG_DEFAULTS=sdkconfig.defaults;sdkconfig.signed-rsa' build
idf.py -C examples/esp32-p4 -B /tmp/tiny-crypto-esp32p4-signed-ecdsa \
  '-DSDKCONFIG_DEFAULTS=sdkconfig.defaults;sdkconfig.signed-ecdsa' build
```

These builds require external signing before installation. Hardware secure
boot and key provisioning remain separate steps. Defaults apply only to settings
absent from a build directory's `sdkconfig`.

Select signed applications and the RSA or ECDSA-v2 scheme in `menuconfig`.
The application override retains IDF's signature-block checks and trusted-key
selection. Without hardware secure boot, IDF takes the trusted key from the
running application's signature block. With secure boot enabled, it uses
the eFuse key digests. Provisioning and revocation remain IDF operations.

The benchmark excludes update download and installation. Signed-update builds
retain the signature verifier, `esp_ota_end` and `esp_ota_set_boot_partition`
to check the full verification and boot-selection link.
Use IDF's OTA APIs in an application: finish writing and validating the image
with `esp_ota_end`, then select it with `esp_ota_set_boot_partition`. The
signature verifier alone covers one step of the OTA workflow.

Downgrade protection requires rollback configuration beyond signature
verification. IDF's
`CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE` handles an unconfirmed application's
fallback, while `CONFIG_BOOTLOADER_APP_ANTI_ROLLBACK` enforces its secure
version. Both are disabled in the example defaults. Configure an OTA partition
table and test confirmation and recovery before deploying either feature.
Secure-version advancement and secure-boot provisioning can change eFuses
irreversibly. Do not use production hardware for initial validation.

Host [signed-image and policy tests](testing.md#esp-idf-signed-image-tests)
exercise RSA and ECDSA. Reboot, interrupted-write and hardware eFuse behavior
require on-device testing.

## Use as a component

Add this directory to `EXTRA_COMPONENT_DIRS` before including ESP-IDF's
`project.cmake`:

```cmake
list(APPEND EXTRA_COMPONENT_DIRS "/path/to/tiny_crypto_c/ports/esp-idf/tiny_crypto_c")
list(APPEND EXTRA_COMPONENT_DIRS "/path/to/tiny_crypto_c/ports/esp-idf/vendor/bootloader_support")
set(TINY_CRYPTO_TARGET piv-acu CACHE STRING "PIV role")
set(TINY_CRYPTO_RESOURCE_PROFILE mini CACHE STRING "Resource profile")
```

Your component declares `REQUIRES tiny_crypto_c`. The adapter uses the normal
CMake source and feature selection, so public headers and library objects get
the same configuration. The core has no ESP-IDF dependency or hardware crypto
backend.

See the [ESP-IDF build guide](https://docs.espressif.com/projects/esp-idf/en/v5.5.1/esp32p4/api-guides/build-system.html)
for component integration and [Running the tests](testing.md) for host suites.
