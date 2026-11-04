#include "sniffer_card_nonce_log.h"

#include <inttypes.h>
#include <storage/storage.h>
#include <toolbox/stream/buffered_file_stream.h>
#include <toolbox/stream/stream.h>

#define TAG "SnifferCard"

/** One nonce pair, spelled exactly as the firmware's `mfkey32_logger_save_params()` spells it
 * (`applications/main/nfc/helpers/mfkey32_logger.c`).
 *
 * The format matters to the byte: the MFKey app does not parse this with a format string. It
 * checks the line starts with `Sec`, then walks space-separated tokens and takes 5, 7, 9, 11, 13,
 * 15 and 17 as cuid, nt0, nr0, ar0, nt1, nr1 and ar1 (`applications/system/mfkey/init_plugin.c`).
 * Any change to the token count or the spacing silently feeds it the wrong fields.
 */
#define SNIFFER_CARD_NONCE_LOG_FORMAT                                                     \
    "Sec %u key %c cuid %08" PRIx32 " nt0 %08" PRIx32 " nr0 %08" PRIx32 " ar0 %08" PRIx32 \
    " nt1 %08" PRIx32 " nr1 %08" PRIx32 " ar1 %08" PRIx32 "\n"

/** The same line read back, for telling a pair already in the log from a new one.
 *
 * SCNx32 rather than PRIx32: the same letter on this target, but these are read with sscanf and
 * the two macros are not interchangeable by contract.
 */
#define SNIFFER_CARD_NONCE_LOG_SCAN_FORMAT                                            \
    "Sec %u key %c cuid %8" SCNx32 " nt0 %8" SCNx32 " nr0 %8" SCNx32 " ar0 %8" SCNx32 \
    " nt1 %8" SCNx32 " nr1 %8" SCNx32 " ar1 %8" SCNx32

/** Conversions SNIFFER_CARD_NONCE_LOG_SCAN_FORMAT performs when a line is a nonce pair. */
#define SNIFFER_CARD_NONCE_LOG_SCAN_FIELDS 9

/** The six words of one authentication pair, plus the card they belong to.
 *
 * What makes two lines the same line. The sector and key type are deliberately not part of it:
 * several pairs of one sector and key type are expected now, and comparing on those would throw
 * away every combination after the first.
 */
typedef struct {
    uint32_t cuid;
    uint32_t nt0, nr0, ar0;
    uint32_t nt1, nr1, ar1;
} SnifferCardPairRef;

/** A pair of captures and whether the log already holds it. */
typedef struct {
    const SnifferTrace* a;
    const SnifferTrace* b;
    uint8_t sector;
    uint8_t key_type;
    bool duplicate;
} SnifferCardNoncePair;

static SnifferCardPairRef sniffer_card_pair_ref(const SnifferTrace* a, const SnifferTrace* b) {
    return (SnifferCardPairRef){
        .cuid = sniffer_trace_get_cuid(a),
        .nt0 = a->nt,
        .nr0 = a->nr,
        .ar0 = a->ar,
        .nt1 = b->nt,
        .nr1 = b->nr,
        .ar1 = b->ar,
    };
}

/** Whether two pair references describe the same two authentications.
 *
 * Either way round. Nothing this app writes swaps them, but a pair that arrived from somewhere
 * else with its halves the other way about is still the same work for MFKey.
 */
static bool sniffer_card_pair_ref_eq(const SnifferCardPairRef* x, const SnifferCardPairRef* y) {
    if(x->cuid != y->cuid) return false;

    const bool same_order = x->nt0 == y->nt0 && x->nr0 == y->nr0 && x->ar0 == y->ar0 &&
                            x->nt1 == y->nt1 && x->nr1 == y->nr1 && x->ar1 == y->ar1;
    const bool swapped = x->nt0 == y->nt1 && x->nr0 == y->nr1 && x->ar0 == y->ar1 &&
                         x->nt1 == y->nt0 && x->nr1 == y->nr0 && x->ar1 == y->ar0;

    return same_order || swapped;
}

/** Build every pair of captures that could share a key, and count the captures that cannot.
 *
 * Two captures qualify when they are of one sector and key type and their tag nonces differ. A
 * shared nonce means a shared keystream position, so mfkey32 would have nothing to intersect -
 * minutes spent on a line that cannot succeed.
 *
 * @param[out] pairs room for SNIFFER_CARD_MAX_PAIRS
 * @return how many pairs were made
 */
