#include "mfc.h"

#include <protocols/mf_classic/mf_classic.h>

#define SAFLOK_MFC_VARKEY_START 16
#define SAFLOK_MFC_VARKEY_SIZE  6

const uint8_t SAFLOK_MFC_GLOBAL_KEY[6] = {0x2a, 0x2c, 0x13, 0xcc, 0x24, 0x2a};

void saflok_mfc_generate_key(const uint8_t* uid, uint8_t sector, uint8_t* key) {
    if(sector == 1) {
        // Specifically ONLY sector 1 uses a global key
        memcpy(key, SAFLOK_MFC_GLOBAL_KEY, 6);
        return;
    }

    // Everything else uses a per-UID diversified key
    static const uint8_t magic_table[192] = {
        0x00, 0x00, 0xAA, 0x00, 0x00, 0x00, 0xF0, 0x57, 0xB3, 0x9E, 0xE3, 0xD8, 0x00, 0x00, 0xAA,
        0x00, 0x00, 0x00, 0x96, 0x9D, 0x95, 0x4A, 0xC1, 0x57, 0x00, 0x00, 0xAA, 0x00, 0x00, 0x00,
        0x8F, 0x43, 0x58, 0x0D, 0x2C, 0x9D, 0x00, 0x00, 0xAA, 0x00, 0x00, 0x00, 0xFF, 0xCC, 0xE0,
        0x05, 0x0C, 0x43, 0x00, 0x00, 0xAA, 0x00, 0x00, 0x00, 0x34, 0x1B, 0x15, 0xA6, 0x90, 0xCC,
        0x00, 0x00, 0xAA, 0x00, 0x00, 0x00, 0x89, 0x58, 0x56, 0x12, 0xE7, 0x1B, 0x00, 0x00, 0xAA,
        0x00, 0x00, 0x00, 0xBB, 0x74, 0xB0, 0x95, 0x36, 0x58, 0x00, 0x00, 0xAA, 0x00, 0x00, 0x00,
        0xFB, 0x97, 0xF8, 0x4B, 0x5B, 0x74, 0x00, 0x00, 0xAA, 0x00, 0x00, 0x00, 0xC9, 0xD1, 0x88,
        0x35, 0x9F, 0x92, 0x00, 0x00, 0xAA, 0x00, 0x00, 0x00, 0x8F, 0x92, 0xE9, 0x7F, 0x58, 0x97,
        0x00, 0x00, 0xAA, 0x00, 0x00, 0x00, 0x16, 0x6C, 0xA2, 0xB0, 0x9F, 0xD1, 0x00, 0x00, 0xAA,
        0x00, 0x00, 0x00, 0x27, 0xDD, 0x93, 0x10, 0x1C, 0x6C, 0x00, 0x00, 0xAA, 0x00, 0x00, 0x00,
        0xDA, 0x3E, 0x3F, 0xD6, 0x49, 0xDD, 0x00, 0x00, 0xAA, 0x00, 0x00, 0x00, 0x58, 0xDD, 0xED,
        0x07, 0x8E, 0x3E, 0x00, 0x00, 0xAA, 0x00, 0x00, 0x00, 0x5C, 0xD0, 0x05, 0xCF, 0xD9, 0x07,
        0x00, 0x00, 0xAA, 0x00, 0x00, 0x00, 0x11, 0x8D, 0xD0, 0x01, 0x87, 0xD0};

    uint8_t magic_byte = (uid[3] >> 4) + (uid[2] >> 4) + (uid[0] & 0x0F);
    uint8_t magickal_index = (magic_byte & 0x0F) * 12 + 11;

    uint8_t temp_key[6] = {magic_byte, uid[0], uid[1], uid[2], uid[3], magic_byte};
    uint8_t carry_sum = 0;

    for(int i = 6 - 1; i >= 0; i--, magickal_index--) {
        uint16_t keysum = temp_key[i] + magic_table[magickal_index] + carry_sum;
        temp_key[i] = (keysum & 0xFF);
        carry_sum = keysum >> 8;
    }

    memcpy(key, temp_key, 6);
}

