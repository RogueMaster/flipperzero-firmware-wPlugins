#include "tag_facts.h"

#include <string.h>

#include "bambu_colors.h"

#define VARIANT_MAX 8

/* A data block the host would read: present, in a sector read whole. */
static const uint8_t* block(const TagDump* dump, uint8_t n) {
    if(!tag_dump_block_present(dump, n)) return NULL;
    if(tag_dump_sector_missing(dump, n / TAG_DUMP_MFC_BLOCKS_PER_SECTOR)) return NULL;
    return dump->blocks[n];
}

/* NUL-terminated printable ASCII, trimmed; "" when empty or not text. */
static void text(const uint8_t* data, size_t len, char* out, size_t cap) {
    out[0] = '\0';
    size_t n = 0;
    while(n < len && data[n] != 0) {
        if(data[n] < 0x20 || data[n] > 0x7E) return;
        n++;
    }
    size_t start = 0;
    while(start < n && data[start] == ' ')
        start++;
    while(n > start && data[n - 1] == ' ')
        n--;
    if(n - start >= cap) n = start + cap - 1;
    memcpy(out, data + start, n - start);
    out[n - start] = '\0';
}

static uint16_t u16(const uint8_t* data, size_t offset) {
    return (uint16_t)(data[offset] | data[offset + 1] << 8);
}

static uint16_t temperature(uint16_t celsius) {
    return celsius >= 1 && celsius <= 999 ? celsius : 0;
}

/* "1.75" (two decimals, as the host's %.2f); "" outside 0-100 mm. */
static void diameter(const uint8_t* data, char out[PROTO_DIAMETER_MAX + 1]) {
    float mm;
    memcpy(&mm, data, sizeof(mm)); /* little endian, as the Flipper */
    out[0] = '\0';
    if(!(mm > 0.0f && mm < 100.0f)) return;
    unsigned hundredths = (unsigned)(mm * 100.0f + 0.5f);
    if(hundredths >= 10000) return;
    char* p = out;
    if(hundredths >= 1000) *p++ = (char)('0' + hundredths / 1000);
    *p++ = (char)('0' + hundredths / 100 % 10);
    *p++ = '.';
    *p++ = (char)('0' + hundredths / 10 % 10);
    *p++ = (char)('0' + hundredths % 10);
    *p = '\0';
}

static char lower(char c) {
    return c >= 'A' && c <= 'Z' ? (char)(c - 'A' + 'a') : c;
}

static bool same_nocase(const char* a, const char* b, size_t n) {
    for(size_t i = 0; i < n; i++)
        if(lower(a[i]) != lower(b[i])) return false;
    return true;
}

/* The detailed type without a first word equal to the material
 * (case-insensitive): ("PLA Basic", "PLA") -> "Basic". */
static void subtype(const char* detailed, const char* material, char* out, size_t cap) {
    size_t m = strlen(material);
    const char* rest = detailed;
    bool word = m > 0 && strlen(detailed) >= m && same_nocase(detailed, material, m) &&
                (detailed[m] == ' ' || detailed[m] == '\0');
    if(word) {
        rest = detailed + m;
        while(*rest == ' ')
            rest++;
    }
    strncpy(out, rest, cap - 1);
    out[cap - 1] = '\0';
}

static void put_code(uint32_t code, char* out, size_t cap) {
    char digits[11];
    size_t i = sizeof(digits) - 1;
    digits[i] = '\0';
    do {
        digits[--i] = (char)('0' + code % 10);
        code /= 10;
    } while(code > 0);
    strncpy(out, digits + i, cap - 1);
    out[cap - 1] = '\0';
}

bool tag_facts_read(const TagDump* dump, TagFacts* facts) {
    memset(facts, 0, sizeof(*facts));
    if(dump->type != TagTypeMfc) return false;
    ProtoTagInfo* info = &facts->info;
    bool any = false;
    char variant[VARIANT_MAX + 1] = "";
    const uint8_t* b;
    if((b = block(dump, 1)) != NULL) {
        text(b, 8, variant, sizeof(variant));
        text(b + 8, 8, info->filament_id, sizeof(info->filament_id));
        any = true;
    }
    if((b = block(dump, 2)) != NULL) {
        text(b, TAG_DUMP_BLOCK_LEN, info->material, sizeof(info->material));
        any = true;
    }
    if((b = block(dump, 4)) != NULL) {
        char detailed[TAG_DUMP_BLOCK_LEN + 1];
        text(b, TAG_DUMP_BLOCK_LEN, detailed, sizeof(detailed));
        subtype(detailed, info->material, info->subtype, sizeof(info->subtype));
        any = true;
    }
    if((b = block(dump, 5)) != NULL) {
        info->has_rgba = true;
        info->rgba = (uint32_t)b[0] << 24 | (uint32_t)b[1] << 16 | (uint32_t)b[2] << 8 | b[3];
        uint16_t label = u16(b, 4);
        info->has_label = label != 0;
        info->label = label > PROTO_GRAMS_MAX ? PROTO_GRAMS_MAX : label;
        diameter(b + 8, info->diameter);
        any = true;
    }
    if((b = block(dump, 6)) != NULL) {
        facts->drying_temp = temperature(u16(b, 0));
        facts->drying_hours = u16(b, 2);
        facts->bed_temp = temperature(u16(b, 6));
        info->nozzle_max = temperature(u16(b, 8));
        info->has_nozzle_max = info->nozzle_max != 0;
        info->nozzle_min = temperature(u16(b, 10));
        info->has_nozzle_min = info->nozzle_min != 0;
        any = true;
    }
    BambuColor color;
    if(bambu_color_find(info->filament_id, variant, info->has_rgba, info->rgba, &color)) {
        put_code(color.code, info->code, sizeof(info->code));
        strncpy(info->color_name, color.name, sizeof(info->color_name) - 1);
    }
    return any;
}
