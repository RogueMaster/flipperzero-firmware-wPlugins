#include "tag_dump.h"

#include <string.h>

void tag_dump_init(TagDump* dump, TagType type) {
    memset(dump, 0, sizeof(*dump));
    dump->type = type;
}

static bool tag_dump_uid_len_valid(size_t len) {
    return len == 4 || len == 7 || len == 10;
}

bool tag_dump_set_uid(TagDump* dump, const uint8_t* uid, size_t len) {
    if(!tag_dump_uid_len_valid(len)) return false;
    memcpy(dump->uid, uid, len);
    dump->uid_len = (uint8_t)len;
    return true;
}

bool tag_dump_set_block(TagDump* dump, uint8_t block, const uint8_t data[TAG_DUMP_BLOCK_LEN]) {
    if(dump->type != TagTypeMfc || block >= TAG_DUMP_MFC_BLOCKS) return false;
    memcpy(dump->blocks[block], data, TAG_DUMP_BLOCK_LEN);
    dump->block_present |= (uint64_t)1 << block;
    return true;
}

bool tag_dump_block_present(const TagDump* dump, uint8_t block) {
    return block < TAG_DUMP_MFC_BLOCKS && (dump->block_present >> block) & 1U;
}

bool tag_dump_sector_missing(const TagDump* dump, uint8_t sector) {
    if(sector >= TAG_DUMP_MFC_SECTORS) return true;
    uint8_t first = (uint8_t)(sector * TAG_DUMP_MFC_BLOCKS_PER_SECTOR);
    for(uint8_t b = first; b < first + TAG_DUMP_MFC_BLOCKS_PER_SECTOR - 1; b++)
        if(!tag_dump_block_present(dump, b)) return true;
    return false;
}

bool tag_dump_append_page(TagDump* dump, const uint8_t data[TAG_DUMP_PAGE_LEN]) {
    if(dump->type != TagTypeNtag || dump->page_count >= TAG_DUMP_MAX_PAGES) return false;
    memcpy(dump->pages[dump->page_count++], data, TAG_DUMP_PAGE_LEN);
    return true;
}

bool tag_dump_valid(const TagDump* dump) {
    return (dump->type == TagTypeMfc || dump->type == TagTypeNtag) &&
           tag_dump_uid_len_valid(dump->uid_len) && dump->page_count <= TAG_DUMP_MAX_PAGES;
}
