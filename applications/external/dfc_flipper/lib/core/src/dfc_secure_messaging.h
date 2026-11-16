#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "dfc_common.h"

#define DFC_SM_MAX_SIZE              192
#define DFC_SM_MAX_CIPHER_BLOCK_SIZE 16
#define DFC_SM_LEGACY_BLOCK_SIZE     8
#define DFC_SM_LEGACY_MAC_LENGTH     4
#define DFC_SM_LENGTH_MARKER         0x80
#define DFC_SM_MAX_CRYPTO_SIZE       (DFC_MAX_FILE_DATA + DFC_SM_MAX_CIPHER_BLOCK_SIZE)

typedef struct {
    uint8_t cipher; // DFC_CMD_AUTHENTICATE_LEGACY / _ISO / _AES
    uint8_t session_key[DFC_MAX_KEY_LEN];
    uint8_t session_key_len;
    // Shared CBC IV, chained across every enciphered-mode operation regardless of
    // direction, for ISO(0x1A)/AES(0xAA) sessions. The IV for each new CBC operation is
    // the last ciphertext block of the preceding operation. Legacy (0x0A) sessions use
    // an all-zero IV for each operation.
    uint8_t iv[16];

    // True when this session belongs to the reader. A legacy (D40) session's
    // enciphered mode picks its DES primitive by party rather than by direction,
    // so the two ends are not interchangeable there. Unset means PICC.
    bool pcd;

} DfcSecureMessaging;

// Initializes caller-owned storage. Authentication uses this to stage a complete
// replacement before committing it to an existing, pointer-stable session.
bool dfc_secure_messaging_init(
    DfcSecureMessaging* sm,
    uint8_t cipher,
    const uint8_t* session_key,
    size_t session_key_len,
    const uint8_t* initial_iv);

// `initial_iv` seeds the chained IV for ISO/AES sessions (pass the final chained IV state
// from the authentication handshake); ignored for legacy sessions. Pass NULL to start from
// all-zero (e.g. legacy, or tests that don't care about IV chaining).
DfcSecureMessaging* dfc_secure_messaging_alloc(
    uint8_t cipher,
    const uint8_t* session_key,
    size_t session_key_len,
    const uint8_t* initial_iv);

void dfc_secure_messaging_free(DfcSecureMessaging* sm);
void dfc_secure_messaging_reset_iv(DfcSecureMessaging* sm);
bool dfc_secure_messaging_applies_ev1(DfcSecureMessaging* sm, uint8_t cmd);
// Returns false without advancing the IV when CMAC generation fails.
bool dfc_secure_messaging_update_ev1_command(
    DfcSecureMessaging* sm,
    uint8_t cmd,
    const uint8_t* data,
    size_t data_len);
// Accepts a contiguous command directly, including the configured maximum
// command size, without allocating a second message-sized buffer.
bool dfc_secure_messaging_update_ev1_command_full(
    DfcSecureMessaging* sm,
    const uint8_t* command,
    size_t command_len);
// EV1 commands that carry MACt on the wire: verify the trailing 8-byte
// truncated CMAC over Cmd||data_without_mac and update the IV from the full
// CMAC. Returns clear length (data_len - 8) or SIZE_MAX on failure.
size_t dfc_secure_messaging_verify_ev1_transmitted_command_mac(
    DfcSecureMessaging* sm,
    uint8_t cmd,
    const uint8_t* data,
    size_t data_len);
// As above, with Cmd included in `command`; avoids a copy for long writes.
size_t dfc_secure_messaging_verify_ev1_transmitted_command_mac_full(
    DfcSecureMessaging* sm,
    const uint8_t* command,
    size_t command_len);
// True for commands whose EV1 form places MACt on the wire.
bool dfc_secure_messaging_ev1_transmits_command_mac(uint8_t cmd);

// Computes the transmitted payload length for a command before it is encoded.
// `sm` identifies legacy or EV1 protection; pass NULL only for EV2. Returns
// SIZE_MAX for invalid modes or arithmetic overflow.
static inline size_t dfc_secure_messaging_command_payload_length(
    const DfcSecureMessaging* sm,
    bool ev2,
    uint8_t comm_mode,
    size_t clear_len) {
    if((comm_mode != DFC_COMM_PLAIN && comm_mode != DFC_COMM_MAC &&
        comm_mode != DFC_COMM_ENCIPHERED) ||
       (ev2 && sm)) {
        return SIZE_MAX;
    }
    if(comm_mode == DFC_COMM_PLAIN) return clear_len;
    if(!ev2 && !sm) return SIZE_MAX;

    size_t overhead;
    size_t block = 1;
    if(comm_mode == DFC_COMM_MAC) {
        overhead = ev2 || sm->cipher != DFC_CMD_AUTHENTICATE_LEGACY ? DFC_WIRE_MAC_LENGTH :
                                                                      DFC_SM_LEGACY_MAC_LENGTH;
    } else if(ev2) {
        overhead = 1;
        block = DFC_AES_KEY_LENGTH;
    } else {
        overhead = sm->cipher == DFC_CMD_AUTHENTICATE_LEGACY ? 2 : 4;
        block = sm->cipher == DFC_CMD_AUTHENTICATE_AES ? DFC_AES_KEY_LENGTH :
                                                         DFC_SM_LEGACY_BLOCK_SIZE;
    }
    size_t trailer = ev2 && comm_mode == DFC_COMM_ENCIPHERED ? DFC_WIRE_MAC_LENGTH : 0;
    size_t maximum_extra = overhead + block - 1 + trailer;
    if(clear_len > SIZE_MAX - maximum_extra) return SIZE_MAX;
    return ((clear_len + overhead + block - 1) / block) * block + trailer;
}
bool dfc_secure_messaging_generate_ev1_response_mac(
    DfcSecureMessaging* sm,
    uint8_t status,
    const uint8_t* plain,
    size_t plain_len,
    uint8_t mac[DFC_WIRE_MAC_LENGTH]);