void saflok_generate_mf_classic(
    NfcDevice* nfc_device,
    uint8_t* uid,
    size_t uid_len,
    BasicAccessData* saflok_data) {
    MfClassicData* mfc_data = mf_classic_alloc();

    mf_classic_set_uid(mfc_data, uid, uid_len);

    // Set up manufacturer block
    mfc_data->iso14443_3a_data->uid_len = uid_len;
    mfc_data->iso14443_3a_data->atqa[0] = 0x04;
    mfc_data->iso14443_3a_data->atqa[1] = 0x00;
    mfc_data->iso14443_3a_data->sak = 0x08;
    mfc_data->type = MfClassicType1k;
    mf_classic_set_block_read(mfc_data, 0, &mfc_data->block[0]);

    // Fill the remaining blocks
    uint16_t block_num = mf_classic_get_total_block_num(MfClassicType1k);
    for(uint16_t block = 1; block < block_num; block++) {
        if(mf_classic_is_sector_trailer(block)) {
            MfClassicSectorTrailer* sec_tr = (MfClassicSectorTrailer*)mfc_data->block[block].data;
            sec_tr->access_bits.data[0] = 0xFF;
            sec_tr->access_bits.data[1] = 0x07;
            sec_tr->access_bits.data[2] = 0x80;
            sec_tr->access_bits.data[3] = 0x69;

            // Generate diversified key from UID
            uint8_t key[6];
            saflok_mfc_generate_key(uid, mf_classic_get_sector_by_block(block), key);
            uint64_t sector_key = bit_lib_bytes_to_num_be(key, 6);

            mf_classic_set_block_read(mfc_data, block, &mfc_data->block[block]);
            mf_classic_set_key_found(
                mfc_data, mf_classic_get_sector_by_block(block), MfClassicKeyTypeA, sector_key);
            mf_classic_set_key_found(
                mfc_data, mf_classic_get_sector_by_block(block), MfClassicKeyTypeB, 0xFFFFFFFFFFFF);

        } else {
            memset(&mfc_data->block[block].data, 0x00, MF_CLASSIC_BLOCK_SIZE);
        }

        mf_classic_set_block_read(mfc_data, block, &mfc_data->block[block]);

        // This is the default log header for cards with no log data
        // 00 00 00 00 00 00 00 00   00 00 00 C1 00 00 00 00
        if(block == 4) {
            mfc_data->block[block].data[11] = 0xC1;
        }
    }

    uint8_t basicAccess[BASIC_ACCESS_BYTE_NUM];
    saflok_generate_data(saflok_data, basicAccess);

    // Saflok data is stored in block 1 and the first byte of block 2
    memcpy(mfc_data->block[1].data, basicAccess, 16);
    mfc_data->block[2].data[0] = basicAccess[16];

    // TODO: Figure out what these actually mean
    mfc_data->block[2].data[2] = 0x04;
    mfc_data->block[2].data[4] = 0x01;

    nfc_device_set_data(nfc_device, NfcProtocolMfClassic, mfc_data);
    mf_classic_free(mfc_data);
}

bool saflok_parse_mf_classic(
    NfcDevice* nfc_device,
    uint8_t* uid,
    size_t* uid_len,
    BasicAccessData* saflok_data) {
    // Get MFC data and UID
    const MfClassicData* mfc_data = nfc_device_get_data(nfc_device, NfcProtocolMfClassic);
    memcpy(uid, nfc_device_get_uid(nfc_device, uid_len), *uid_len);

    // Data is stored in blocks 1 and 2
    uint8_t data[BASIC_ACCESS_BYTE_NUM];
    memcpy(data, mfc_data->block[1].data, 16);
    data[16] = mfc_data->block[2].data[0];

    uint8_t basicAccess[BASIC_ACCESS_BYTE_NUM];
    saflok_decrypt_card(data, BASIC_ACCESS_BYTE_NUM, basicAccess);

    // Validate checksum before parsing
    if(saflok_calculate_checksum(basicAccess) != basicAccess[16]) {
        return false;
    }

    saflok_data->format = SaflipFormatMifareClassic;
    saflok_parse_basic_access(basicAccess, saflok_data);

    return true;
}

