/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Filip Sedivy
 */

#include "emrtd_transceiver.h"

#include "../crypto/emrtd_crypto.h"
#include "../protocol/emrtd_apdu.h"

/*
 * ISO/IEC 14443-4 counts a whole frame in FSC and FSD: the prologue, the
 * information field and the epilogue. What is left for an APDU is therefore
 * the announced frame size less those two.
 *
 * The prologue is one PCB byte plus an optional CID byte, and the epilogue is
 * the two byte CRC. The CID is counted here whether or not it is actually
 * sent, because one byte of slack costs nothing once the result is rounded
 * down to a cipher block, and it keeps the reader correct against a card that
 * insists on being addressed.
 */
#define EMRTD_FRAME_PROLOGUE 2
#define EMRTD_FRAME_EPILOGUE 2
#define EMRTD_FRAME_OVERHEAD (EMRTD_FRAME_PROLOGUE + EMRTD_FRAME_EPILOGUE)

/** Frame size a card is assumed to accept until its ATS says otherwise (ISO 14443-4, 5.1). */
#define EMRTD_FRAME_DEFAULT 32

/*
 * What the Secure Messaging envelope costs in a response, in the worst case:
 * the status word, DO'8E' with its eight byte checksum, DO'99', and the header
 * of DO'87' - its tag, a two byte length and the padding indicator.
 */
#define EMRTD_SM_RESPONSE_OVERHEAD (2 + (2 + EMRTD_MAC_SIZE) + 4 + 4)

/** A plain response only carries the status word. */
#define EMRTD_PLAIN_RESPONSE_OVERHEAD 2

/*
 * A short command APDU spends four bytes on the header, one on Lc and one on
 * Le, which is what the data field has to share the frame with.
 */
#define EMRTD_COMMAND_OVERHEAD 6

/** Bytes of a frame left for the APDU, or zero when the frame is too small to hold one. */
static size_t emrtd_frame_payload(uint16_t frame_size) {
    const size_t size = frame_size != 0 ? (size_t)frame_size : EMRTD_FRAME_DEFAULT;
    if(size <= EMRTD_FRAME_OVERHEAD) {
        return 0;
    }
    return size - EMRTD_FRAME_OVERHEAD;
}

EmrtdError emrtd_transceiver_exchange(
    EmrtdTransceiver* transceiver,
    const uint8_t* tx,
    size_t tx_len,
    uint8_t* rx,
    size_t rx_cap,
    size_t* rx_len) {
    if(transceiver == NULL || transceiver->api == NULL || transceiver->api->transceive == NULL ||
       tx == NULL || rx == NULL || rx_len == NULL) {
        return EmrtdErrorInternal;
    }
    /* Every APDU has at least CLA, INS, P1 and P2; anything shorter is a bug here. */
    if(tx_len < 4) {
        return EmrtdErrorInternal;
    }
    if(rx_cap < EMRTD_PLAIN_RESPONSE_OVERHEAD) {
        return EmrtdErrorBufferTooSmall;
    }

    *rx_len = 0;
    const EmrtdError error =
        transceiver->api->transceive(transceiver->ctx, tx, tx_len, rx, rx_cap, rx_len);
    if(error != EmrtdErrorNone) {
        return error;
    }

    /*
     * A response without a status word is not a response. Catching it here
     * means every layer above can rely on the two trailing bytes, and it also
     * contains an implementation that reports more bytes than it was given
     * room for.
     */
    if(*rx_len < EMRTD_PLAIN_RESPONSE_OVERHEAD || *rx_len > rx_cap) {
        *rx_len = 0;
        return EmrtdErrorProtocol;
    }
    return EmrtdErrorNone;
}

size_t emrtd_transceiver_max_le(const EmrtdTransceiver* transceiver, size_t block_size) {
    if(transceiver == NULL) {
        return 0;
    }

    /*
     * The response travels from the card to the reader, so it is the reader's
     * own frame size that bounds it. The Flipper announces FSD = 256 in RATS.
     * On type B the firmware's ISO 14443-4 layer does not reassemble a chained
     * response, so this has to be respected rather than merely preferred; on
     * type A the reader follows the chain itself, and the bound is kept anyway
     * because a shorter exchange is a more robust one. See docs/platform.md.
     */
    size_t budget = emrtd_frame_payload(transceiver->fsd);

    if(block_size == 0) {
        /* No session yet, so the answer arrives in the clear. */
        if(budget <= EMRTD_PLAIN_RESPONSE_OVERHEAD) {
            return 0;
        }
        budget -= EMRTD_PLAIN_RESPONSE_OVERHEAD;
        return budget < EMRTD_LE_MAX ? budget : EMRTD_LE_MAX;
    }

    if(budget <= EMRTD_SM_RESPONSE_OVERHEAD) {
        return 0;
    }
    budget -= EMRTD_SM_RESPONSE_OVERHEAD;

    /*
     * ISO 9797-1 method 2 padding is always added, so a plaintext that is
     * already a whole number of blocks still grows by one. Reserving a block
     * and then rounding down leaves the ciphertext fitting exactly.
     */
    if(budget <= block_size) {
        return 0;
    }
    const size_t le = ((budget - block_size) / block_size) * block_size;
    return le < EMRTD_LE_MAX ? le : EMRTD_LE_MAX;
}

size_t emrtd_transceiver_max_lc(const EmrtdTransceiver* transceiver) {
    if(transceiver == NULL) {
        return 0;
    }

    /* The command travels the other way, so the card's frame size bounds it. */
    const size_t payload = emrtd_frame_payload(transceiver->fsc);
    if(payload <= EMRTD_COMMAND_OVERHEAD) {
        return 0;
    }
    size_t lc = payload - EMRTD_COMMAND_OVERHEAD;

    /* Lc is one byte in a short APDU, and no command here needs an extended one. */
    if(lc > 255) {
        lc = 255;
    }
    if(lc > EMRTD_APDU_MAX_SIZE - EMRTD_COMMAND_OVERHEAD) {
        lc = EMRTD_APDU_MAX_SIZE - EMRTD_COMMAND_OVERHEAD;
    }
    return lc;
}

uint16_t emrtd_transceiver_fsc_from_fsci(uint8_t fsci) {
    /*
     * ISO/IEC 14443-4 table 3. Codes 0 to 8 are the original set; 9 to 12 were
     * added for the larger frames of ISO/IEC 14443-4:2008 onwards.
     */
    static const uint16_t sizes[] = {
        16,
        24,
        32,
        40,
        48,
        64,
        96,
        128,
        256,
        512,
        1024,
        2048,
        4096,
    };

    const uint8_t index = fsci & 0x0F;
    if(index >= sizeof(sizes) / sizeof(sizes[0])) {
        /*
         * Codes 13 to 15 are reserved. A card using one tells us nothing, so
         * fall back on the largest frame this reader can handle anyway; the
         * result is clamped to EMRTD_APDU_MAX_SIZE further up.
         */
        return EMRTD_APDU_MAX_SIZE;
    }
    return sizes[index];
}
