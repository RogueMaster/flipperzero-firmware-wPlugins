#include "bambu_export.h"
#include "bambu_crypto.h"
#include "bambu_tag.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

namespace {
constexpr uint8_t BlocksPerSector = 4U;

const char* DataAccessText[] = {
    "read AB; write AB; increment AB; decrement transfer restore AB",
    "read AB; decrement transfer restore AB",
    "read AB",
    "read B; write B",
    "read AB; writeB",
    "read B",
    "read AB; write B; increment B; decrement transfer restore AB",
    "none",
};

const char* TrailerAccessText[] = {
    "read A by A; read ACCESS by A; read B by A; write B by A",
    "write A by A; read ACCESS by A write ACCESS by A; read B by A; write B by A",
    "read ACCESS by A; read B by A",
    "write A by B; read ACCESS by AB; write ACCESS by B; write B by B",
    "write A by B; read ACCESS by AB; write B by B",
    "read ACCESS by AB; write ACCESS by B",
    "read ACCESS by AB",
    "read ACCESS by AB",
};

bool write_bytes(File* file, const void* data, size_t size) {
    return size == 0U || storage_file_write(file, data, size) == size;
}

bool write_text(File* file, const char* text) {
    return text && write_bytes(file, text, strlen(text));
}

bool write_format(File* file, const char* format, ...) {
    char buffer[512];
    va_list args;
    va_start(args, format);
    const int length = vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    if(length < 0 || static_cast<size_t>(length) >= sizeof(buffer)) return false;
    return write_bytes(file, buffer, static_cast<size_t>(length));
}

bool open_output(Storage* storage, const char* path, File** out_file) {
    if(!storage || !path || !out_file) return false;
    File* file = storage_file_alloc(storage);
    if(!file) return false;
    if(!storage_file_open(file, path, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_free(file);
        return false;
    }
    *out_file = file;
    return true;
}

void close_output(File* file) {
    if(!file) return;
    storage_file_close(file);
    storage_file_free(file);
}

void hex_compact(const uint8_t* data, size_t size, char* out, size_t out_size) {
    static const char Hex[] = "0123456789ABCDEF";
    if(!out || out_size == 0U) return;
    size_t cursor = 0;
    for(size_t i = 0; i < size && cursor + 2U < out_size; ++i) {
        out[cursor++] = Hex[data[i] >> 4];
        out[cursor++] = Hex[data[i] & 0x0FU];
    }
    out[cursor] = '\0';
}

void hex_spaced(const uint8_t* data, size_t size, char* out, size_t out_size) {
    static const char Hex[] = "0123456789ABCDEF";
    if(!out || out_size == 0U) return;
    size_t cursor = 0;
    for(size_t i = 0; i < size; ++i) {
        if(cursor + 2U >= out_size) break;
        out[cursor++] = Hex[data[i] >> 4];
        out[cursor++] = Hex[data[i] & 0x0FU];
        if(i + 1U < size) {
            if(cursor + 1U >= out_size) break;
            out[cursor++] = ' ';
        }
    }
    out[cursor] = '\0';
}

uint8_t trailer_block(uint8_t sector) {
    return static_cast<uint8_t>(sector * BlocksPerSector + 3U);
}

uint8_t access_code(const uint8_t access[4], uint8_t block_in_sector) {
    const uint8_t c1 = static_cast<uint8_t>((access[1] >> (4U + block_in_sector)) & 0x01U);
    const uint8_t c2 = static_cast<uint8_t>((access[2] >> block_in_sector) & 0x01U);
    const uint8_t c3 = static_cast<uint8_t>((access[2] >> (4U + block_in_sector)) & 0x01U);
    return static_cast<uint8_t>((c1 << 2U) | (c2 << 1U) | c3);
}

bool make_file_path(
    const char* folder,
    const char* uid,
    const char* suffix,
    char* out,
    size_t out_size) {
    const int length = snprintf(out, out_size, "%s/hf-mf-%s%s", folder, uid, suffix);
    return length > 0 && static_cast<size_t>(length) < out_size;
}

bool write_dump_bin(Storage* storage, const char* path, const BrTagDump& dump) {
    File* file = nullptr;
    if(!open_output(storage, path, &file)) return false;

    bool ok = true;
    for(uint8_t block = 0; block < BR_BLOCK_COUNT && ok; ++block) {
        ok = dump.read[block] && write_bytes(file, dump.blocks[block], BR_BLOCK_SIZE);
    }

    close_output(file);
    return ok;
}

bool write_key_bin(Storage* storage, const char* path, const BrTagDump& dump) {
    File* file = nullptr;
    if(!open_output(storage, path, &file)) return false;

    bool ok = true;
    // Bambu-Lab-RFID-Library/convert.py writes all A keys first, then all B keys.
    for(uint8_t sector = 0; sector < BR_SECTOR_COUNT && ok; ++sector) {
        const uint8_t* trailer = dump.blocks[trailer_block(sector)];
        ok = write_bytes(file, trailer, MF_CLASSIC_KEY_SIZE);
    }
    for(uint8_t sector = 0; sector < BR_SECTOR_COUNT && ok; ++sector) {
        const uint8_t* trailer = dump.blocks[trailer_block(sector)];
        ok = write_bytes(file, trailer + 10U, MF_CLASSIC_KEY_SIZE);
    }

    close_output(file);
    return ok;
}

bool write_dump_json(Storage* storage, const char* path, const BrTagDump& dump) {
    File* file = nullptr;
    if(!open_output(storage, path, &file)) return false;

    char uid[9];
    char atqa[5];
    char sak[3];
    char block_hex[BR_BLOCK_SIZE * 2U + 1U];
    char key_a[MF_CLASSIC_KEY_SIZE * 2U + 1U];
    char key_b[MF_CLASSIC_KEY_SIZE * 2U + 1U];
    char access_hex[9];
    br_format_uid(dump.blocks[0], uid);
    hex_compact(dump.blocks[0] + 6U, 2U, atqa, sizeof(atqa));
    hex_compact(dump.blocks[0] + 5U, 1U, sak, sizeof(sak));

    bool ok = write_text(file, "{\n") &&
              write_format(file, "  \"Created\": \"%s %s\",\n", BR_APP_NAME, BR_APP_VERSION) &&
              write_text(file, "  \"FileType\": \"mfc v2\",\n") &&
              write_text(file, "  \"Card\": {\n") &&
              write_format(file, "    \"UID\": \"%s\",\n", uid) &&
              write_format(file, "    \"ATQA\": \"%s\",\n", atqa) &&
              write_format(file, "    \"SAK\": \"%s\"\n", sak) && write_text(file, "  },\n") &&
              write_text(file, "  \"blocks\": {\n");

    for(uint8_t block = 0; block < BR_BLOCK_COUNT && ok; ++block) {
        hex_compact(dump.blocks[block], BR_BLOCK_SIZE, block_hex, sizeof(block_hex));
        ok = write_format(
            file,
            "    \"%u\": \"%s\"%s\n",
            static_cast<unsigned int>(block),
            block_hex,
            block + 1U < BR_BLOCK_COUNT ? "," : "");
    }

    ok = ok && write_text(file, "  },\n  \"SectorKeys\": {\n");
    for(uint8_t sector = 0; sector < BR_SECTOR_COUNT && ok; ++sector) {
        const uint8_t* trailer = dump.blocks[trailer_block(sector)];
        hex_compact(trailer, MF_CLASSIC_KEY_SIZE, key_a, sizeof(key_a));
        hex_compact(trailer + 10U, MF_CLASSIC_KEY_SIZE, key_b, sizeof(key_b));
        hex_compact(trailer + 6U, 4U, access_hex, sizeof(access_hex));

        ok = write_format(file, "    \"%u\": {\n", static_cast<unsigned int>(sector)) &&
             write_format(file, "      \"KeyA\": \"%s\",\n", key_a) &&
             write_format(file, "      \"KeyB\": \"%s\",\n", key_b) &&
             write_format(file, "      \"AccessConditions\": \"%s\",\n", access_hex) &&
             write_text(file, "      \"AccessConditionsText\": {\n");

        for(uint8_t local_block = 0; local_block < BlocksPerSector && ok; ++local_block) {
            const uint8_t code = access_code(trailer + 6U, local_block);
            const uint8_t absolute_block =
                static_cast<uint8_t>(sector * BlocksPerSector + local_block);
            const char* text = local_block == 3U ? TrailerAccessText[code] : DataAccessText[code];
            ok = write_format(
                file,
                "        \"block%u\": \"%s\",\n",
                static_cast<unsigned int>(absolute_block),
                text);
        }
        ok =
            ok &&
            write_format(
                file, "        \"UserData\": \"%02X\"\n", static_cast<unsigned int>(trailer[9])) &&
            write_text(file, "      }\n") &&
            write_format(file, "    }%s\n", sector + 1U < BR_SECTOR_COUNT ? "," : "");
    }

    ok = ok && write_text(file, "  }\n}\n");
    close_output(file);
    return ok;
}

bool write_flipper_nfc(Storage* storage, const char* path, const BrTagDump& dump) {
    File* file = nullptr;
    if(!open_output(storage, path, &file)) return false;

    char uid[12];
    char atqa[6];
    char sak[3];
    char block_hex[BR_BLOCK_SIZE * 3U];
    hex_spaced(dump.blocks[0], BR_UID_SIZE, uid, sizeof(uid));
    const uint8_t reversed_atqa[2] = {dump.blocks[0][7], dump.blocks[0][6]};
    hex_spaced(reversed_atqa, sizeof(reversed_atqa), atqa, sizeof(atqa));
    hex_compact(dump.blocks[0] + 5U, 1U, sak, sizeof(sak));

    bool ok =
        write_text(file, "Filetype: Flipper NFC device\n") && write_text(file, "Version: 4\n") &&
        write_text(
            file,
            "# Device type can be ISO14443-3A, ISO14443-3B, ISO14443-4A, ISO14443-4B, ISO15693-3, FeliCa, NTAG/Ultralight, Mifare Classic, Mifare Plus, Mifare DESFire, SLIX, ST25TB, EMV\n") &&
        write_text(file, "Device type: Mifare Classic\n") &&
        write_text(file, "# UID is common for all formats\n") &&
        write_format(file, "UID: %s\n", uid) &&
        write_text(file, "# ISO14443-3A specific data\n") &&
        write_format(file, "ATQA: %s\n", atqa) && write_format(file, "SAK: %s\n", sak) &&
        write_text(file, "# Mifare Classic specific data\n") &&
        write_text(file, "Mifare Classic type: 1K\n") &&
        write_text(file, "Data format version: 2\n") &&
        write_text(
            file,
            "# Mifare Classic blocks, '?"
            "?' means unknown data\n");

    for(uint8_t block = 0; block < BR_BLOCK_COUNT && ok; ++block) {
        hex_spaced(dump.blocks[block], BR_BLOCK_SIZE, block_hex, sizeof(block_hex));
        ok = write_format(file, "Block %u: %s\n", static_cast<unsigned int>(block), block_hex);
    }

    close_output(file);
    return ok;
}

void format_fixed_2(char* out, size_t out_size, float value) {
    const uint32_t scaled = static_cast<uint32_t>(value * 100.0f + 0.5f);
    snprintf(
        out,
        out_size,
        "%lu.%02lu",
        static_cast<unsigned long>(scaled / 100U),
        static_cast<unsigned long>(scaled % 100U));
}

void format_fixed_1(char* out, size_t out_size, float value) {
    const uint32_t scaled = static_cast<uint32_t>(value * 10.0f + 0.5f);
    snprintf(
        out,
        out_size,
        "%lu.%01lu",
        static_cast<unsigned long>(scaled / 10U),
        static_cast<unsigned long>(scaled % 10U));
}

bool write_readme(Storage* storage, const char* path, const BrTagInfo& info) {
    File* file = nullptr;
    if(!open_output(storage, path, &file)) return false;

    char diameter[16];
    char spool_width[16];
    char nozzle[16];
    format_fixed_2(diameter, sizeof(diameter), info.filament_diameter_mm);
    format_fixed_2(spool_width, sizeof(spool_width), info.spool_width_mm);
    format_fixed_1(nozzle, sizeof(nozzle), info.nozzle_diameter_mm);

    bool ok =
        write_format(file, "# Bambu RFID Tag %s\n\n", info.uid_hex) &&
        write_format(file, "Generated by %s %s.\n\n", BR_APP_NAME, BR_APP_VERSION) &&
        write_text(file, "## Tag information\n\n") &&
        write_format(file, "- UID: `%s`\n", info.uid_hex) &&
        write_format(file, "- Filament type: `%s`\n", info.filament_type) &&
        write_format(file, "- Detailed filament type: `%s`\n", info.detailed_filament_type) &&
        write_format(file, "- Material ID: `%s`\n", info.material_id) &&
        write_format(file, "- Variant ID: `%s`\n", info.variant_id) &&
        write_format(file, "- Primary color (RGBA): `%s`\n", info.color_hex) &&
        (info.color_count != 2U ||
         write_format(file, "- Secondary color (RGBA): `%s`\n", info.second_color_hex)) &&
        write_format(file, "- Color count: `%u`\n", static_cast<unsigned int>(info.color_count)) &&
        write_format(
            file, "- Spool weight: `%u g`\n", static_cast<unsigned int>(info.spool_weight_g)) &&
        write_format(file, "- Filament diameter: `%s mm`\n", diameter) &&
        write_format(
            file,
            "- Filament length: `%u m`\n",
            static_cast<unsigned int>(info.filament_length_m)) &&
        write_format(file, "- Spool width: `%s mm`\n", spool_width) &&
        write_format(file, "- Nozzle diameter: `%s mm`\n", nozzle) &&
        write_format(
            file,
            "- Hotend range: `%u-%u C`\n",
            static_cast<unsigned int>(info.hotend_min_c),
            static_cast<unsigned int>(info.hotend_max_c)) &&
        write_format(
            file,
            "- Bed temperature: `%u C` (type `%u`)\n",
            static_cast<unsigned int>(info.bed_temp_c),
            static_cast<unsigned int>(info.bed_temp_type)) &&
        write_format(
            file,
            "- Drying: `%u C` for `%u h`\n",
            static_cast<unsigned int>(info.drying_temp_c),
            static_cast<unsigned int>(info.drying_time_h)) &&
        write_format(file, "- Production timestamp: `%s`\n", info.production_date) &&
        write_format(file, "- Short production date: `%s`\n", info.short_date) &&
        write_format(file, "- Tray UID: `%s`\n", info.tray_uid_hex) &&
        write_format(file, "- XCam data: `%s`\n", info.xcam_hex) &&
        write_format(file, "- Block 17 prefix: `%s`\n", info.block17_hex) &&
        write_format(file, "- Complete 1K dump: `%s`\n\n", info.complete ? "yes" : "no") &&
        write_text(file, "## Files\n\n") &&
        write_format(
            file, "- `hf-mf-%s-dump.bin` - raw 1024-byte MIFARE Classic dump\n", info.uid_hex) &&
        write_format(
            file, "- `hf-mf-%s-dump.json` - Proxmark-style mfc v2 JSON dump\n", info.uid_hex) &&
        write_format(
            file,
            "- `hf-mf-%s-key.bin` - 16 Key A values followed by 16 Key B values\n",
            info.uid_hex) &&
        write_format(file, "- `hf-mf-%s.nfc` - Flipper NFC device file\n", info.uid_hex) &&
        write_text(file, "- `README.md` - parsed tag summary\n");

    close_output(file);
    return ok;
}
} // namespace

bool br_save_tag_bundle(
    Storage* storage,
    const MfClassicData* data,
    const BrTagInfo* info,
    char* out_path,
    size_t out_path_size) {
    if(!storage || !data || !info || !out_path || out_path_size == 0U) return false;

    BrTagDump dump;
    if(!br_dump_from_mf(data, &dump) || !info->complete) return false;

    // Always stamp the UID-derived keys into each sector trailer before export.
    // This keeps dump.bin/JSON/NFC/key.bin byte-consistent even if a firmware
    // read path reports the trailer access bytes without materializing key bytes.
    MfClassicDeviceKeys derived_keys = {};
    if(!br_derive_sector_keys(info->uid, &derived_keys)) return false;
    for(uint8_t sector = 0; sector < BR_SECTOR_COUNT; ++sector) {
        uint8_t* trailer = dump.blocks[trailer_block(sector)];
        memcpy(trailer, derived_keys.key_a[sector].data, MF_CLASSIC_KEY_SIZE);
        memcpy(trailer + 10U, derived_keys.key_b[sector].data, MF_CLASSIC_KEY_SIZE);
    }

    storage_common_mkdir(storage, BR_DATA_DIR);
    storage_common_mkdir(storage, BR_TAGS_DIR);

    char folder[BR_PATH_MAX];
    int length = snprintf(folder, sizeof(folder), "%s/%s", BR_TAGS_DIR, info->uid_hex);
    if(length <= 0 || static_cast<size_t>(length) >= sizeof(folder)) return false;
    storage_common_mkdir(storage, folder);

    char path[BR_PATH_MAX];
    if(!make_file_path(folder, info->uid_hex, "-dump.bin", path, sizeof(path)) ||
       !write_dump_bin(storage, path, dump))
        return false;
    if(!make_file_path(folder, info->uid_hex, "-dump.json", path, sizeof(path)) ||
       !write_dump_json(storage, path, dump))
        return false;
    if(!make_file_path(folder, info->uid_hex, "-key.bin", path, sizeof(path)) ||
       !write_key_bin(storage, path, dump))
        return false;
    if(!make_file_path(folder, info->uid_hex, ".nfc", path, sizeof(path)) ||
       !write_flipper_nfc(storage, path, dump))
        return false;

    length = snprintf(path, sizeof(path), "%s/README.md", folder);
    if(length <= 0 || static_cast<size_t>(length) >= sizeof(path) ||
       !write_readme(storage, path, *info))
        return false;

    length = snprintf(out_path, out_path_size, "%s", folder);
    return length > 0 && static_cast<size_t>(length) < out_path_size;
}