void saflok_generate_mf_classic_logs(
    NfcDevice* nfc_device,
    LogEntry* log_entries,
    size_t num_log_entries) {
    // Get a copy of the existing MFC data so we can edit it
    MfClassicData* mfc_data = mf_classic_alloc();
    mf_classic_copy(mfc_data, nfc_device_get_data(nfc_device, NfcProtocolMfClassic));

    // Block 2, byte 5 is the number of bytes before the beginning of logs
    size_t log_start_offset = mfc_data->block[2].data[5];
    size_t block_idx = SAFLOK_MFC_VARKEY_START + (log_start_offset / 16);
    uint8_t half = (log_start_offset % 16) ? 1 : 0;

    for(size_t i = 0; i < num_log_entries; i++) {
        LogEntry entry = log_entries[i];
        uint8_t data[SAFLOK_LOG_SIZE];
        saflok_encode_log_entry(entry, data);

        // Skip sector trailers
        if(mf_classic_is_sector_trailer(block_idx)) {
            block_idx++;
            half = 0;
        }

        // Write log data to half
        memcpy(mfc_data->block[block_idx].data + (SAFLOK_LOG_SIZE * half), data, SAFLOK_LOG_SIZE);

        // Move to next half
        if(++half > 1) {
            half = 0;
            block_idx++;
        }
    }

    // If we landed on a sector trailer, skip it too
    if(mf_classic_is_sector_trailer(block_idx)) {
        block_idx++;
    }

    // TODO: Figure out what of this is correct and needed
    // This is almost certainly correct
    mfc_data->block[4].data[0] = 0x20 + block_idx;
    mfc_data->block[5].data[0] = 0x20 + block_idx;

    // This is almost certainly correct
    mfc_data->block[4].data[2] = num_log_entries;
    mfc_data->block[5].data[2] = num_log_entries;

    // This is definitely NOT correct
    // mfc_data->block[4].data[15] = num_log_entries / 2;
    // mfc_data->block[5].data[15] = num_log_entries / 2;

    // Write updated data
    nfc_device_set_data(nfc_device, NfcProtocolMfClassic, mfc_data);
    mf_classic_free(mfc_data);
}

size_t saflok_parse_mf_classic_logs(NfcDevice* nfc_device, LogEntry log_entries[]) {
    const MfClassicData* mfc_data = nfc_device_get_data(nfc_device, NfcProtocolMfClassic);

    // Block 2, byte 5 is the number of bytes before the beginning of logs
    size_t log_start_offset = mfc_data->block[2].data[5];
    size_t total_entries = 0;

    uint8_t empty_log_entry[SAFLOK_LOG_SIZE];
    memset(empty_log_entry, 0x00, SAFLOK_LOG_SIZE);

    // Search for and parse new log entries
    size_t total_blocks = mf_classic_get_total_block_num(mfc_data->type);
    for(size_t blk = SAFLOK_MFC_VARKEY_START; blk > 0 && blk < total_blocks; blk++) {
        // Skip sector trailers
        if(mf_classic_is_sector_trailer(blk)) continue;

        // Read each half as a separate log entry
        for(uint8_t half = 0; half < 2; half++) {
            // Skip if we're before the start of the log offset
            size_t half_idx = (blk - SAFLOK_MFC_VARKEY_START) * 2 + half;
            if(half_idx * SAFLOK_LOG_SIZE < log_start_offset) continue;

            // Fetch this half's data
            uint8_t log[SAFLOK_LOG_SIZE];
            memcpy(log, mfc_data->block[blk].data + (SAFLOK_LOG_SIZE * half), SAFLOK_LOG_SIZE);

            if(memcmp(log, empty_log_entry, SAFLOK_LOG_SIZE) == 0) {
                // End of log entries detected, reset to break outer loop
                // Set to -1 so when the loop increments, it becomes 0
                blk = -1;
                break;
            }

            bool valid = saflok_decode_log_entry(log, log_entries + total_entries);
            // Only increment counter if the entry was
            // valid, so we don't store invalid entries
            if(valid) total_entries++;
        }
    }

    return total_entries;
}

