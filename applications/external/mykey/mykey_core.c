#include "cogs_mikai.h"
#include <furi.h>
#include <string.h>
#include <machine/endian.h>
#include <storage/storage.h>

// Encode or decode a MyKey block (XOR bit manipulation)
static inline void encode_decode_block(uint32_t* block) {
    *block ^= (*block & 0x00C00000) << 6 | (*block & 0x0000C000) << 12 | (*block & 0x000000C0) << 18 |
              (*block & 0x000C0000) >> 6 | (*block & 0x00030000) >> 12 | (*block & 0x00000300) >> 6;
    *block ^= (*block & 0x30000000) >> 6 | (*block & 0x0C000000) >> 12 | (*block & 0x03000000) >> 18 |
              (*block & 0x00003000) << 6 | (*block & 0x00000030) << 12 | (*block & 0x0000000C) << 6;
    *block ^= (*block & 0x00C00000) << 6 | (*block & 0x0000C000) << 12 | (*block & 0x000000C0) << 18 |
              (*block & 0x000C0000) >> 6 | (*block & 0x00030000) >> 12 | (*block & 0x00000300) >> 6;
}

// Public wrapper for debug purposes
void mykey_encode_decode_block(uint32_t* block) {
    encode_decode_block(block);
}

// Calculate checksum of a generic block
static inline void calculate_block_checksum(uint32_t* block, const uint8_t block_num) {
    uint8_t checksum = 0xFF - block_num - (*block & 0x0F) - (*block >> 4 & 0x0F) -
                       (*block >> 8 & 0x0F) - (*block >> 12 & 0x0F) - (*block >> 16 & 0x0F) -
                       (*block >> 20 & 0x0F);

    // Clear first byte and set to checksum value
    *block &= 0x00FFFFFF;
    *block |= checksum << 24;
}

// Return the number of days between 1/1/1995 and a specified date
static uint32_t days_difference(uint8_t day, uint8_t month, uint16_t year) {
    if(month < 3) {
        year--;
        month += 12;
    }
    return year * 365 + year / 4 - year / 100 + year / 400 + (month * 153 + 3) / 5 + day - 728692;
}

// Get current transaction offset
static uint8_t get_current_transaction_offset(MyKeyData* key) {
    uint32_t block3C = key->eeprom[0x3C];

    // If first transaction, set the pointer to 7 to fill the first transaction block
    if(block3C == 0xFFFFFFFF) {
        return 0x07;
    }

    // Decode transaction pointer
    uint32_t current = block3C ^ (key->eeprom[0x07] & 0x00FFFFFF);
    encode_decode_block(&current);

    if((current & 0x00FF0000 >> 16) > 0x07) {
        // Out of range
        return 0x07;
    } else {
        // Return result (a value between 0x00 and 0x07)
        return current >> 16;
    }
}

// Calculate the encryption key and save the result in key struct
void mykey_calculate_encryption_key(MyKeyData* key) {
    FURI_LOG_I(TAG, "=== Encryption Key Calculation ===");
    FURI_LOG_I(TAG, "UID (as stored): 0x%016llX", key->uid);

    // OTP calculation (reverse block 6 + 1, incremental. 1,2,3, etc.)
    uint32_t block6 = key->eeprom[0x06];
    FURI_LOG_I(TAG, "Block 0x06 raw: 0x%08lX", block6);

    uint32_t block6_reversed = __bswap32(block6);
    FURI_LOG_I(TAG, "Block 0x06 reversed: 0x%08lX", block6_reversed);

    uint32_t otp = ~block6_reversed + 1;
    FURI_LOG_I(TAG, "OTP (~reversed + 1): 0x%08lX", otp);

    // Encryption key calculation
    // MK = UID * VENDOR
    // SK (Encryption key) = MK * OTP
    uint32_t block18_raw = key->eeprom[0x18];
    uint32_t block19_raw = key->eeprom[0x19];
    FURI_LOG_I(TAG, "Block 0x18 raw: 0x%08lX", block18_raw);
    FURI_LOG_I(TAG, "Block 0x19 raw: 0x%08lX", block19_raw);

    uint32_t block18 = block18_raw;
    uint32_t block19 = block19_raw;
    encode_decode_block(&block18);
    encode_decode_block(&block19);
    FURI_LOG_I(TAG, "Block 0x18 decoded: 0x%08lX", block18);
    FURI_LOG_I(TAG, "Block 0x19 decoded: 0x%08lX", block19);

    uint64_t vendor = (((uint64_t)block18 << 16) | (block19 & 0x0000FFFF)) + 1;
    FURI_LOG_I(TAG, "Vendor: 0x%llX", vendor);

    // Calculate encryption key: UID * vendor * OTP
    // UID is now correctly stored in big-endian format, no swapping needed
    key->encryption_key = (key->uid * vendor * otp) & 0xFFFFFFFF;
    FURI_LOG_I(TAG, "Encryption Key: 0x%08lX", key->encryption_key);
    FURI_LOG_I(TAG, "===================================");
}

