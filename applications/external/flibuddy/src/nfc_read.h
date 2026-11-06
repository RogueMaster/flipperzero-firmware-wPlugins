/* Reads one filament spool tag into a TagDump (Flipper only).
 *
 * tag_reader_start() runs a worker thread that first waits until the tag
 * it read last has left the field (tag_watch.h), so a spool left on the
 * Flipper is read once; then it waits for an ISO 14443-3A tag
 * (nfc_scanner), reports TagReaderEventDetected, reads the tag with
 * tag_reader_read() and reports TagReaderEventRead, TagReaderEventFailed or
 * TagReaderEventUnsupported;
 * then the worker ends, so one start reads at most one tag. Tags of other
 * technologies are ignored. Callbacks run on the worker thread.
 *
 * tag_reader_stop() stops the NFC and joins the worker, and no callback
 * runs after it returns. Waits for a tag and for it to leave are cut short
 * at once. A read in progress is not: the SDK's sync pollers wait on
 * thread flag bit 0, which the reader's flags stay off (reader_wait.h), so
 * the stop flag cannot wake them and stop() waits for the read in progress
 * to finish. A tag whose read was stopped that way reports nothing and is
 * not remembered, so the next start reads it.
 */
#pragma once

#include <stdbool.h>

#include "nfc_classify.h"
#include "tag_dump.h"

typedef enum {
    TagReaderEventDetected, /* a tag is in the field; reading starts */
    TagReaderEventRead, /* the dump passed to start() holds the tag */
    TagReaderEventFailed, /* lost, or no data: may succeed on the next try */
    TagReaderEventUnsupported, /* a tag the app cannot read: unsupported SAK, locked NTAG */
} TagReaderEvent;

typedef void (*TagReaderCallback)(TagReaderEvent event, void* ctx);

typedef struct TagReader TagReader;

TagReader* tag_reader_alloc(void);

/* Stops the reader if needed. */
void tag_reader_free(TagReader* reader);

/* Starts waiting for a tag; `out` must stay valid until the next stop().
 * A reader stays started until stop(), even after its worker has ended;
 * start() does nothing while it is started. */
void tag_reader_start(TagReader* reader, TagDump* out, TagReaderCallback callback, void* ctx);

void tag_reader_stop(TagReader* reader);

/* Forgets the tag read last, so the next worker reads it again even if it
 * is still in the field. For a result the caller dropped (a read that
 * finished after the screen moved on). Call it after stop(), from the GUI
 * thread. */
void tag_reader_forget(TagReader* reader);

/* Call start, stop and is_started from one thread (the GUI thread). */
bool tag_reader_is_started(const TagReader* reader);

/* Reads the tag in the field, blocking: UID, SAK and ATQA, then for SAK
 * 0x08/0x18 every MIFARE Classic sector with the Bambu Key A derived from
 * the 4-byte UID (sectors that fail stay missing), for SAK 0x00 every NTAG
 * page. Call it from a FuriThread (the sync pollers wait on thread flags).
 * When it does not read, `out` still holds the UID if a tag answered
 * (uid_len 0 otherwise). Either way the worker remembers that UID, so the
 * tag is not read again until it has left the field.
 */
TagReadResult tag_reader_read(TagReader* reader, TagDump* out);
