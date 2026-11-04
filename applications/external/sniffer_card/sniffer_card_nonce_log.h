/**
 * @file sniffer_card_nonce_log.h
 * @brief Handing the captured traces to the MFKey app.
 */
#pragma once

#include "sniffer_card.h"

/** Where the firmware's NFC app keeps the nonces it collects in Detect Reader, and the only file
 * the MFKey app's mfkey32 path reads.
 *
 * Spelled out rather than taken from a header, because `NFC_APP_MFKEY32_LOGS_FILE_PATH` lives in
 * `applications/main/nfc/nfc_app_i.h`, which is firmware-private and not in the FAP SDK. Both
 * sides of that split are checked in this repo's README.
 */
#define SNIFFER_CARD_NONCE_LOG_PATH   EXT_PATH("nfc/.mfkey32.log")
#define SNIFFER_CARD_NONCE_LOG_FOLDER EXT_PATH("nfc")

/** Every combination of two captures, which is the most pairs a card can yield. */
#define SNIFFER_CARD_MAX_PAIRS (SNIFFER_CARD_TRACE_SLOTS * (SNIFFER_CARD_TRACE_SLOTS - 1) / 2)

/** What came of writing the nonces out. */
typedef struct {
    size_t pairs_written; /**< Lines appended. */
    size_t pairs_duplicate; /**< Pairs already in the log, byte for byte. */
    size_t traces_unpaired; /**< Captures that pair with nothing at all. */
    /** The log could not be opened for writing, as against written to and failed.
     *
     * Kept apart because they need different things said: one is usually a missing or full SD
     * card, the other a write that went wrong part way. */
    bool open_failed;
    /** The existing log could not be read, so nothing was compared against it.
     *
     * Everything then looks new, and re-reading a card appends work the log may already hold -
     * which MFKey will not notice, since its loader does not deduplicate. Writing a possible
     * duplicate beats refusing captures the card is about to overwrite, but the uncertainty has
     * to reach the screen rather than stop here. */
    bool log_unchecked;
} SnifferCardNonceLogResult;

/** Append every usable pair of traces to the MFKey nonce log.
 *
 * mfkey32 needs two authentications made with the same key. Which captures share a key cannot be
 * known here - that is the answer mfkey32 produces, not an input to it - so every combination of
 * two captures of one sector and key type is written.
 *
 * It is tempting to write one pair per sector and key type and call the rest redundant. A card
 * read during development disproved that: seven captures, all sector 0, four of key B and three
 * of key A, and not one of the nine possible pairings recovered a key - the reader had presented
 * a different key each time. Pairing only the first two would have written three confident lines
 * that could never succeed while a recoverable pair, had one existed, went unwritten. Proxmark3
 * pairs exhaustively for the same reason; it can afford to, finishing in milliseconds where
 * MFKey takes minutes a line, so the cost here is the user's time rather than correctness.
 *
 * A pair already in the log, byte for byte, is skipped. These cards keep their captures until a
 * reader overwrites them, so reading one twice would otherwise append the same work again - and
 * MFKey does not deduplicate: its loader runs the full attack per line, and only checks the
 * dictionaries for keys already found. The comparison is the whole pair rather than its sector
 * and key type, because two pairs of one sector and key type are now the point.
 *
 * Opened in append mode, like the firmware's own writer, so nonces from a reader sniffed with
 * the Flipper and nonces from a sniffer card accumulate in the same place.
 *
 * @return false when the log could not be opened or written
 */
bool sniffer_card_nonce_log_save(
    const SnifferTrace* traces,
    size_t count,
    SnifferCardNonceLogResult* result);

/** What sniffer_card_nonce_log_save() would do, without doing it.
 *
 * Reads the log for the duplicate count, so the screen that offers the save can say what saving
 * will actually add.
 */
void sniffer_card_nonce_log_count(
    const SnifferTrace* traces,
    size_t count,
    SnifferCardNonceLogResult* result);
