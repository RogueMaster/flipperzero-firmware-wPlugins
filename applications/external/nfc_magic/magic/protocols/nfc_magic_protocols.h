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
    NfcMagicProtocolClassic, // Last to give priority to the others

    NfcMagicProtocolNum,
    NfcMagicProtocolInvalid,
} NfcMagicProtocol;

const char* nfc_magic_protocols_get_name(NfcMagicProtocol protocol);

#ifdef __cplusplus
}
#endif
