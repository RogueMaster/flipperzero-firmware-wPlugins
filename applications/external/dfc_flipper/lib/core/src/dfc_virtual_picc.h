#pragma once

#include <stddef.h>
#include <stdint.h>

#include "dfc_credential.h"
#include "dfc_emulator.h"

typedef enum {
    DfcVirtualPiccStatusOk = 0,
    DfcVirtualPiccStatusNoCard,
    DfcVirtualPiccStatusUnsupportedProtocol,
    DfcVirtualPiccStatusBufferTooSmall,
    DfcVirtualPiccStatusProtocolError,
} DfcVirtualPiccStatus;

typedef struct {
    uint8_t protocol[2];
    size_t protocol_len;
    // The UID anticollision actually presented. With random ID enabled this is
    // the 4-octet generated one, not the credential's stored UID.
    uint8_t uid[DFC_DESFIRE_UID_MAX_LENGTH];
    size_t uid_len;
    // Sized for a user ATS, which may be longer than the built-in default.
    uint8_t ats[DFC_PICC_ATS_MAX];
    size_t ats_len;
    uint8_t sak;
    uint8_t atqa[2];
    size_t atqa_len;
    uint8_t rf_detail[3];
    size_t rf_detail_len;
} DfcVirtualPiccActivation;

typedef struct {
    DfcCredential* credential;
    DfcEmulator* emulator;
    bool activated;
    // Random ID: the UID presented for this activation. Regenerated on every
    // activation and dropped when the field goes away, so a reader that sees the
    // card twice sees two different NFCID1s.
    uint8_t random_uid[DFC_RANDOM_UID_LEN];
    bool random_uid_valid;
    bool iso_dep_selected;
    uint8_t expected_pcd_sequence;
    uint8_t picc_sequence;
    uint8_t pending_response[DFC_WORKER_MAX_BUFFER_SIZE];
    size_t pending_response_len;
    size_t pending_response_offset;
    uint8_t pending_response_prefix[3];
    size_t pending_response_prefix_len;
    bool iso_file_selected;
    size_t iso_file_index;
} DfcVirtualPiccSession;

DfcVirtualPiccSession* dfc_virtual_picc_session_alloc(DfcCredential* credential);
void dfc_virtual_picc_session_free(DfcVirtualPiccSession* session);

// The anticollision parameters this credential is answered with: UID (generated
// when random ID is enabled, and a new one on every call), ATS, SAK and ATQA,
// each the credential's own where it carries one and the built-in default
// otherwise. One authority for that precedence, so the scripted sessions, the
// host tool and the real listener cannot drift apart.
void dfc_virtual_picc_anticollision(
    const DfcCredential* credential,
    DfcVirtualPiccActivation* activation);

DfcVirtualPiccStatus dfc_virtual_picc_field_off(DfcVirtualPiccSession* session);
// Clear authentication and ISO-DEP protocol state without changing the
// activation identifier or reallocating any session objects.
DfcVirtualPiccStatus dfc_virtual_picc_reset_protocol(DfcVirtualPiccSession* session);
DfcVirtualPiccStatus dfc_virtual_picc_scan_iso14443a(
    DfcVirtualPiccSession* session,
    DfcVirtualPiccActivation* activation);
DfcVirtualPiccStatus dfc_virtual_picc_iso_dep_exchange(
    DfcVirtualPiccSession* session,
    const uint8_t* command,
    size_t command_len,
    uint8_t* response,
    size_t response_capacity,
    size_t* response_len);
DfcVirtualPiccStatus dfc_virtual_picc_iso_dep_frame_exchange(
    DfcVirtualPiccSession* session,
    const uint8_t* frame,
    size_t frame_len,
    uint8_t* response,
    size_t response_capacity,
    size_t* response_len);
