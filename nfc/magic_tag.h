#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <nfc/nfc.h>
#include <nfc/protocols/iso15693_3/iso15693_3_poller.h>

/** A generation identifies both a tag family and its command sequence. */
typedef enum {
    MagicGenMfcGen1a,
    MagicGenMfcGen2,
    MagicGenMfcGen3,
    MagicGenMfcGen4,
    MagicGenMfcGtu,
    MagicGenIso15693Gen1,
    MagicGenIso15693Gen2,
    MagicGenIso15693Gen3,
    MagicGenCount,
} MagicGenType;

/** Replace UID and recalculate BCC in a previously read 16-byte MFC block 0. */
void mtools_mfc_prepare_block0(uint8_t block0[16], const uint8_t uid[4]);
bool mtools_mfc_read_block0(Nfc* nfc, uint8_t block0[16]);
bool mtools_mfc_gen4_read_config(Nfc* nfc, uint8_t config[30]);
bool mtools_write_magic_uid_with_block0(
    Nfc* nfc,
    MagicGenType gen,
    const uint8_t* uid,
    size_t uid_len,
    const uint8_t* edited_block0);
const char* mtools_magic_write_error(void);
bool mtools_magic_uid_length_supported(MagicGenType gen, size_t uid_len);
bool mtools_magic_uid_write_implemented(MagicGenType gen);

/** Returns true when a supported read-only generation fingerprint matches. */
bool mtools_detect_magic_tag(Nfc* nfc, MagicGenType gen);
MagicGenType mtools_detect_iso15693_ready(
    Iso15693_3Poller* poller,
    const Iso15693_3SystemInfo* info,
    const uint8_t* uid,
    size_t uid_len);

/** Writes and verifies a UID with the generation-specific command sequence. */
bool mtools_write_magic_uid(Nfc* nfc, MagicGenType gen, const uint8_t* uid, size_t uid_len);
