#include "sniffer_card_poller.h"

#include <nfc/protocols/iso14443_3a/iso14443_3a_poller.h>

#define TAG "SnifferCard"

/** Ultralight-style READ, which is how a Gen2 card hands over its trace slots.
 *
 * The card emulates MIFARE Classic to the reader it is sniffing, but answers the Flipper on a
 * second, undocumented personality: a plain `30 <page>` with no authentication, which a genuine
 * Classic refuses outright. That refusal is what identifies the card.
 */
#define SNIFFER_CARD_CMD_READ (0x30)

/** The five commands that arm a UID write, in order, from the vendor app's `SetUID` path
 * (`com.fnstar.sniffercard.NFCReadActivity`).
 *
 * They are READ commands by opcode, and the pages they name mean nothing as pages - the sequence
 * itself is the knock. Proxmark3 has no equivalent: `hf mf supercard` answers "Not supported on
 * this card" for Gen2, so this is the one thing the vendor app knows that it does not.
 */
static const uint8_t sniffer_card_uid_knock[] = {0xF3, 0x2A, 0xBD, 0xEF, 0x01};

/** UID bytes the card expects, whatever its own UID length.
 *
 * Always seven, zero-padding a 4-byte UID, which makes the last three commands `30 00` - a read
 * of page 0 by opcode. Confirmed on the wire: a sniff of the vendor app writing a 4-byte UID
 * sends `30 5D`, `30 0D`, `30 19`, `30 65` and then `30 00` three times.
 */
#define SNIFFER_CARD_UID_WRITE_BYTES 7

/** HLTA, which is how the card is told to keep the UID it was just given.
 *
 * Not a guess and not an ordinary halt. A sniff of the vendor app writing a UID shows it sending
 * HLTA straight after the seventh byte, and the card *answering* it - which an ISO14443-3A tag
 * never does to a halt. The answer is the same 16-byte frame every command in this mode returns,
 * except that its leading status byte goes from `11` to `22`. The next WUPA then gets nothing at
 * all while the card writes, and the one after that comes back under the new UID.
 *
 * This is the step that was missing. Without it the knock and all seven bytes are accepted and
 * acknowledged, and then dropping the field discards the lot - which is why twelve commands could
 * succeed and change nothing. Android sends it implicitly when the app closes the tag connection.
 */
#define SNIFFER_CARD_CMD_HALT_0 (0x50)
#define SNIFFER_CARD_CMD_HALT_1 (0x00)

/** Status byte the card answers the committing HLTA with. */
#define SNIFFER_CARD_STATUS_COMMITTED (0x22)

/** Pause between commands of the write sequence.
 *
 * The vendor app leaves about 9 ms between frames, which is JVM overhead rather than intent - a
 * run with no gap at all was acknowledged just the same. Kept small and deliberate rather than
 * sending as fast as the field allows, because nothing here is time-critical and the card is
 * writing memory at the end of it.
 */
#define SNIFFER_CARD_WRITE_GAP_MS (10)

/** Pause before the committing HLTA, and after it while the card writes.
 *
 * The sniff shows about 136 ms of quiet before the vendor app's HLTA, and the WUPA straight after
 * it going unanswered.
 */
#define SNIFFER_CARD_COMMIT_MS (150)

/** Activations to spend waiting for the card to come back after the commit.
 *
 * One is not enough: the card is writing its memory and answers nothing while it does. Each
 * attempt costs a field cycle, so a handful is a second at most, against the cost of telling
 * someone a write failed when it worked.
 */
#define SNIFFER_CARD_VERIFY_ATTEMPTS (4)

#define SNIFFER_CARD_POLLER_MAX_BUFFER_SIZE (64U)
#define SNIFFER_CARD_POLLER_MAX_FWT         (150000U)

/** SAK of a tag with no MIFARE Classic personality at all.
 *
 * A plain Ultralight or NTAG answers `30 00` with sixteen bytes exactly as these cards do, so the
 * read alone cannot tell them apart. Nothing that sniffs Classic authentications presents this
 * SAK, so it is the cheap half of the check and it runs first.
 */
#define SNIFFER_CARD_SAK_NOT_CLASSIC (0x00)

