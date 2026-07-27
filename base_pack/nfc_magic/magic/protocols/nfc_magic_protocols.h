#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    NfcMagicProtocolGen1,
    // Gen4 is probed before Gen2: a wiped/blank UMC (e.g. after
    // `hf_mf_ultimatecard -w 0`) also satisfies the Gen2 CUID heuristic (default keys,
    // writable block 0, static nonce), so its GTU backdoor must be checked first to
    // classify it as Gen4 rather than as Gen2 CUID.
    NfcMagicProtocolGen4,
    NfcMagicProtocolGen2,
    NfcMagicProtocolUscuidUl, // Ultralight family (USCUID-UL)
    // ISO-A Ultralight that activated but answered no magic signature (not a confirmed
    // USCUID-UL, not an AA-55 UL-5). Offers a non-confirmed "write anyway" path via the
    // direct engine, mirroring how NfcMagicProtocolClassic encodes an unconfirmed Classic.
    NfcMagicProtocolUscuidUlNotDetected,
    NfcMagicProtocolIso15693, // ISO15693 magic candidate, probed explicitly in
    // nfc_magic_scanner_detect_pass, not by enum order
    NfcMagicProtocolClassic, // generic ISO-A MIFARE Classic (unconfirmed-magic "try writing"
        // fallback); detection priority is set explicitly in
        // nfc_magic_scanner_detect_pass, not by enum order

    NfcMagicProtocolNum,
    NfcMagicProtocolInvalid,
} NfcMagicProtocol;

const char* nfc_magic_protocols_get_name(NfcMagicProtocol protocol);

#ifdef __cplusplus
}
#endif
