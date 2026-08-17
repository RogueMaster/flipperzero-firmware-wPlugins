#include "bambu_tag.h"

#include <stdio.h>
#include <string.h>
#include <strings.h>

namespace {
uint16_t le16(const uint8_t* p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

float le_float(const uint8_t* p) {
    uint32_t bits = (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
                    ((uint32_t)p[3] << 24);
    float value = 0.0f;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

void ascii_field(const uint8_t* src, size_t len, char* dst, size_t dst_size) {
    if(!dst || !dst_size) return;
    size_t out = 0;
    for(size_t i = 0; i < len && out + 1U < dst_size; ++i) {
        uint8_t c = src[i];
        if(c == 0) break;
        dst[out++] = (c >= 0x20 && c <= 0x7E) ? (char)c : '?';
    }
    while(out && dst[out - 1] == ' ')
        --out;
    dst[out] = '\0';
}

void hex_bytes(const uint8_t* src, size_t len, char* dst, size_t dst_size) {
    static const char hex[] = "0123456789ABCDEF";
    if(!dst || !dst_size) return;
    size_t out = 0;
    for(size_t i = 0; i < len && out + 2U < dst_size; ++i) {
        dst[out++] = hex[src[i] >> 4];
        dst[out++] = hex[src[i] & 0x0F];
    }
    dst[out] = '\0';
}

void rgba_string(const uint8_t rgba[4], char out[10]) {
    static const char hex[] = "0123456789ABCDEF";
    out[0] = '#';
    for(size_t i = 0; i < 4; ++i) {
        out[1 + i * 2] = hex[rgba[i] >> 4];
        out[2 + i * 2] = hex[rgba[i] & 0x0F];
    }
    out[9] = '\0';
}

bool has_nfc_extension(const char* path) {
    const char* dot = strrchr(path, '.');
    return dot && strcasecmp(dot, ".nfc") == 0;
}

bool has_bin_extension(const char* path) {
    const char* dot = strrchr(path, '.');
    return dot && strcasecmp(dot, ".bin") == 0;
}

bool load_raw_bin(Storage* storage, const char* path, BrTagDump* dump) {
    File* file = storage_file_alloc(storage);
    if(!file) return false;
    bool ok = false;
    if(storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING) &&
       storage_file_size(file) == BR_TAG_SIZE) {
        uint8_t raw[BR_TAG_SIZE];
        if(storage_file_read(file, raw, sizeof(raw)) == sizeof(raw)) {
            memset(dump, 0, sizeof(*dump));
            for(size_t block = 0; block < BR_BLOCK_COUNT; ++block) {
                memcpy(dump->blocks[block], raw + block * BR_BLOCK_SIZE, BR_BLOCK_SIZE);
                dump->read[block] = true;
            }
            ok = true;
        }
    }
    storage_file_close(file);
    storage_file_free(file);
    return ok;
}
} // namespace

void br_format_uid(const uint8_t uid[BR_UID_SIZE], char out[BR_UID_SIZE * 2U + 1U]) {
    hex_bytes(uid, BR_UID_SIZE, out, BR_UID_SIZE * 2U + 1U);
}

bool br_dump_from_mf(const MfClassicData* data, BrTagDump* dump) {
    if(!data || !dump || data->type != MfClassicType1k) return false;
    memset(dump, 0, sizeof(*dump));
    for(uint8_t block = 0; block < BR_BLOCK_COUNT; ++block) {
        if(mf_classic_is_block_read(data, block)) {
            memcpy(dump->blocks[block], data->block[block].data, BR_BLOCK_SIZE);
            dump->read[block] = true;
        }
    }
    return true;
}

bool br_parse_dump(const BrTagDump* dump, BrTagInfo* info) {
    if(!dump || !info) return false;
    memset(info, 0, sizeof(*info));

    static const uint8_t required[] = {0, 1, 2, 4, 5, 6, 8, 9, 10, 12, 13, 14, 16, 17};
    for(size_t i = 0; i < COUNT_OF(required); ++i) {
        if(!dump->read[required[i]]) return false;
    }

    memcpy(info->uid, dump->blocks[0], BR_UID_SIZE);
    br_format_uid(info->uid, info->uid_hex);

    ascii_field(dump->blocks[1] + 0, 8, info->variant_id, sizeof(info->variant_id));
    ascii_field(dump->blocks[1] + 8, 8, info->material_id, sizeof(info->material_id));
    ascii_field(dump->blocks[2], 16, info->filament_type, sizeof(info->filament_type));
    ascii_field(
        dump->blocks[4], 16, info->detailed_filament_type, sizeof(info->detailed_filament_type));

    memcpy(info->color_rgba, dump->blocks[5], 4);
    rgba_string(info->color_rgba, info->color_hex);
    info->spool_weight_g = le16(dump->blocks[5] + 4);
    info->filament_diameter_mm = le_float(dump->blocks[5] + 8);

    info->drying_temp_c = le16(dump->blocks[6] + 0);
    info->drying_time_h = le16(dump->blocks[6] + 2);
    info->bed_temp_type = le16(dump->blocks[6] + 4);
    info->bed_temp_c = le16(dump->blocks[6] + 6);
    info->hotend_max_c = le16(dump->blocks[6] + 8);
    info->hotend_min_c = le16(dump->blocks[6] + 10);

    hex_bytes(dump->blocks[8], 12, info->xcam_hex, sizeof(info->xcam_hex));
    info->nozzle_diameter_mm = le_float(dump->blocks[8] + 12);
    hex_bytes(dump->blocks[9], 16, info->tray_uid_hex, sizeof(info->tray_uid_hex));
    info->spool_width_mm = (float)le16(dump->blocks[10] + 4) / 100.0f;
    ascii_field(dump->blocks[12], 16, info->production_date, sizeof(info->production_date));
    ascii_field(dump->blocks[13], 16, info->short_date, sizeof(info->short_date));
    info->filament_length_m = le16(dump->blocks[14] + 4);

    if(dump->blocks[16][0] == 0x02 && dump->blocks[16][1] == 0x00) {
        info->color_count = (uint8_t)le16(dump->blocks[16] + 2);
    } else {
        info->color_count = 1;
    }
    if(info->color_count == 2) {
        // Block 16 stores the second color in reverse ABGR order.
        info->second_color_rgba[0] = dump->blocks[16][7];
        info->second_color_rgba[1] = dump->blocks[16][6];
        info->second_color_rgba[2] = dump->blocks[16][5];
        info->second_color_rgba[3] = dump->blocks[16][4];
        rgba_string(info->second_color_rgba, info->second_color_hex);
    }

    hex_bytes(dump->blocks[17], 2, info->block17_hex, sizeof(info->block17_hex));

    info->complete = true;
    for(uint8_t block = 0; block < BR_BLOCK_COUNT; ++block) {
        if(!dump->read[block]) {
            info->complete = false;
            break;
        }
    }

    // A successful UID-derived read plus the expected Bambu material layout is a strong identifier.
    info->valid = info->uid_hex[0] != '\0' && info->filament_type[0] != '\0' &&
                  info->material_id[0] != '\0';
    return info->valid;
}

bool br_parse_mf(const MfClassicData* data, BrTagInfo* info) {
    BrTagDump dump;
    return br_dump_from_mf(data, &dump) && br_parse_dump(&dump, info);
}

bool br_load_tag_file(Storage* storage, const char* path, BrTagInfo* info) {
    if(!storage || !path || !info) return false;

    if(has_nfc_extension(path)) {
        NfcDevice* device = nfc_device_alloc();
        if(!device) return false;
        bool ok = nfc_device_load(device, path);
        if(ok && nfc_device_get_protocol(device) == NfcProtocolMfClassic) {
            const MfClassicData* data = static_cast<const MfClassicData*>(
                nfc_device_get_data(device, NfcProtocolMfClassic));
            ok = br_parse_mf(data, info);
        } else {
            ok = false;
        }
        nfc_device_free(device);
        return ok;
    }

    if(has_bin_extension(path)) {
        BrTagDump dump;
        return load_raw_bin(storage, path, &dump) && br_parse_dump(&dump, info);
    }

    return false;
}