/** Where a UID-write session has got to.
 *
 * The write and the read-back are two activations of the same card: a card that has just been
 * given a new UID is still selected under the old one, so it has to be selected again before it
 * will say what it has become.
 *
 * Getting there is a field cycle, not a HLTA. Halting the card would leave it answering only
 * WUPA, and the firmware activates with REQA alone
 * (`iso14443_3a_poller_activate()` -> `NfcIso14443aShortFrameSensReq`), so a halted card would
 * never come back and every write would read as unconfirmed. Returning NfcCommandReset instead
 * takes the field down for 100 ms and brings it up
 * (`nfc_worker_poller_reset_handler()` in `lib/nfc/nfc.c`), which powers the card out of any
 * state it was left in.
 */
typedef enum {
    SnifferCardWriteStepWrite,
    SnifferCardWriteStepVerify,
} SnifferCardWriteStep;

struct SnifferCardPoller {
    Nfc* nfc;
    /** Allocated per session and freed with it, because an NfcPoller is single-use.
     *
     * `nfc_poller_alloc()` is what calls `nfc_config()` on the way down
     * (`iso14443_3a_poller_alloc()` in `lib/nfc/protocols/iso14443_3a/iso14443_3a_poller.c`), and
     * a session that ends puts the configuration state back to Idle
     * (`nfc_worker_poller_stop_handler()`). `nfc_start()` hard-checks that state, so starting a
     * second session on one poller is a furi_check failure, not a quiet misbehaviour - it takes
     * the whole app down.
     *
     * NULL between sessions, which is also what makes stopping twice harmless: every scene stops
     * in its on_exit whether or not its session is still running.
     */
    NfcPoller* poller;
    BitBuffer* tx_buffer;
    BitBuffer* rx_buffer;

    SnifferCardPollerMode mode;
    SnifferCardPollerCallback callback;
    void* context;

    uint8_t uid_to_write[SNIFFER_CARD_UID_WRITE_BYTES];
    size_t uid_to_write_len;
    SnifferCardWriteStep write_step;

    SnifferCardPollerResult result;
    SnifferCardIdentity identity;
    SnifferTrace traces[SNIFFER_CARD_TRACE_SLOTS];
    size_t trace_count;
    bool read_truncated;
    uint8_t verify_attempts;
};

MfClassicType sniffer_card_identity_get_type(const SnifferCardIdentity* identity) {
    furi_assert(identity);

    if(identity->sak == 0x09) return MfClassicTypeMini; // shares the 1K bit, so test it first
    if(identity->sak & 0x10) return MfClassicType4k; // 0x18, and 0x38 for SmartMX with Classic
    if(identity->sak & 0x08) return MfClassicType1k; // 0x08, and 0x28 for SmartMX with Classic

    return MfClassicTypeNum;
}

const char* sniffer_card_identity_get_type_name(const SnifferCardIdentity* identity) {
    switch(sniffer_card_identity_get_type(identity)) {
    case MfClassicTypeMini:
        return "Mini";
    case MfClassicType1k:
        return "1K";
    case MfClassicType4k:
        return "4K";
    default:
        return "not Classic";
    }
}

SnifferCardPoller* sniffer_card_poller_alloc(Nfc* nfc) {
    furi_assert(nfc);

    SnifferCardPoller* instance = malloc(sizeof(SnifferCardPoller));
    instance->nfc = nfc;
    instance->poller = NULL;
    instance->tx_buffer = bit_buffer_alloc(SNIFFER_CARD_POLLER_MAX_BUFFER_SIZE);
    instance->rx_buffer = bit_buffer_alloc(SNIFFER_CARD_POLLER_MAX_BUFFER_SIZE);
    instance->mode = SnifferCardPollerModeReadIdentity;
    instance->callback = NULL;
    instance->context = NULL;
    instance->uid_to_write_len = 0;
    instance->write_step = SnifferCardWriteStepWrite;
    instance->result = SnifferCardPollerResultWaiting;
    memset(&instance->identity, 0, sizeof(instance->identity));
    instance->trace_count = 0;
    instance->read_truncated = false;
    instance->verify_attempts = 0;

    return instance;
}

void sniffer_card_poller_free(SnifferCardPoller* instance) {
    furi_assert(instance);

    // Normally already stopped by the scene's on_exit, which Back always runs. Guarded rather
    // than trusted, because leaving a session running past its owner is how the radio stays on.
    sniffer_card_poller_stop(instance);
    bit_buffer_free(instance->tx_buffer);
    bit_buffer_free(instance->rx_buffer);
    free(instance);
}