// Check if MyKey is reset (no vendor bound)
bool mykey_is_reset(MyKeyData* key) {
    static const uint32_t block18_reset = 0x8FCD0F48;
    static const uint32_t block19_reset = 0xC0820007;
    return key->eeprom[0x18] == block18_reset && key->eeprom[0x19] == block19_reset;
}

// Get block value
uint32_t mykey_get_block(MyKeyData* key, uint8_t block_num) {
    if(block_num >= SRIX4K_BLOCKS) return 0;
    return key->eeprom[block_num];
}

// Modify block
void mykey_modify_block(MyKeyData* key, uint32_t block, uint8_t block_num) {
    if(block_num < SRIX4K_BLOCKS) {
        key->eeprom[block_num] = block;
    }
}

// Extract current credit using libmikai method (block 0x21) - PRIMARY METHOD
uint16_t mykey_get_current_credit(MyKeyData* key) {
    FURI_LOG_I(TAG, "=== Credit Decoding (libmikai method) ===");

    // Use libmikai approach: read from block 0x21
    uint32_t block21_raw = key->eeprom[0x21];
    FURI_LOG_I(TAG, "Block 0x21 raw: 0x%08lX", block21_raw);
    FURI_LOG_I(TAG, "  Bytes: [%02X %02X %02X %02X]",
        (uint8_t)(block21_raw & 0xFF),
        (uint8_t)((block21_raw >> 8) & 0xFF),
        (uint8_t)((block21_raw >> 16) & 0xFF),
        (uint8_t)((block21_raw >> 24) & 0xFF));

    FURI_LOG_I(TAG, "Encryption key: 0x%08lX", key->encryption_key);

    uint32_t after_xor = block21_raw ^ key->encryption_key;
    FURI_LOG_I(TAG, "After XOR: 0x%08lX", after_xor);

    uint32_t current_credit = after_xor;
    encode_decode_block(&current_credit);
    FURI_LOG_I(TAG, "After encode_decode: 0x%08lX", current_credit);

    uint16_t credit_lower = current_credit & 0xFFFF;
    uint16_t credit_upper = (current_credit >> 16) & 0xFFFF;
    FURI_LOG_I(TAG, "Lower 16 bits: %u cents (%u.%02u EUR)",
        credit_lower, credit_lower / 100, credit_lower % 100);
    FURI_LOG_I(TAG, "Upper 16 bits: %u cents (%u.%02u EUR)",
        credit_upper, credit_upper / 100, credit_upper % 100);
    FURI_LOG_I(TAG, "=========================================");

    return credit_lower;
}

// Get credit from transaction history (for comparison/debugging)
uint16_t mykey_get_credit_from_history(MyKeyData* key) {
    uint32_t block3C = key->eeprom[0x3C];
    if(block3C == 0xFFFFFFFF) {
        return 0xFFFF; // No history available
    }

    // Decrypt block 0x3C to get starting offset
    uint32_t decrypted_3C = block3C ^ key->eeprom[0x07];
    uint32_t starting_offset = ((decrypted_3C & 0x30000000) >> 28) |
                               ((decrypted_3C & 0x00100000) >> 18);

    if(starting_offset >= 8) {
        return 0xFFFF; // Invalid offset
    }

    // Get most recent transaction (offset 8 in the circular buffer)
    // Blocks are already in big-endian format, credit is in lower 16 bits
    uint32_t txn_block = key->eeprom[0x34 + ((starting_offset + 8) % 8)];
    uint16_t credit = txn_block & 0xFFFF;

    FURI_LOG_D(TAG, "Credit from transaction history: %d cents", credit);
    return credit;
}

