#pragma once

#include "mfp_crypto.h"
#include <stdint.h>
#include <stdbool.h>

/* ---- Card geometry ---- */
#define MFP_BLOCK_SIZE     16
#define MFP_MAX_BLOCKS     256
/* MIFARE Plus sector counts per NXP MF1PLUSx0y1 spec:
 *
 * "2K" card (MF1PLUS80):
 *   16 data sectors × 4 blocks × 16 bytes = 1024 bytes user data
 *   Plus 16 × 2 × 16 = 512 bytes of sector keys (stored in trailer blocks)
 *   NXP markets as "2K" counting keys + user memory.
 *
 * "4K" card (MF1PLUS60):
 *   32 small sectors × 4 blocks + 8 large sectors × 16 blocks = 40 sectors
 *   32×4 + 8×16 = 256 blocks × 16 = 4096 bytes user data
 *
 * Enum values match NXP's marketing names, not raw user-data size.
 */
#define MFP_SECTORS_2K     16
#define MFP_SECTORS_4K     40

typedef enum {
    MfpSL1 = 1,
    MfpSL2 = 2,
    MfpSL3 = 3,
} MfpSecurityLevel;

typedef enum {
    MfpSize2K = 0, /* MF1PLUS80, 16 sectors, 1024 B user data */
    MfpSize4K = 1, /* MF1PLUS60, 40 sectors, 4096 B user data */
} MfpCardSize;

typedef enum {
    MfpKeyA = 0,
    MfpKeyB = 1,
} MfpKeyType;

typedef enum {
    MfpOk = 0,
    MfpErrorProtocol,
    MfpErrorAuth,
    MfpErrorNotMfp,
    MfpErrorComm,
    MfpErrorMac,
} MfpError;

typedef uint8_t MfpKey[MFP_AES_KEY_SIZE];
typedef uint8_t MfpBlock[MFP_BLOCK_SIZE];

typedef struct {
    uint8_t hw_vendor;
    uint8_t hw_type;
    uint8_t hw_subtype;
    uint8_t hw_major;
    uint8_t hw_minor;
    uint8_t hw_storage;
    uint8_t hw_proto;
    uint8_t sw_vendor;
    uint8_t sw_type;
    uint8_t sw_subtype;
    uint8_t sw_major;
    uint8_t sw_minor;
    uint8_t sw_storage;
    uint8_t sw_proto;
    uint8_t uid[7];
    uint8_t uid_len;
    MfpSecurityLevel sl;
    MfpCardSize size;
} MfpVersion;

/** Active session state after successful authentication. */
typedef struct {
    uint8_t k_enc[MFP_AES_KEY_SIZE];
    uint8_t k_mac[MFP_AES_KEY_SIZE];
    uint8_t ti[4];
    uint16_t r_ctr;
    uint16_t w_ctr;
} MfpSession;

/**
 * The poller sends APDUs over the Iso14443_4aPoller it receives
 * in the NfcGenericEvent callback.  Call these from inside
 * the NfcProtocolIso14443_4a poller callback.
 */

/** Read GetVersion and populate MfpVersion.
 *  out_iso_err  (optional): raw Iso14443_4aError from the first send_block call.
 *  out_raw_resp (optional, 32 bytes): copy of the first response frame.
 *  out_raw_len  (optional): length of that frame. */
MfpError mfp_poller_read_version(
    void* iso4a_poller,
    MfpVersion* out,
    int* out_iso_err,
    uint8_t* out_raw_resp,
    uint8_t* out_raw_len);

/** Two-phase SL3 first authentication (command 0x70).
 *  Use for the first authentication in a session.
 *  sector: 0-based sector number
 *  key_type: MfpKeyA or MfpKeyB
 *  key: 16-byte AES key
 *  session: filled on success (TI is NEW, counters reset)
 */
MfpError mfp_poller_auth(
    void* iso4a_poller,
    uint8_t sector,
    MfpKeyType key_type,
    const MfpKey key,
    MfpSession* session);

/** Two-phase SL3 follow-on authentication (command 0x76).
 *  Use to switch to a different sector within an existing session.
 *  TI is PRESERVED, counters reset, new session keys derived.
 *  Requires a previous successful mfp_poller_auth() call.
 */
MfpError mfp_poller_auth_nonfirst(
    void* iso4a_poller,
    uint8_t sector,
    MfpKeyType key_type,
    const MfpKey key,
    MfpSession* session);

/** Read one encrypted block. Must be called after mfp_poller_auth(). */
MfpError mfp_poller_read_block(
    void* iso4a_poller,
    uint8_t block_num,
    MfpSession* session,
    MfpBlock out_block);

/** Write one block (encrypted + MAC). Must be called after mfp_poller_auth().
 *  data: 16 bytes of plaintext to write. */
MfpError mfp_poller_write_block(
    void* iso4a_poller,
    uint8_t block_num,
    MfpSession* session,
    const uint8_t data[MFP_BLOCK_SIZE]);