static size_t sniffer_card_nonce_log_collect(
    const SnifferTrace* traces,
    size_t count,
    SnifferCardNoncePair* pairs,
    size_t* unpaired) {
    furi_check(count <= SNIFFER_CARD_TRACE_SLOTS);

    bool in_pair[SNIFFER_CARD_TRACE_SLOTS] = {0};
    size_t pair_count = 0;

    for(size_t i = 0; i < count; i++) {
        for(size_t j = i + 1; j < count; j++) {
            if(!sniffer_trace_same_key(&traces[i], &traces[j])) continue;
            if(traces[i].nt == traces[j].nt) continue;

            furi_check(pair_count < SNIFFER_CARD_MAX_PAIRS);
            pairs[pair_count++] = (SnifferCardNoncePair){
                .a = &traces[i],
                .b = &traces[j],
                .sector = sniffer_trace_get_sector(&traces[i]),
                .key_type = traces[i].key_type,
                .duplicate = false,
            };

            in_pair[i] = true;
            in_pair[j] = true;
        }
    }

    *unpaired = 0;
    for(size_t i = 0; i < count; i++) {
        if(!in_pair[i]) (*unpaired)++;
    }

    return pair_count;
}

/** Mark the pairs the log already holds.
 *
 * Reads the log once and compares against every candidate, rather than building a set of what is
 * in the file: the candidates are bounded and the file is not, so the small side is the one worth
 * holding in memory.
 *
 * A log that cannot be read is treated as a log with nothing in it. Writing a duplicate costs
 * MFKey time; refusing to write because the file could not be examined would cost the user their
 * captures, which the card will overwrite.
 */
