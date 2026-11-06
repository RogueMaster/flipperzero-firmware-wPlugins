#include "nfc_classify.h"

#define SAK_MFC_1K 0x08
#define SAK_MFC_4K 0x18
#define SAK_NTAG   0x00

bool tag_type_from_sak(uint8_t sak, TagType* type) {
    switch(sak) {
    case SAK_MFC_1K:
    case SAK_MFC_4K:
        *type = TagTypeMfc;
        return true;
    case SAK_NTAG:
        *type = TagTypeNtag;
        return true;
    default:
        return false;
    }
}

TagReadResult tag_ntag_read_result(size_t pages, bool password_protected, bool still_present) {
    if(pages > 0) return TagReadOk; /* `scan` needs at least one page */
    return (password_protected || still_present) ? TagReadUnsupported : TagReadFailed;
}

bool tag_block_text(const TagDump* dump, uint8_t block, char out[TAG_BLOCK_TEXT_MAX]) {
    out[0] = '\0';
    if(dump->type != TagTypeMfc || !tag_dump_block_present(dump, block)) return false;
    const uint8_t* data = dump->blocks[block];
    size_t len = 0;
    while(len < TAG_DUMP_BLOCK_LEN && data[len] != 0) {
        uint8_t c = data[len];
        out[len++] = (c >= 0x20 && c < 0x7F) ? (char)c : '?';
    }
    while(len > 0 && out[len - 1] == ' ')
        len--;
    out[len] = '\0';
    return len > 0;
}

unsigned tag_sectors_read(const TagDump* dump) {
    if(dump->type != TagTypeMfc) return 0;
    unsigned read = 0;
    for(uint8_t s = 0; s < TAG_DUMP_MFC_SECTORS; s++)
        if(!tag_dump_sector_missing(dump, s)) read++;
    return read;
}