size_t dfc_secure_messaging_generate_ev1_response(
    DfcSecureMessaging* sm,
    uint8_t status,
    const uint8_t* plain,
    size_t plain_len,
    uint8_t* out,
    size_t out_capacity);
// Appends the response CMAC directly to `buffer`, which must have eight spare
// octets. Both response
// helpers return zero without changing the IV or output when CMAC fails.
size_t dfc_secure_messaging_generate_ev1_response_in_place(
    DfcSecureMessaging* sm,
    uint8_t status,
    uint8_t* buffer,
    size_t plain_len,
    size_t capacity);
// Returns the cleartext length, or SIZE_MAX on CMAC/format failure.
size_t dfc_secure_messaging_unwrap_ev1_response(
    DfcSecureMessaging* sm,
    uint8_t status,
    const uint8_t* wrapped,
    size_t wrapped_len,
    uint8_t* out,
    size_t out_capacity);

// D40 uses a four-byte CBC-MAC over payload bytes only. These named entry
// points keep that rule separate from EV1 chained CMAC and EV2 MACs.
size_t dfc_secure_messaging_wrap_d40_command_mac(
    DfcSecureMessaging* sm,
    const uint8_t* plain,
    size_t plain_len,
    uint8_t* out,
    size_t out_capacity);
bool dfc_secure_messaging_verify_d40_command_mac(
    DfcSecureMessaging* sm,
    const uint8_t* wrapped,
    size_t wrapped_len,
    uint8_t* out,
    size_t out_capacity,
    size_t* out_len);
size_t dfc_secure_messaging_generate_d40_response_mac(
    DfcSecureMessaging* sm,
    const uint8_t* plain,
    size_t plain_len,
    uint8_t* out,
    size_t out_capacity);
bool dfc_secure_messaging_unwrap_d40_response_mac(
    DfcSecureMessaging* sm,
    const uint8_t* wrapped,
    size_t wrapped_len,
    uint8_t* out,
    size_t out_capacity,
    size_t* out_len);

// Wraps a command payload in plain or enciphered communication mode. MAC mode
// is family-specific and is rejected here. `header` is the cleartext command
// head, such as Cmd||FileNo||Offset||Length, and `plain` is its payload. CRC
// covers the command head in EV1 and only the payload in D40. Writes only the
// wrapped payload to `out`. Returns zero without changing the IV or output when the
// requested cryptographic operation fails. Enciphered-mode output is wiped if
// a cipher fails after writing has begun.
size_t dfc_secure_messaging_wrap(
    DfcSecureMessaging* sm,
    uint8_t comm_mode,
    const uint8_t* header,
    size_t header_len,
    const uint8_t* plain,
    size_t plain_len,
    uint8_t* out,
    size_t out_capacity);

// Unwraps a plain or enciphered response payload; MAC mode is rejected. The
// status byte is not included in `wrapped`. Returns true on format success and
// writes the cleartext length to `out_len`, including zero for an authenticated
// empty payload. Success advances the IV to the accepted message's chaining
// value. Failure returns false, sets `out_len` to zero, and leaves the IV
// unchanged. Callers must ignore `out` after failure.
bool dfc_secure_messaging_unwrap(
    DfcSecureMessaging* sm,
    uint8_t comm_mode,
    uint8_t status,
    const uint8_t* wrapped,
    size_t wrapped_len,
    uint8_t* out,
    size_t out_capacity,
    size_t* out_len);

// Emulator side: generates a plain or enciphered response payload.
size_t dfc_secure_messaging_generate_response(
    DfcSecureMessaging* sm,
    uint8_t comm_mode,
    uint8_t status,
    const uint8_t* plain,
    size_t plain_len,
    uint8_t* out,
    size_t out_capacity);
size_t dfc_secure_messaging_generate_response_with_length_marker(
    DfcSecureMessaging* sm,
    uint8_t comm_mode,
    uint8_t status,
    const uint8_t* plain,
    size_t plain_len,
    uint8_t* out,
    size_t out_capacity,
    bool length_unknown);

// Emulator side: unwraps a plain or enciphered command payload.
size_t dfc_secure_messaging_verify_command(
    DfcSecureMessaging* sm,
    uint8_t comm_mode,
    const uint8_t* header,
    size_t header_len,
    const uint8_t* wrapped,
    size_t wrapped_len,
    uint8_t* out,
    size_t out_capacity);