static void sniffer_card_nonce_log_mark_duplicates(
    Storage* storage,
    SnifferCardNoncePair* pairs,
    size_t pair_count,
    bool* log_unchecked) {
    // A log that is not there yet holds nothing, which is a complete answer rather than an
    // unchecked one.
    if(!storage_file_exists(storage, SNIFFER_CARD_NONCE_LOG_PATH)) return;

    Stream* stream = buffered_file_stream_alloc(storage);
    FuriString* line = furi_string_alloc();

    if(buffered_file_stream_open(
           stream, SNIFFER_CARD_NONCE_LOG_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        while(stream_read_line(stream, line)) {
            unsigned int sector = 0;
            char key_letter = 0;
            SnifferCardPairRef logged = {0};

            if(sscanf(
                   furi_string_get_cstr(line),
                   SNIFFER_CARD_NONCE_LOG_SCAN_FORMAT,
                   &sector,
                   &key_letter,
                   &logged.cuid,
                   &logged.nt0,
                   &logged.nr0,
                   &logged.ar0,
                   &logged.nt1,
                   &logged.nr1,
                   &logged.ar1) != SNIFFER_CARD_NONCE_LOG_SCAN_FIELDS) {
                continue;
            }

            for(size_t i = 0; i < pair_count; i++) {
                if(pairs[i].duplicate) continue;

                const SnifferCardPairRef candidate = sniffer_card_pair_ref(pairs[i].a, pairs[i].b);
                if(sniffer_card_pair_ref_eq(&candidate, &logged)) pairs[i].duplicate = true;
            }
        }
    } else {
        FURI_LOG_W(TAG, "Cannot read %s, treating it as empty", SNIFFER_CARD_NONCE_LOG_PATH);
        *log_unchecked = true;
    }

    furi_string_free(line);
    buffered_file_stream_close(stream);
    stream_free(stream);
}

static void sniffer_card_nonce_log_format(const SnifferCardNoncePair* pair, FuriString* line) {
    const SnifferTrace* a = pair->a;
    const SnifferTrace* b = pair->b;

    furi_string_printf(
        line,
        SNIFFER_CARD_NONCE_LOG_FORMAT,
        pair->sector,
        pair->key_type == SNIFFER_CARD_AUTH_KEY_A ? 'A' : 'B',
        sniffer_trace_get_cuid(a),
        a->nt,
        a->nr,
        a->ar,
        b->nt,
        b->nr,
        b->ar);
}

/** Pair the captures and check them against the log, without writing anything.
 *
 * Counting and writing share this so they cannot disagree: a screen that promised nine pairs and
 * a log that got eight would be a bug invisible from either side.
 */
static size_t sniffer_card_nonce_log_prepare(
    Storage* storage,
    const SnifferTrace* traces,
    size_t count,
    SnifferCardNoncePair* pairs,
    SnifferCardNonceLogResult* result) {
    memset(result, 0, sizeof(*result));

    const size_t pair_count =
        sniffer_card_nonce_log_collect(traces, count, pairs, &result->traces_unpaired);

    sniffer_card_nonce_log_mark_duplicates(storage, pairs, pair_count, &result->log_unchecked);

    for(size_t i = 0; i < pair_count; i++) {
        if(pairs[i].duplicate) {
            result->pairs_duplicate++;
        } else {
            result->pairs_written++;
        }
    }

    return pair_count;
}

void sniffer_card_nonce_log_count(
    const SnifferTrace* traces,
    size_t count,
    SnifferCardNonceLogResult* result) {
    furi_assert(traces);
    furi_assert(result);

    Storage* storage = furi_record_open(RECORD_STORAGE);
    SnifferCardNoncePair pairs[SNIFFER_CARD_MAX_PAIRS];

    sniffer_card_nonce_log_prepare(storage, traces, count, pairs, result);

    furi_record_close(RECORD_STORAGE);
}

bool sniffer_card_nonce_log_save(
    const SnifferTrace* traces,
    size_t count,
    SnifferCardNonceLogResult* result) {
    furi_assert(traces);
    furi_assert(result);

    Storage* storage = furi_record_open(RECORD_STORAGE);
    SnifferCardNoncePair pairs[SNIFFER_CARD_MAX_PAIRS];
    const size_t pair_count =
        sniffer_card_nonce_log_prepare(storage, traces, count, pairs, result);

    Stream* stream = buffered_file_stream_alloc(storage);
    FuriString* block = furi_string_alloc();
    FuriString* line = furi_string_alloc();
    bool opened = false;
    bool saved = false;

    do {
        // Nothing new to add is not a failure: the screen reports it as the duplicate count, and
        // opening the log to write nothing would only risk a write error where there is no write.
        if(result->pairs_written == 0) {
            saved = true;
            break;
        }

        // The firmware's NFC app makes this directory before writing into it
        // (`nfc_app.c` calls storage_simply_mkdir on NFC_APP_FOLDER). A Flipper that has never
        // saved a card does not have it, and without this the open below fails for a reason the
        // user cannot act on and the app could have fixed itself.
        storage_simply_mkdir(storage, SNIFFER_CARD_NONCE_LOG_FOLDER);

        if(!buffered_file_stream_open(
               stream, SNIFFER_CARD_NONCE_LOG_PATH, FSAM_WRITE, FSOM_OPEN_APPEND)) {
            FURI_LOG_E(TAG, "Cannot open %s", SNIFFER_CARD_NONCE_LOG_PATH);
            result->open_failed = true;
            break;
        }
        opened = true;

        // Built whole, then written once. Writing a line at a time leaves a failure window per
        // line, and a failure inside one lands mid-line - which matters because this file is
        // shared with the firmware's NFC app and MFKey's loader does not validate what it reads:
        // it takes any line starting with "Sec", stops at the first missing token, and uses the
        // rest as zeroes. A half-written line would become permanent, well-formed-looking work
        // that can never succeed.
        furi_string_reset(block);
        for(size_t i = 0; i < pair_count; i++) {
            if(pairs[i].duplicate) continue;

            sniffer_card_nonce_log_format(&pairs[i], line);
            furi_string_cat(block, line);
        }

        if(stream_write_string(stream, block) != furi_string_size(block)) {
            FURI_LOG_E(TAG, "Cannot write %s", SNIFFER_CARD_NONCE_LOG_PATH);
            break;
        }

        // The stream caches about a kilobyte in RAM and only reaches the card when that fills,
        // so up to eleven pairs none of this has touched the SD card yet. Without the sync, a
        // full or absent card is discovered inside the close below - whose result used to be
        // discarded, reporting "Saved" for nonces that never left RAM. The captures are gone by
        // then: the card keeps them only until the next reader overwrites them.
        if(!buffered_file_stream_sync(stream)) {
            FURI_LOG_E(TAG, "Cannot flush %s", SNIFFER_CARD_NONCE_LOG_PATH);
            break;
        }

        saved = true;
    } while(false);

    furi_string_free(line);
    furi_string_free(block);

    // Part of the result, not cleanup: this is where the last of the data reaches the card.
    if(!buffered_file_stream_close(stream) && opened && saved) {
        FURI_LOG_E(TAG, "Cannot close %s", SNIFFER_CARD_NONCE_LOG_PATH);
        saved = false;
    }

    stream_free(stream);
    furi_record_close(RECORD_STORAGE);

    if(!saved) {
        // Nothing is known to have landed, so the screen must not claim any of it did.
        result->pairs_written = 0;
    }

    return saved;
}