void sniffer_card_poller_set_uid(SnifferCardPoller* instance, const uint8_t* uid, size_t uid_len) {
    furi_assert(instance);
    furi_assert(uid);
    furi_check(uid_len <= sizeof(instance->uid_to_write));

    // Zero-padded to the full seven the knock sends, so the write path never reads past what was
    // given to it.
    memset(instance->uid_to_write, 0, sizeof(instance->uid_to_write));
    memcpy(instance->uid_to_write, uid, uid_len);
    instance->uid_to_write_len = uid_len;
}

/** Hex for the log, where what a card actually answered is the whole question.
 *
 * Built onto the stack and gated on the level, because FURI_LOG_D is a plain call to
 * furi_log_print_format() and the level test lives inside it - so every argument is evaluated
 * whether or not anything will print. This runs between the frames of a sequence that ends with
 * the card committing to flash, on the NFC worker thread with the field up, so at the default
 * level it was a dozen heap allocations bought to be thrown away.
 */
static void sniffer_card_log_bytes(const char* label, const uint8_t* data, size_t len) {
    if(furi_log_get_level() < FuriLogLevelDebug) return;

    static const char digits[] = "0123456789ABCDEF";
    char hex[SNIFFER_CARD_TRACE_SIZE * 2 + 1];

    furi_check(len <= SNIFFER_CARD_TRACE_SIZE);
    for(size_t i = 0; i < len; i++) {
        hex[i * 2] = digits[data[i] >> 4];
        hex[i * 2 + 1] = digits[data[i] & 0x0F];
    }
    hex[len * 2] = '\0';

    FURI_LOG_D(TAG, "%s %s", label, hex);
}

/** Send one `30 <arg>` and keep the answer.
 *
 * Logged command and answer both: these cards are documented by two sources that disagree, and
 * the only way to tell which sequence a particular one speaks is to see what it says back.
 *
 * @param allow_wrong_crc accept a frame whose CRC did not check out. For the write sequence,
 *        where the answers are not read and the UID is verified off the card afterwards, a bad
 *        CRC is not worth abandoning a half-finished write over - one was seen on a `30 F3` in
 *        normal use, and the retry that followed succeeded. For a trace read it is fatal, because
 *        the bytes are the point. nfc_magic tolerates the same error in its Gen2 detection.
 */
static Iso14443_3aError sniffer_card_poller_read_cmd(
    SnifferCardPoller* instance,
    Iso14443_3aPoller* iso3_poller,
    uint8_t arg,
    bool allow_wrong_crc) {
    bit_buffer_reset(instance->tx_buffer);
    bit_buffer_append_byte(instance->tx_buffer, SNIFFER_CARD_CMD_READ);
    bit_buffer_append_byte(instance->tx_buffer, arg);

    const Iso14443_3aError error = iso14443_3a_poller_send_standard_frame(
        iso3_poller, instance->tx_buffer, instance->rx_buffer, SNIFFER_CARD_POLLER_MAX_FWT);

    if(error == Iso14443_3aErrorWrongCrc && allow_wrong_crc) {
        FURI_LOG_W(TAG, "30 %02X -> bad CRC, carrying on", arg);
        return Iso14443_3aErrorNone;
    }

    if(error != Iso14443_3aErrorNone) {
        FURI_LOG_D(TAG, "30 %02X -> error %d", arg, error);
        return error;
    }

    char label[12];
    snprintf(label, sizeof(label), "30 %02X ->", arg);
    sniffer_card_log_bytes(
        label,
        bit_buffer_get_data(instance->rx_buffer),
        bit_buffer_get_size_bytes(instance->rx_buffer));

    return Iso14443_3aErrorNone;
}

/** Take the UID and SAK off the activation the poller has just done. */
static void
    sniffer_card_poller_take_identity(SnifferCardPoller* instance, SnifferCardIdentity* identity) {
    const Iso14443_3aData* data = nfc_poller_get_data(instance->poller);

    size_t uid_len = 0;
    const uint8_t* uid = iso14443_3a_get_uid(data, &uid_len);

    furi_check(uid_len <= sizeof(identity->uid));
    memset(identity, 0, sizeof(*identity));
    memcpy(identity->uid, uid, uid_len);
    identity->uid_len = uid_len;
    identity->sak = iso14443_3a_get_sak(data);
}

/** Read every trace slot the card will give up.
 *
 * Only the commands the vendor's own app sends on its read path are used. Its UID-write path
 * opens with `30 F3`, whose answer is a better identity signature than anything here - but that
 * byte is also the first step of a five-command sequence that arms a UID write, and nothing
 * establishes what a card does with a lone, unfinished knock. A read must not be able to change
 * the card, so the signature is not worth it.
 */