// Add N cents to MyKey actual credit
bool mykey_add_cents(MyKeyData* key, uint16_t cents, uint8_t day, uint8_t month, uint8_t year) {
    FURI_LOG_I(TAG, "=== Adding %u cents (%u.%02u EUR) ===", cents, cents / 100, cents % 100);

    // Check reset key
    if(mykey_is_reset(key)) {
        FURI_LOG_E(TAG, "Key is reset, cannot add credit");
        return false;
    }

    if(key->eeprom[0x06] == 0 || key->eeprom[0x06] == 0xFFFFFFFF) {
        FURI_LOG_E(TAG, "Key has no vendor");
        return false;
    }

    // Calculate current credit
    uint16_t precedent_credit;
    uint16_t actual_credit = mykey_get_current_credit(key);
    FURI_LOG_I(TAG, "Current credit: %u cents", actual_credit);

    // Get current transaction position
    uint8_t current = get_current_transaction_offset(key);
    FURI_LOG_I(TAG, "Current transaction offset: %u", current);

    // Split credit into multiple transactions. Stop at 5 cent.
    do {
        // Save current credit to precedent
        precedent_credit = actual_credit;

        // Choose current recharge
        if(cents / 200 > 0) {
            cents -= 200;
            actual_credit += 200;
        } else if(cents / 100 > 0) {
            cents -= 100;
            actual_credit += 100;
        } else if(cents / 50 > 0) {
            cents -= 50;
            actual_credit += 50;
        } else if(cents / 20 > 0) {
            cents -= 20;
            actual_credit += 20;
        } else if(cents / 10 > 0) {
            cents -= 10;
            actual_credit += 10;
        } else if(cents / 5 > 0) {
            cents -= 5;
            actual_credit += 5;
        } else {
            // Less than 5 cents
            actual_credit += cents;
            cents = 0;
        }

        // Point to new credit position
        current = (current == 7) ? 0 : current + 1;

        // Save new credit to history blocks
        uint32_t txn_block = (uint32_t)day << 27 | (uint32_t)month << 23 |
                             (uint32_t)year << 16 | actual_credit;
        key->eeprom[0x34 + current] = txn_block;

        FURI_LOG_I(TAG, "Transaction %u: %u cents at block 0x%02X",
                   current, actual_credit, 0x34 + current);
    } while(cents > 5);

    FURI_LOG_I(TAG, "Final credit: %u cents, precedent: %u cents",
               actual_credit, precedent_credit);

    // Save new credit to 21 and 25
    key->eeprom[0x21] = actual_credit;
    calculate_block_checksum(&key->eeprom[0x21], 0x21);
    encode_decode_block(&key->eeprom[0x21]);
    key->eeprom[0x21] ^= key->encryption_key;

    key->eeprom[0x25] = actual_credit;
    calculate_block_checksum(&key->eeprom[0x25], 0x25);
    encode_decode_block(&key->eeprom[0x25]);
    key->eeprom[0x25] ^= key->encryption_key;

    // Save precedent credit to 23 and 27
    key->eeprom[0x23] = precedent_credit;
    calculate_block_checksum(&key->eeprom[0x23], 0x23);
    encode_decode_block(&key->eeprom[0x23]);

    key->eeprom[0x27] = precedent_credit;
    calculate_block_checksum(&key->eeprom[0x27], 0x27);
    encode_decode_block(&key->eeprom[0x27]);

    // Save transaction pointer to block 3C
    key->eeprom[0x3C] = current << 16;
    calculate_block_checksum(&key->eeprom[0x3C], 0x3C);
    encode_decode_block(&key->eeprom[0x3C]);
    key->eeprom[0x3C] ^= key->eeprom[0x07] & 0x00FFFFFF;

    // Increment operation counter (block 0x12, lower 24 bits)
    uint32_t op_count = (key->eeprom[0x12] & 0x00FFFFFF) + 1;
    key->eeprom[0x12] = (key->eeprom[0x12] & 0xFF000000) | (op_count & 0x00FFFFFF);
    FURI_LOG_I(TAG, "Operation counter incremented to: %lu", op_count);

    // Mark as modified
    key->is_modified = true;

    return true;
}

// Reset credit history and charge N cents
bool mykey_set_cents(MyKeyData* key, uint16_t cents, uint8_t day, uint8_t month, uint8_t year) {
    // Backup precedent blocks
    uint32_t dump[10];
    memcpy(dump, &key->eeprom[0x21], SRIX_BLOCK_LENGTH);
    memcpy(dump + 1, &key->eeprom[0x34], 9 * SRIX_BLOCK_LENGTH);

    key->eeprom[0x21] = 0;
    calculate_block_checksum(&key->eeprom[0x21], 0x21);
    encode_decode_block(&key->eeprom[0x21]);
    key->eeprom[0x21] ^= key->encryption_key;

    // Reset transaction history and pointer (0x34-0x3C)
    memset(&key->eeprom[0x34], 0xFF, 9 * SRIX_BLOCK_LENGTH);

    // If there is an error, restore precedent dump
    if(!mykey_add_cents(key, cents, day, month, year)) {
        memcpy(&key->eeprom[0x21], dump, SRIX_BLOCK_LENGTH);
        memcpy(&key->eeprom[0x34], dump + 1, 9 * SRIX_BLOCK_LENGTH);
        return false;
    }

    return true;
}

