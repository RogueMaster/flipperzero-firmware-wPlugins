/* Fills a TagDump from the generated fixture vectors. Include after
 * minitest.h; failed setters count as test failures. */
#pragma once

#include "minitest.h"
#include "vectors/tag_dumps.h"

#include "tag_dump.h"

static inline bool sector_in(const uint8_t* sectors, size_t count, unsigned sector) {
    for(size_t i = 0; i < count; i++)
        if(sectors[i] == sector) return true;
    return false;
}

/* A MIFARE Classic fixture as the reader would leave it: every block of the
 * readable sectors (trailers included) present, the missing sectors not. */
static inline void load_mfc_vectors(
    TagDump* d,
    const uint8_t* uid,
    size_t uid_len,
    uint8_t sak,
    const uint8_t atqa[2],
    const uint8_t (*blocks)[16],
    const uint8_t* missing,
    size_t missing_count) {
    tag_dump_init(d, TagTypeMfc);
    CHECK(tag_dump_set_uid(d, uid, uid_len));
    d->sak = sak;
    memcpy(d->atqa, atqa, 2);
    for(uint8_t b = 0; b < TAG_DUMP_MFC_BLOCKS; b++)
        if(!sector_in(missing, missing_count, b / TAG_DUMP_MFC_BLOCKS_PER_SECTOR))
            CHECK(tag_dump_set_block(d, b, blocks[b]));
}

/* The synthetic Bambu fixture, sector 7 missing as in scan_mfc.json. */
static inline void load_mfc(TagDump* d) {
    load_mfc_vectors(
        d,
        MFC_UID,
        sizeof(MFC_UID),
        MFC_SAK,
        MFC_ATQA,
        MFC_BLOCKS,
        MFC_MISSING_SECTORS,
        MFC_MISSING_COUNT);
}

/* The real Bambu dump (PLA Pure) with the sectors it was captured without. */
static inline void load_mfc_real(TagDump* d) {
    load_mfc_vectors(
        d,
        REAL_UID,
        sizeof(REAL_UID),
        REAL_SAK,
        REAL_ATQA,
        REAL_BLOCKS,
        REAL_MISSING_SECTORS,
        REAL_MISSING_COUNT);
}

static inline void load_ntag(TagDump* d) {
    tag_dump_init(d, TagTypeNtag);
    CHECK(tag_dump_set_uid(d, NTAG_UID, sizeof(NTAG_UID)));
    d->sak = NTAG_SAK;
    memcpy(d->atqa, NTAG_ATQA, sizeof(NTAG_ATQA));
    for(size_t p = 0; p < NTAG_PAGE_COUNT; p++)
        CHECK(tag_dump_append_page(d, NTAG_PAGES[p]));
}
