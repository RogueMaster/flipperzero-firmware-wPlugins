#pragma once

#include <stdbool.h>
#include <stddef.h>

// How the mnemonic is encrypted on the SD card (apps_data/flipbip).
typedef enum {
    // .flipbip.key + .flipbip.dat: RC4 under a random key, which is itself
    // encrypted with a key built into the app. Any Flipper with FlipBIP can
    // open these files; they are only obfuscated.
    FlipBipEncryptionLegacy,
    // .flipbip.dat: AES-256-CBC under this Flipper's unique secure enclave key
    // (slot 11, never leaves the chip). Only this Flipper can open it.
    // Same file format as the Astrrra/FlipBIP fork, so its wallets load too.
    FlipBipEncryptionEnclave,
} FlipBipEncryption;

typedef enum {
    FlipBipWalletNone,
    FlipBipWalletLegacy,
    FlipBipWalletEnclave,
} FlipBipWalletFormat;

// True if this device has a working, factory provisioned secure enclave.
// False on Flipper clones (e.g. Kiisu) and if the radio core is not running.
bool flipbip_enclave_available(void);

// Format of the saved wallet, from the files on the SD card.
FlipBipWalletFormat flipbip_wallet_format(void);

// Finish a mode switch that was interrupted (power loss, SD removed): drop
// temporary files and, for an enclave wallet, any leftover legacy key files.
// Call once at app start.
void flipbip_wallet_recover(void);

// Load the mnemonic (up to 255 chars) from either format. Falls back to the
// backup files if the main ones are missing or damaged.
bool flipbip_load_file_secure(char* mnemonic);

// Save a mnemonic, replacing any wallet. Written to temporary files, read back
// and checked, then renamed into place, so a failure leaves the old wallet.
bool flipbip_save_file_secure(const char* mnemonic, FlipBipEncryption encryption);

// Re-encrypt the saved wallet in another format (no-op if already in it).
bool flipbip_switch_encryption(FlipBipEncryption encryption);

bool flipbip_save_qrfile(
    const char* qr_msg_prefix,
    const char* qr_msg_content,
    const char* file_name);
