/* The raw contents of one NFC tag read, as the Flipper sends it in `scan`.
 *
 * The Flipper does not interpret tag data; the host parses it. A MIFARE
 * Classic 1K dump keeps all 64 blocks with a presence bit each (the reader
 * sets the blocks of every sector it could authenticate and read). An NTAG
 * dump keeps pages from page 0 up to the end of the tag's memory.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define TAG_DUMP_MAX_UID               10
#define TAG_DUMP_MFC_BLOCKS            64
#define TAG_DUMP_MFC_SECTORS           16
#define TAG_DUMP_MFC_BLOCKS_PER_SECTOR 4
#define TAG_DUMP_BLOCK_LEN             16
#define TAG_DUMP_MAX_PAGES             231 /* NTAG216 */
#define TAG_DUMP_PAGE_LEN              4

typedef enum {
    TagTypeMfc, /* MIFARE Classic 1K, wire name "mfc" */
    TagTypeNtag, /* NTAG21x, wire name "ntag" */
} TagType;

typedef struct {
    TagType type;
    uint8_t uid[TAG_DUMP_MAX_UID];
    uint8_t uid_len;
    uint8_t sak;
    uint8_t atqa[2];
    /* A dump is one kind or the other, so the two share their storage
     * (the app keeps several dumps: the scan, the intent, Recent). */
    union {
        uint8_t blocks[TAG_DUMP_MFC_BLOCKS][TAG_DUMP_BLOCK_LEN]; /* mfc only */
        uint8_t pages[TAG_DUMP_MAX_PAGES][TAG_DUMP_PAGE_LEN]; /* ntag only */
    };
    uint64_t block_present; /* mfc: bit n set, blocks[n] holds data */
    uint16_t page_count; /* ntag */
} TagDump;

/* Clears the dump and sets its type. */
void tag_dump_init(TagDump* dump, TagType type);

/* Accepts the ISO 14443-3A UID sizes 4, 7 and 10; false leaves the dump as is. */
bool tag_dump_set_uid(TagDump* dump, const uint8_t* uid, size_t len);

/* Stores a 16-byte block and marks it present; false when block >= 64 or
 * the dump is not a MIFARE Classic one. */
bool tag_dump_set_block(TagDump* dump, uint8_t block, const uint8_t data[TAG_DUMP_BLOCK_LEN]);

bool tag_dump_block_present(const TagDump* dump, uint8_t block);

/* True unless all three data blocks of the sector are present (the trailer
 * does not count); also true for sector >= 16. */
bool tag_dump_sector_missing(const TagDump* dump, uint8_t sector);

/* Appends a 4-byte page; false when the dump already holds 231 pages or
 * is not an NTAG one. */
bool tag_dump_append_page(TagDump* dump, const uint8_t data[TAG_DUMP_PAGE_LEN]);

/* Whether the dump can be encoded: known type, valid UID length, page count
 * within bounds. */
bool tag_dump_valid(const TagDump* dump);
