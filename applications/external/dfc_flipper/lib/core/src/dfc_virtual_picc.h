#pragma once

#include <stddef.h>
#include <stdint.h>

#include "dfc_credential.h"
#include "dfc_emulator.h"

#define DFC_VIRTUAL_PICC_MAX_BLOCK_SIZE 51

typedef enum {
    DfcVirtualPiccStatusOk = 0,
    DfcVirtualPiccStatusNoCard,
    DfcVirtualPiccStatusUnsupportedProtocol,
    DfcVirtualPiccStatusBufferTooSmall,
    DfcVirtualPiccStatusProtocolError,
} DfcVirtualPiccStatus;

typedef struct {
    size_t protocol_len;
    size_t uid_len;
    size_t ats_len;
    size_t atqa_len;
    size_t rf_detail_len;
    uint8_t protocol[2];
    // The UID anticollision actually presented. With random ID enabled this is
    // the 4-octet generated one, not the credential's stored UID.
    uint8_t uid[DFC_DESFIRE_UID_MAX_LENGTH];
    // Sized for a user ATS, which may be longer than the built-in default.
    uint8_t ats[DFC_PICC_ATS_MAX];
    uint8_t sak;
    uint8_t atqa[2];
    uint8_t rf_detail[3];
} DfcVirtualPiccActivation;

typedef struct {
    DfcCredential* credential;
    DfcEmulator* emulator;
    size_t pending_command_len;
    size_t pending_response_len;
    size_t pending_response_offset;
    size_t pending_response_prefix_len;
    size_t last_picc_block_len;
    size_t iso_file_index;
    bool activated;
    // Random ID: the UID presented for this activation. Regenerated on every
    // activation and dropped when the field goes away, so a reader that sees the
    // card twice sees two different NFCID1s.
    uint8_t random_uid[DFC_RANDOM_UID_LEN];
    bool random_uid_valid;
    bool iso_dep_selected;
    uint8_t iso_dep_cid;
    uint8_t expected_pcd_sequence;
    uint8_t picc_sequence;
    // Native command capacity plus wrapped APDU header and Le.
    uint8_t pending_command[DFC_WORKER_MAX_BUFFER_SIZE + 6];
    uint8_t pending_response_prefix[3];
    // The pending payload is lent by the emulator's chain buffer rather than
    // its ordinary transmit buffer.
    bool pending_response_staged;
    uint8_t last_picc_block[DFC_VIRTUAL_PICC_MAX_BLOCK_SIZE];
    bool iso_file_selected;
} DfcVirtualPiccSession;

#if DFC_ENABLE_EMULATOR

DfcVirtualPiccSession* dfc_virtual_picc_session_alloc(DfcCredential* credential);
void dfc_virtual_picc_session_free(DfcVirtualPiccSession* session);

// The anticollision parameters this credential is answered with: UID (generated
// when random ID is enabled, and a new one on every call), ATS, SAK and ATQA,
// each the credential's own where it carries one and the built-in default
// otherwise. One authority for that precedence, so the scripted sessions, the
// host tool and the real listener cannot drift apart. Returns false without
// changing `activation` if a random identifier cannot be generated.
bool dfc_virtual_picc_anticollision(
    const DfcCredential* credential,
    DfcVirtualPiccActivation* activation);

DfcVirtualPiccStatus dfc_virtual_picc_field_off(DfcVirtualPiccSession* session);
// Expiry rolls back pending writes and returns the card to its inactive state.
DfcVirtualPiccStatus
    dfc_virtual_picc_advance_time(DfcVirtualPiccSession* session, uint32_t elapsed_milliseconds);
// Clear authentication and ISO-DEP protocol state without changing the
// activation identifier or reallocating any session objects.
DfcVirtualPiccStatus dfc_virtual_picc_reset_protocol(DfcVirtualPiccSession* session);
DfcVirtualPiccStatus dfc_virtual_picc_scan_iso14443a(
    DfcVirtualPiccSession* session,
    DfcVirtualPiccActivation* activation);
// Exchange one APDU or native command. If a completed native operation does
// not fit `response`, its response is retained in the session; retry the exact
// same command with a larger buffer. Other commands are rejected until that
// response is retrieved or the protocol is reset. ISO command handlers that
// can preflight their exact response leave state unchanged on capacity errors.
DfcVirtualPiccStatus dfc_virtual_picc_iso_dep_exchange(
    DfcVirtualPiccSession* session,
    const uint8_t* command,
    size_t command_len,
    uint8_t* response,
    size_t response_capacity,
    size_t* response_len);
// Exchange one ISO-DEP frame. If a completed I-block response does not fit,
// retry the exact frame with a larger response buffer. The command is executed
// once, and other frames are rejected until its response is emitted or the
// protocol is reset.
DfcVirtualPiccStatus dfc_virtual_picc_iso_dep_frame_exchange(
    DfcVirtualPiccSession* session,
    const uint8_t* frame,
    size_t frame_len,
    uint8_t* response,
    size_t response_capacity,
    size_t* response_len);

#endif // DFC_ENABLE_EMULATOR