void saflok_generate_mf_classic_variable_keys(
    NfcDevice* nfc_device,
    VariableKey* variable_keys,
    size_t num_variable_keys,
    VariableKeysOptionalFunction optional_function) {
    // Get a copy of the existing MFC data so we can edit it
    MfClassicData* mfc_data = mf_classic_alloc();
    mf_classic_copy(mfc_data, nfc_device_get_data(nfc_device, NfcProtocolMfClassic));

    size_t variable_key_bytes = 0;

    uint8_t data[SAFLOK_MFC_VARKEY_SIZE];
    size_t block_idx = SAFLOK_MFC_VARKEY_START;
    uint8_t block_offset = 0;
    for(size_t idx = 0; idx < num_variable_keys; idx++) {
        VariableKey key = variable_keys[idx];
        memset(data, 0, SAFLOK_MFC_VARKEY_SIZE);

        // Add lock ID
        data[0] = (key.lock_id >> 8) & 0x3F;
        data[1] = key.lock_id & 0xFF;

        // Add flags
        if(key.inhibit) data[0] |= 0x80;
        if(key.use_optional) data[0] |= 0x40;

        // Add creation date/time
        uint16_t creation_year = key.creation.year - SAFLOK_YEAR_OFFSET;
        data[5] |= creation_year & 0x70;
        data[2] = (creation_year << 4) & 0xF0;
        data[2] |= key.creation.month & 0x0F;
        data[3] = (key.creation.day << 3) & 0xF8;
        data[3] |= (key.creation.hour >> 2) & 0x07;
        data[4] = (key.creation.hour << 6) & 0xC0;
        data[4] |= key.creation.minute & 0x3F;

        if(mf_classic_is_sector_trailer(block_idx)) {
            variable_key_bytes += 16;
            block_idx++;
        }

        uint8_t split = 16 - block_offset;
        if(split > SAFLOK_MFC_VARKEY_SIZE) split = SAFLOK_MFC_VARKEY_SIZE;
        memcpy(mfc_data->block[block_idx].data + block_offset, data, split);

        // Update write position
        block_offset += split;
        if(block_offset >= 16) {
            block_offset -= 16;
            block_idx++;
            // If we entered a sector trailer, skip it
            if(mf_classic_is_sector_trailer(block_idx)) {
                variable_key_bytes += 16;
                block_idx++;
            }
        }

        // If the data wraps around into the next block, write that as well
        if(split < SAFLOK_MFC_VARKEY_SIZE) {
            memcpy(mfc_data->block[block_idx].data, data + split, SAFLOK_MFC_VARKEY_SIZE - split);
            block_offset += SAFLOK_MFC_VARKEY_SIZE - split;
        }

        variable_key_bytes += SAFLOK_MFC_VARKEY_SIZE;
    }

    // Round up to nearest half-block
    while(variable_key_bytes % 8 != 0)
        variable_key_bytes++;

    // If it would start in a sector trailer, move to next block
    if(mf_classic_is_sector_trailer(SAFLOK_MFC_VARKEY_START + (variable_key_bytes / 16))) {
        variable_key_bytes += 16;
    }

    // Write the log offset to block 2, byte 5
    mfc_data->block[2].data[5] = variable_key_bytes;

    // TODO: Figure out where this goes and how it's formatted
    // These values are based on what I saw change, but no idea whether they're correct.
    switch(optional_function) {
    case VariableKeysOptionalFunctionNone:
        mfc_data->block[2].data[6] = 0x00;
        break;
    case VariableKeysOptionalFunctionLevelInhibit:
        mfc_data->block[2].data[6] = 0x21;
        break;
    case VariableKeysOptionalFunctionElectLockUnlock:
        mfc_data->block[2].data[6] = 0x88;
        break;
    case VariableKeysOptionalFunctionLatchUnlatch:
        mfc_data->block[2].data[6] = 0xA8;
        break;
    default:
        break;
    }

    // Write updated data
    nfc_device_set_data(nfc_device, NfcProtocolMfClassic, mfc_data);
    mf_classic_free(mfc_data);
}

