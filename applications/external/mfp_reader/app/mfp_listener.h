#pragma once

#include "mfp_poller.h"
#include "mfp_crypto.h"

#include <nfc/nfc.h>
#include <nfc/nfc_listener.h>
#include <nfc/protocols/iso14443_4a/iso14443_4a.h>

/* Forward declaration — full definition in mfp_app.h */
typedef struct MfpApp MfpApp;

/* MIFARE Plus SL3 card emulator.
 * Uses Iso14443_4aListener for ISO framing and implements MFP SL3 commands
 * (GetVersion, AuthFirstPart1/Part2, ReadEncrypted, WriteEncrypted) in the
 * event callback. Responses are sent as raw I-blocks via nfc_listener_tx(). */

typedef struct {
    uint8_t key_a[MFP_AES_KEY_SIZE];
    uint8_t key_b[MFP_AES_KEY_SIZE];
    bool    key_a_valid;
    bool    key_b_valid;
} MfpListenerSectorKeys;

typedef struct MfpListener {
    Nfc* nfc;
    NfcListener* listener;
    Iso14443_4aData* iso_data;

    /* Card data */
    uint8_t blocks[MFP_MAX_BLOCKS][MFP_BLOCK_SIZE];
    MfpListenerSectorKeys sector_keys[MFP_SECTORS_4K];
    MfpCardSize size;
    uint8_t  total_sectors;
    uint8_t  uid[7];
    uint8_t  uid_len;
    uint8_t  sak;
    uint8_t  atqa[2];

    /* Auth session state */
    bool     authed;
    uint8_t  auth_sector;
    MfpKeyType auth_key_type;
    uint8_t  rnd_b[MFP_AES_BLOCK_SIZE];
    uint8_t  rnd_a[MFP_AES_BLOCK_SIZE];
    MfpSession session;
    bool     part1_done;
    bool     part1_is_nonfirst;  /* true if pending Part 1 was AuthNonFirst (0x76) */
    uint8_t  saved_ti[4];        /* TI preserved across AuthNonFirst */
    bool     has_saved_ti;

    /* PCB tracking (we can't access the listener's internal state) */
    uint8_t  current_pcb;

    /* Stats / activity tracking */
    uint32_t auths_count;       /* successful Part2 completions */
    uint32_t reads_count;
    uint32_t writes_count;
    uint32_t nak_count;         /* failed commands (auth/mac/permission) */
    uint8_t  last_op_type;      /* 'A' | 'R' | 'W' | 'N' (nak) | 0 */
    uint8_t  last_op_block;     /* block number (read/write) or sector (auth) */
    uint8_t  last_op_sector;    /* sector containing last_op_block */
    uint8_t  last_nak_code;     /* last NAK status byte (0x07/0x08/0x0C etc.) */

    /* Callback for scene to update UI */
    void (*on_activity)(void* ctx);
    void* activity_ctx;
} MfpListener;

/** Allocate and configure the emulator. */
MfpListener* mfp_listener_alloc(Nfc* nfc);

void mfp_listener_free(MfpListener* instance);

/** Populate card data from the just-scanned card in MfpApp. */
void mfp_listener_set_from_app(MfpListener* instance, const MfpApp* app);

/** Start emulation. */
void mfp_listener_start(MfpListener* instance);

/** Stop emulation. */
void mfp_listener_stop(MfpListener* instance);