static SnifferCardPollerResult
    sniffer_card_poller_read_traces(SnifferCardPoller* instance, Iso14443_3aPoller* iso3_poller) {
    if(instance->identity.sak == SNIFFER_CARD_SAK_NOT_CLASSIC) {
        FURI_LOG_I(TAG, "Not a Classic-emulating tag, SAK %02X", instance->identity.sak);
        return SnifferCardPollerResultNotClassic;
    }

    instance->trace_count = 0;

    uint8_t page = 0;
    for(; page < SNIFFER_CARD_TRACE_SLOTS; page++) {
        const Iso14443_3aError error =
            sniffer_card_poller_read_cmd(instance, iso3_poller, page, false);

        if(error != Iso14443_3aErrorNone) {
            // Why the read stopped decides what the user is told, so the two reasons are kept
            // apart rather than inferred afterwards from how far the loop got.
            //
            // A card that has stopped answering altogether still holds whatever it had not
            // handed over, and the usual advice - go and tap a reader - would overwrite it. A
            // card that merely refuses this slot has run out of slots, which is how a card with
            // seven of them ends a loop written for eight. Judging that by the page number
            // instead would call every read of such a card truncated.
            instance->read_truncated = error == Iso14443_3aErrorTimeout ||
                                       error == Iso14443_3aErrorNotPresent;
            break;
        }

        // An answer of the wrong shape is the card declining, not the link failing.
        if(bit_buffer_get_size_bytes(instance->rx_buffer) != SNIFFER_CARD_TRACE_SIZE) break;

        SnifferTrace trace;
        if(sniffer_trace_parse_gen2(bit_buffer_get_data(instance->rx_buffer), &trace)) {
            instance->traces[instance->trace_count++] = trace;
        }
    }

    // A MIFARE Classic answers an unauthenticated read with a NAK, so no slot read at all is the
    // ordinary outcome for an ordinary card and not an error worth reporting as one.
    if(page == 0) {
        FURI_LOG_I(TAG, "No slot answered: not a Gen2 sniffer card");
        return SnifferCardPollerResultNotSniffer;
    }

    if(instance->read_truncated) {
        FURI_LOG_W(TAG, "Read broke off at slot %u of %u", page, SNIFFER_CARD_TRACE_SLOTS);
    }

    FURI_LOG_I(TAG, "%u slots read, %zu captures", page, instance->trace_count);

    return SnifferCardPollerResultSuccess;
}