// Reset a MyKey to associate it with another vendor
void mykey_reset(MyKeyData* key) {
    for(uint8_t i = 0x10; i < SRIX4K_BLOCKS; i++) {
        uint32_t current_block;

        switch(i) {
            case 0x10:
            case 0x14:
            case 0x3F:
            case 0x43: {
                // Key ID (first byte) + days elapsed from production
                uint32_t production_date = key->eeprom[0x08];

                // Decode BCD (Binary Coded Decimal) production date
                uint8_t pday = (production_date >> 28 & 0x0F) * 10 + (production_date >> 24 & 0x0F);
                uint8_t pmonth = (production_date >> 20 & 0x0F) * 10 + (production_date >> 16 & 0x0F);
                uint16_t pyear = (production_date & 0x0F) * 1000 +
                                (production_date >> 4 & 0x0F) * 100 +
                                (production_date >> 12 & 0x0F) * 10 +
                                (production_date >> 8 & 0x0F);

                uint32_t elapsed = days_difference(pday, pmonth, pyear);
                current_block = ((key->eeprom[0x07] & 0xFF000000) >> 8) |
                               (((elapsed / 1000 % 10) << 12) + ((elapsed / 100 % 10) << 8)) |
                               (((elapsed / 10 % 10) << 4) + (elapsed % 10));
                calculate_block_checksum(&current_block, i);
                break;
            }

            case 0x11:
            case 0x15:
            case 0x40:
            case 0x44:
                // Key ID [last three bytes]
                current_block = key->eeprom[0x07];
                calculate_block_checksum(&current_block, i);
                break;

            case 0x22:
            case 0x26:
            case 0x51:
            case 0x55: {
                // Production date (last three bytes)
                uint32_t production_date = key->eeprom[0x08];
                current_block = (production_date & 0x0000FF00) << 8 | (production_date & 0x00FF0000) >> 8 |
                               (production_date & 0xFF000000) >> 24;
                calculate_block_checksum(&current_block, i);
                encode_decode_block(&current_block);
                break;
            }

            case 0x12:
            case 0x16:
            case 0x41:
            case 0x45:
                // Operations counter
                current_block = 1;
                calculate_block_checksum(&current_block, i);
                break;

            case 0x13:
            case 0x17:
            case 0x42:
            case 0x46:
                // Generic blocks
                current_block = 0x00040013;
                calculate_block_checksum(&current_block, i);
                break;

            case 0x18:
            case 0x1C:
            case 0x47:
            case 0x4B:
                // Generic blocks
                current_block = 0x0000FEDC;
                calculate_block_checksum(&current_block, i);
                encode_decode_block(&current_block);
                break;

            case 0x19:
            case 0x1D:
            case 0x48:
            case 0x4C:
                // Generic blocks
                current_block = 0x00000123;
                calculate_block_checksum(&current_block, i);
                encode_decode_block(&current_block);
                break;

            case 0x21:
            case 0x25:
                // Current credit (0,00€)
                mykey_calculate_encryption_key(key);
                current_block = 0;
                calculate_block_checksum(&current_block, i);
                encode_decode_block(&current_block);
                current_block ^= key->encryption_key;
                break;

            case 0x20:
            case 0x24:
            case 0x4F:
            case 0x53:
                // Generic blocks
                current_block = 0x00010000;
                calculate_block_checksum(&current_block, i);
                encode_decode_block(&current_block);
                break;

            case 0x1A:
            case 0x1B:
            case 0x1E:
            case 0x1F:
            case 0x23:
            case 0x27:
            case 0x49:
            case 0x4A:
            case 0x4D:
            case 0x4E:
            case 0x50:
            case 0x52:
            case 0x54:
            case 0x56:
                // Generic blocks
                current_block = 0;
                calculate_block_checksum(&current_block, i);
                encode_decode_block(&current_block);
                break;

            default:
                current_block = 0xFFFFFFFF;
                break;
        }

        // If this block has a different value than EEPROM, modify it
        if(key->eeprom[i] != current_block) {
            key->eeprom[i] = current_block;
        }
    }

    // Mark as modified
    key->is_modified = true;
}


// Save raw card data to file for debugging
bool mykey_save_raw_data(COGSMyKaiApp* app, const char* path) {
    if(!app->mykey.is_loaded) {
        return false;
    }

    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);

    if(!storage_file_open(file, path, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        return false;
    }

    // Write header
    FuriString* line = furi_string_alloc();
    furi_string_printf(line, "MyKey Raw Data Dump\n");
    furi_string_cat_printf(line, "UID: %016llX\n", (unsigned long long)app->mykey.uid);
    furi_string_cat_printf(line, "Encryption Key: 0x%08lX\n\n", app->mykey.encryption_key);
    storage_file_write(file, furi_string_get_cstr(line), furi_string_size(line));

    // Write all blocks
    for(size_t i = 0; i < SRIX4K_BLOCKS; i++) {
        furi_string_printf(line, "Block 0x%02zX: 0x%08lX\n", i, app->mykey.eeprom[i]);
        storage_file_write(file, furi_string_get_cstr(line), furi_string_size(line));
    }

    furi_string_free(line);
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);

    return true;
}
