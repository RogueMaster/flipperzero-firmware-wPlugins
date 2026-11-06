/* Pure helpers around the NFC reader (nfc_read.h): which tags the app
 * reads, and what the screens show about a tag without the host.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "tag_dump.h"

/* Bambu MIFARE Classic blocks with ASCII text. */
#define TAG_BLOCK_MATERIAL      2
#define TAG_BLOCK_DETAILED_TYPE 4
#define TAG_BLOCK_TEXT_MAX      (TAG_DUMP_BLOCK_LEN + 1)

/* SAK 0x08 (MIFARE Classic 1K) and 0x18 (4K, read as its first 1K) → mfc;
 * 0x00 (NTAG21x / Ultralight) → ntag. Any other SAK is not supported and
 * leaves *type untouched. */
bool tag_type_from_sak(uint8_t sak, TagType* type);

/* What a tag read came to. Unsupported is a tag the app will never read
 * (an unsupported SAK, a password-protected NTAG): no error cue, no retry
 * text. Failed is a read that may succeed next time. */
typedef enum {
    TagReadOk,
    TagReadFailed,
    TagReadUnsupported,
} TagReadResult;

/* The outcome of reading an NTAG / Ultralight: `pages` read, whether the
 * SDK's read stopped on a password (MfUltralightErrorAuth), and whether the
 * tag still answers. The reader remembers a tag it could not read until it
 * leaves, so one that still answers will not be retried: it is Unsupported.
 * Only a tag that is gone is a retryable failure. */
TagReadResult tag_ntag_read_result(size_t pages, bool password_protected, bool still_present);

/* A MIFARE Classic block as text: it stops at the first NUL, turns other
 * control and non-ASCII bytes into '?' and drops trailing spaces. False
 * (and "") when the dump is not mfc, the block is missing or it holds no
 * visible text. */
bool tag_block_text(const TagDump* dump, uint8_t block, char out[TAG_BLOCK_TEXT_MAX]);

/* MIFARE Classic sectors with all three data blocks read; 0 for NTAG. */
unsigned tag_sectors_read(const TagDump* dump);