/** Knock, then write the UID a byte per command. */
static SnifferCardPollerResult
    sniffer_card_poller_write_uid(SnifferCardPoller* instance, Iso14443_3aPoller* iso3_poller) {
    if(instance->identity.sak == SNIFFER_CARD_SAK_NOT_CLASSIC) {
        FURI_LOG_I(TAG, "Not a Classic-emulating tag, SAK %02X", instance->identity.sak);
        return SnifferCardPollerResultNotClassic;
    }

    // The vendor app refuses this too, and for a physical reason: the card is a 4-byte or a
    // 7-byte part. Writing a 7-byte UID into a 4-byte card has nowhere to put the other three
    // bytes, and the reader the card is meant to fool would see a UID of the wrong length.
    if(instance->identity.uid_len != instance->uid_to_write_len) {
        FURI_LOG_W(
            TAG,
            "Card UID is %zu bytes, asked to write %zu",
            instance->identity.uid_len,
            instance->uid_to_write_len);
        return SnifferCardPollerResultUidLenMismatch;
    }

    sniffer_card_log_bytes(
        "sniffer UID before:", instance->identity.uid, instance->identity.uid_len);
    sniffer_card_log_bytes("UID to write:", instance->uid_to_write, SNIFFER_CARD_UID_WRITE_BYTES);
    FURI_LOG_I(TAG, "SAK %02X", instance->identity.sak);

    for(size_t i = 0; i < COUNT_OF(sniffer_card_uid_knock); i++) {
        if(sniffer_card_poller_read_cmd(instance, iso3_poller, sniffer_card_uid_knock[i], true) !=
           Iso14443_3aErrorNone) {
            FURI_LOG_E(TAG, "Knock step %zu failed", i);
            return SnifferCardPollerResultError;
        }
        furi_delay_ms(SNIFFER_CARD_WRITE_GAP_MS);
    }

    for(size_t i = 0; i < SNIFFER_CARD_UID_WRITE_BYTES; i++) {
        if(sniffer_card_poller_read_cmd(instance, iso3_poller, instance->uid_to_write[i], true) !=
           Iso14443_3aErrorNone) {
            FURI_LOG_E(TAG, "UID byte %zu failed", i);
            return SnifferCardPollerResultError;
        }
        furi_delay_ms(SNIFFER_CARD_WRITE_GAP_MS);
    }

    furi_delay_ms(SNIFFER_CARD_COMMIT_MS);

    // The commit. Sent as an ordinary frame rather than through
    // iso14443_3a_poller_halt(), which discards the answer - and the answer is the only thing
    // that distinguishes a card that took the UID from one that ignored the whole exchange.
    bit_buffer_reset(instance->tx_buffer);
    bit_buffer_append_byte(instance->tx_buffer, SNIFFER_CARD_CMD_HALT_0);
    bit_buffer_append_byte(instance->tx_buffer, SNIFFER_CARD_CMD_HALT_1);

    const Iso14443_3aError error = iso14443_3a_poller_send_standard_frame(
        iso3_poller, instance->tx_buffer, instance->rx_buffer, SNIFFER_CARD_POLLER_MAX_FWT);
    if(error != Iso14443_3aErrorNone && error != Iso14443_3aErrorWrongCrc) {
        // An ordinary tag answers a halt with silence, so a timeout here says this card does not
        // treat HLTA as a commit - which is as much as can be concluded without it.
        FURI_LOG_E(TAG, "No answer to the committing halt: %d", error);
        return SnifferCardPollerResultError;
    }

    // bit_buffer_get_byte bounds-checks against the buffer's capacity, not against how much was
    // actually received (`furi_check(buf->capacity_bytes > index)`), so reading byte 0 of an
    // empty frame quietly returns whatever the previous exchange left there - which on this path
    // is the 0x11 from the last UID byte, and would read as a card that is merely not committed.
    if(bit_buffer_get_size_bytes(instance->rx_buffer) == 0) {
        FURI_LOG_E(TAG, "Commit answered with an empty frame");
        return SnifferCardPollerResultError;
    }

    const uint8_t status = bit_buffer_get_byte(instance->rx_buffer, 0);
    FURI_LOG_I(TAG, "halt answered, status %02X", status);
    if(status != SNIFFER_CARD_STATUS_COMMITTED) {
        if(error == Iso14443_3aErrorWrongCrc) {
            // The same reasoning that tolerates a bad CRC on the way in applies to the answer
            // coming back: this byte came out of a frame known to be corrupt, so it is not
            // evidence of anything. Let the read-back decide, since it is what decides anyway.
            FURI_LOG_W(TAG, "commit status %02X arrived with a bad CRC, verifying anyway", status);
        } else {
            FURI_LOG_E(TAG, "expected %02X from the commit", SNIFFER_CARD_STATUS_COMMITTED);
            return SnifferCardPollerResultError;
        }
    }

    // The card writes its memory now and answers nothing while it does.
    furi_delay_ms(SNIFFER_CARD_COMMIT_MS);

    return SnifferCardPollerResultSuccess;
}

/** Compare what the card answers as now against what it was told to be. */
static SnifferCardPollerResult sniffer_card_poller_verify_uid(SnifferCardPoller* instance) {
    SnifferCardIdentity after;
    sniffer_card_poller_take_identity(instance, &after);
    instance->identity = after;

    sniffer_card_log_bytes("sniffer UID after:", after.uid, after.uid_len);

    if(after.uid_len != instance->uid_to_write_len ||
       memcmp(after.uid, instance->uid_to_write, after.uid_len) != 0) {
        FURI_LOG_E(TAG, "UID write did not take");
        return SnifferCardPollerResultWriteMismatch;
    }

    FURI_LOG_I(TAG, "UID write took");

    return SnifferCardPollerResultSuccess;
}

