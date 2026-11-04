/**
 * @file sniffer_card_poller.h
 * @brief One session over the radio: read a card's identity, read a sniffer card's trace slots,
 *        or program a sniffer card's UID.
 */
#pragma once

#include <nfc/nfc.h>
#include <nfc/protocols/iso14443_3a/iso14443_3a.h>
#include <nfc/protocols/mf_classic/mf_classic.h>

#include "sniffer_card.h"

/** What a session is for. */
typedef enum {
    /** Activate whatever is on the Flipper and keep its UID and SAK. Used on the card the keys
     * are wanted for, and to check what a sniffer card currently answers as. */
    SnifferCardPollerModeReadIdentity,
    /** Read a Gen2 sniffer card's trace slots. */
    SnifferCardPollerModeReadTraces,
    /** Program a Gen2 sniffer card's UID, then re-activate it and check what it answers as. */
    SnifferCardPollerModeWriteUid,
} SnifferCardPollerMode;

/** How the session ended. */
typedef enum {
    SnifferCardPollerResultWaiting, /**< Nothing on the Flipper yet. */
    SnifferCardPollerResultSuccess,
    /** The tag presents no MIFARE Classic personality at all, so it cannot be one of these
     * cards whatever else it is. An identity read never produces this: it reports whatever
     * answered and lets the confirm screen judge the type. */
    SnifferCardPollerResultNotClassic,
    /** A Classic-looking tag that refuses an unauthenticated read, i.e. an ordinary Classic
     * rather than a sniffer card. */
    SnifferCardPollerResultNotSniffer,
    /** The sniffer card's UID is a different length from the one asked for. These cards are
     * physically a 4-byte or a 7-byte part and cannot be made into the other. */
    SnifferCardPollerResultUidLenMismatch,
    /** The UID was written and the card came back answering as something else. */
    SnifferCardPollerResultWriteMismatch,
    /** The UID was written and the card did not come back at all, so nothing confirms it. */
    SnifferCardPollerResultWriteUnverified,
    SnifferCardPollerResultError, /**< The tag went away mid-session, or answered nonsense. */

    /** Not a verdict: the count, so a table of outcomes can be checked against it at compile
     * time rather than by whoever next remembers to look. */
    SnifferCardPollerResultNum,
} SnifferCardPollerResult;

/** What a tag answered during activation. */
typedef struct {
    uint8_t uid[ISO14443_3A_MAX_UID_SIZE];
    size_t uid_len;
    uint8_t sak;
} SnifferCardIdentity;

/** The MIFARE Classic size a SAK declares.
 *
 * Only the SAK half of the firmware's own detection is mirrored: an activation yields no ATS and
 * no authenticated probe, so the MIFARE Plus 2K promotion and the legacy block-presence fallback
 * it also has cannot be reproduced here, and 2K is therefore never returned.
 *
 * AN10833 encodes the size as bits rather than whole values, which is what the firmware's own
 * `mf_classic_poller_handler_detect_type()` tests - so a magic card that answers every block is
 * not mis-sized against its own SAK. Mini shares the 1K bit and has to be matched first.
 *
 * @return MfClassicTypeNum when the SAK is not a Classic at all
 */
MfClassicType sniffer_card_identity_get_type(const SnifferCardIdentity* identity);

/** The card size as a label, for a screen that has to name it.
 *
 * Its own function because the SDK exports no name for a bare MfClassicType -
 * `mf_classic_get_device_name()` wants a whole MfClassicData, which an activation alone does not
 * produce.
 */
const char* sniffer_card_identity_get_type_name(const SnifferCardIdentity* identity);

typedef struct SnifferCardPoller SnifferCardPoller;

/** Called from the NFC worker thread once the session has reached a verdict.
 *
 * Runs on that thread, not the GUI's, so a scene's handler must do nothing but post a custom
 * event and read the result back on the other side.
 */
typedef void (*SnifferCardPollerCallback)(void* context);

SnifferCardPoller* sniffer_card_poller_alloc(Nfc* nfc);

void sniffer_card_poller_free(SnifferCardPoller* instance);

/** The UID a SnifferCardPollerModeWriteUid session should program. Set it before starting. */
void sniffer_card_poller_set_uid(SnifferCardPoller* instance, const uint8_t* uid, size_t uid_len);

/** Begin looking for a card. Keeps looking until a tag answers, or until stopped. */
void sniffer_card_poller_start(
    SnifferCardPoller* instance,
    SnifferCardPollerMode mode,
    SnifferCardPollerCallback callback,
    void* context);

void sniffer_card_poller_stop(SnifferCardPoller* instance);

SnifferCardPollerResult sniffer_card_poller_get_result(const SnifferCardPoller* instance);

/** What the tag answered as. After a UID write this is what it answered as afterwards. */
const SnifferCardIdentity* sniffer_card_poller_get_identity(const SnifferCardPoller* instance);

/** The captures the session read, valid until the next start.
 *
 * @param[out] traces where the array lives; untouched when this returns zero
 * @return how many slots held a real capture
 */
size_t
    sniffer_card_poller_get_traces(const SnifferCardPoller* instance, const SnifferTrace** traces);

/** Whether the read stopped before the card ran out of slots.
 *
 * A read that broke off part way has left captures on the card unread, and the card keeps them
 * only until a reader overwrites them - so the difference between this and an empty card is the
 * difference between "read it again" and advice that destroys what is there.
 */
bool sniffer_card_poller_read_was_truncated(const SnifferCardPoller* instance);