size_t saflok_parse_mf_classic_variable_keys(
    NfcDevice* nfc_device,
    VariableKey* variable_keys,
    VariableKeysOptionalFunction* optional_function) {
    const MfClassicData* mfc_data = nfc_device_get_data(nfc_device, NfcProtocolMfClassic);

    // Block 2, byte 5 is the number of bytes before the beginning of logs
    size_t remaining_varkey_bytes = mfc_data->block[2].data[5];
    size_t total_keys = 0;

    uint8_t empty_var_key[SAFLOK_MFC_VARKEY_SIZE];
    memset(empty_var_key, 0x00, SAFLOK_MFC_VARKEY_SIZE);

    // Seach for and parse variable keys
    uint8_t variable_key_data[SAFLOK_MFC_VARKEY_SIZE];
    size_t block_idx = SAFLOK_MFC_VARKEY_START;
    uint8_t block_offset = 0;
    while(remaining_varkey_bytes >= SAFLOK_MFC_VARKEY_SIZE) {
        if(mf_classic_is_sector_trailer(block_idx)) {
            remaining_varkey_bytes -= 16;
            block_idx++;
        }

        uint8_t split = 16 - block_offset;
        if(split > SAFLOK_MFC_VARKEY_SIZE) split = SAFLOK_MFC_VARKEY_SIZE;
        memcpy(variable_key_data, mfc_data->block[block_idx].data + block_offset, split);

        // Update read position
        block_offset += split;
        if(block_offset >= 16) {
            block_offset -= 16;
            block_idx++;
            // If we entered a sector trailer, skip it
            if(mf_classic_is_sector_trailer(block_idx)) {
                remaining_varkey_bytes -= 16;
                block_idx++;
            }
        }

        // If the data wraps around into the next block, read that as well
        if(split < SAFLOK_MFC_VARKEY_SIZE) {
            memcpy(
                variable_key_data + split,
                mfc_data->block[block_idx].data,
                SAFLOK_MFC_VARKEY_SIZE - split);
            block_offset += SAFLOK_MFC_VARKEY_SIZE - split;
        }

        // Only parse and store varkey if it's not empty
        if(memcmp(variable_key_data, empty_var_key, SAFLOK_MFC_VARKEY_SIZE) != 0) {
            DateTime time;
            uint8_t creation_year_bits = (variable_key_data[5] & 0x70);
            time.year = (creation_year_bits | ((variable_key_data[2] & 0xF0) >> 4)) + 1980;
            time.month = variable_key_data[2] & 0x0F;
            time.day = (variable_key_data[3] >> 3) & 0x1F;
            time.hour = ((variable_key_data[3] & 0x07) << 2) | (variable_key_data[4] >> 6);
            time.minute = variable_key_data[4] & 0x3F;

            uint16_t lock_id = ((variable_key_data[0] & 0x3F) << 8) | variable_key_data[1];

            bool inhibit = (variable_key_data[0] & 0x80) ? 1 : 0;
            bool use_optional = (variable_key_data[0] & 0x40) ? 1 : 0;

            VariableKey variable_key = {
                .lock_id = lock_id,
                .creation = time,
                .inhibit = inhibit,
                .use_optional = use_optional,
            };

            variable_keys[total_keys++] = variable_key;
        }
        remaining_varkey_bytes -= SAFLOK_MFC_VARKEY_SIZE;
    }

    // TODO: Figure out where this is stored and how it's formatted
    // These values are based on what I saw change, but no idea whether they're correct.
    switch(mfc_data->block[2].data[6]) {
    case 0x00:
        *optional_function = VariableKeysOptionalFunctionNone;
        break;
    case 0x21:
        *optional_function = VariableKeysOptionalFunctionLevelInhibit;
        break;
    case 0x88:
        *optional_function = VariableKeysOptionalFunctionElectLockUnlock;
        break;
    case 0xA8:
        *optional_function = VariableKeysOptionalFunctionLatchUnlatch;
        break;
    default:
        break;
    }

    return total_keys;
}