static NfcCommand sniffer_card_poller_nfc_callback(NfcGenericEvent event, void* context) {
    furi_assert(context);
    furi_assert(event.protocol == NfcProtocolIso14443_3a);
    furi_assert(event.event_data);
    furi_assert(event.instance);

    SnifferCardPoller* instance = context;
    Iso14443_3aPoller* iso3_poller = event.instance;
    const Iso14443_3aPollerEvent* iso3_event = event.event_data;

    if(iso3_event->type != Iso14443_3aPollerEventTypeReady) {
        // The read-back is the one place where a tag failing to activate is a verdict rather
        // than something to keep waiting through: the card was there a moment ago, and if it is
        // not there now then nothing can confirm what the write did.
        //
        // But not on the first miss. The card is writing its memory at this point and answers
        // nothing while it does - the sniff of the vendor app shows exactly one WUPA going
        // unanswered before the card returns under its new UID. Giving up on that one would
        // report a successful write as unconfirmed, so a few failed activations are spent first.
        if(instance->mode == SnifferCardPollerModeWriteUid &&
           instance->write_step == SnifferCardWriteStepVerify) {
            if(++instance->verify_attempts < SNIFFER_CARD_VERIFY_ATTEMPTS) {
                return NfcCommandContinue;
            }

            FURI_LOG_E(TAG, "Card did not come back to be checked");
            instance->result = SnifferCardPollerResultWriteUnverified;
            instance->callback(instance->context);
            return NfcCommandStop;
        }

        // Otherwise an empty field is the normal state of the screen that started this session,
        // which is asking for a card to be presented.
        return NfcCommandContinue;
    }

    if(instance->mode == SnifferCardPollerModeWriteUid &&
       instance->write_step == SnifferCardWriteStepVerify) {
        instance->result = sniffer_card_poller_verify_uid(instance);

        instance->callback(instance->context);
        return NfcCommandStop;
    }

    sniffer_card_poller_take_identity(instance, &instance->identity);

    switch(instance->mode) {
    case SnifferCardPollerModeReadIdentity:
        instance->result = SnifferCardPollerResultSuccess;
        break;

    case SnifferCardPollerModeReadTraces:
        instance->result = sniffer_card_poller_read_traces(instance, iso3_poller);
        break;

    case SnifferCardPollerModeWriteUid:
        instance->result = sniffer_card_poller_write_uid(instance, iso3_poller);
        if(instance->result == SnifferCardPollerResultSuccess) {
            // Not a result yet: cycle the field so the card is powered down and selected again,
            // and judge the write on what it answers as then.
            instance->write_step = SnifferCardWriteStepVerify;
            return NfcCommandReset;
        }
        break;
    }

    instance->callback(instance->context);

    return NfcCommandStop;
}

void sniffer_card_poller_start(
    SnifferCardPoller* instance,
    SnifferCardPollerMode mode,
    SnifferCardPollerCallback callback,
    void* context) {
    furi_assert(instance);
    furi_assert(callback);

    instance->mode = mode;
    instance->callback = callback;
    instance->context = context;
    instance->write_step = SnifferCardWriteStepWrite;
    instance->result = SnifferCardPollerResultWaiting;
    memset(&instance->identity, 0, sizeof(instance->identity));
    instance->trace_count = 0;
    instance->read_truncated = false;
    instance->verify_attempts = 0;
    // A write session that was never given a UID would otherwise run on whatever the last one
    // left behind, and report its length mismatch about a UID nobody chose.
    furi_check(mode != SnifferCardPollerModeWriteUid || instance->uid_to_write_len > 0);

    // A fresh poller for a fresh session; see SnifferCardPoller::poller for why it cannot be
    // allocated once and kept.
    furi_check(instance->poller == NULL);
    instance->poller = nfc_poller_alloc(instance->nfc, NfcProtocolIso14443_3a);

    nfc_poller_start(instance->poller, sniffer_card_poller_nfc_callback, instance);
}

void sniffer_card_poller_stop(SnifferCardPoller* instance) {
    furi_assert(instance);

    if(instance->poller == NULL) return;

    // Stopping is needed even when the session ended itself: the callback returning
    // NfcCommandStop ends the worker loop, but only nfc_stop() joins its thread and takes the
    // Nfc instance out of its running state.
    nfc_poller_stop(instance->poller);
    nfc_poller_free(instance->poller);
    instance->poller = NULL;
}

SnifferCardPollerResult sniffer_card_poller_get_result(const SnifferCardPoller* instance) {
    furi_assert(instance);

    return instance->result;
}

const SnifferCardIdentity* sniffer_card_poller_get_identity(const SnifferCardPoller* instance) {
    furi_assert(instance);

    return &instance->identity;
}

size_t
    sniffer_card_poller_get_traces(const SnifferCardPoller* instance, const SnifferTrace** traces) {
    furi_assert(instance);
    furi_assert(traces);

    *traces = instance->traces;

    return instance->trace_count;
}

bool sniffer_card_poller_read_was_truncated(const SnifferCardPoller* instance) {
    furi_assert(instance);

    return instance->read_truncated;
}
