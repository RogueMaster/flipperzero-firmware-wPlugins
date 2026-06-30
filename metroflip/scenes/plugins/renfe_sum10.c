#include <flipper_application.h>
#include "../../metroflip_i.h"

#include <nfc/protocols/mf_classic/mf_classic_poller_sync.h>
#include <nfc/protocols/mf_classic/mf_classic.h>
#include <nfc/protocols/mf_classic/mf_classic_poller.h>
#include "../../api/metroflip/metroflip_api.h"

#include <dolphin/dolphin.h>
#include <bit_lib.h>
#include <furi_hal.h>
#include <nfc/nfc.h>
#include <nfc/nfc_device.h>
#include <nfc/nfc_listener.h>
#include <storage/storage.h>
#include <ctype.h>
#include "../../api/metroflip/metroflip_api.h"
#include "../../metroflip_plugins.h"

#define TAG "Metroflip:Scene:RenfeSum10"
typedef struct {
    uint8_t block_data[16];  
    uint32_t timestamp;     
    int block_number;       
} HistoryEntry;

// Station name cache structure
#define MAX_STATION_NAME_LENGTH 20
#define MAX_CACHED_STATIONS 50
typedef struct {
    uint16_t code;
    char name[MAX_STATION_NAME_LENGTH];
} StationEntry;

typedef struct {
    StationEntry stations[MAX_CACHED_STATIONS];
    size_t count;
    bool loaded;
    char current_region[16];  // Reduced from 32
} StationCache;

// Global station cache - will be allocated dynamically
static StationCache* station_cache = NULL;

// Forward declarations for helper functions
static bool renfe_sum10_is_history_entry(const uint8_t* block_data);
static void renfe_sum10_parse_history_entry(FuriString* parsed_data, const uint8_t* block_data, int entry_num);
static void renfe_sum10_parse_travel_history(FuriString* parsed_data, const MfClassicData* data);
static uint32_t renfe_sum10_extract_timestamp(const uint8_t* block_data);
static void renfe_sum10_sort_history_entries(HistoryEntry* entries, int count);
static bool renfe_sum10_has_history_data(const MfClassicData* data);
static bool renfe_sum10_load_station_file(const char* region);
static const char* renfe_sum10_get_station_name_from_cache(uint16_t station_code);
static void renfe_sum10_clear_station_cache(void);
static const char* renfe_sum10_get_station_name_dynamic(uint16_t station_code);
static bool renfe_sum10_is_travel_history_block(const uint8_t* block_data, int block_number);
static bool renfe_sum10_is_default_station_code(uint16_t station_code);
static bool renfe_sum10_is_valid_timestamp(const uint8_t* block_data);
static const char* renfe_sum10_get_origin_station(const MfClassicData* data);
static const char* renfe_sum10_detect_card_variant(const MfClassicData* data);
static char renfe_sum10_normalize_accent(uint8_t byte);

// Normalize accented characters to their non-accented equivalents
static char renfe_sum10_normalize_accent(uint8_t byte) {
    // Handle common Spanish accented characters
    switch(byte) {
        // Uppercase accented vowels
        case 0xC1: return 'A'; // Á
        case 0xC9: return 'E'; // É
        case 0xCD: return 'I'; // Í
        case 0xD3: return 'O'; // Ó
        case 0xDA: return 'U'; // Ú
        case 0xDC: return 'U'; // Ü
        case 0xD1: return 'N'; // Ñ
        
        // Lowercase accented vowels
        case 0xE1: return 'A'; // á
        case 0xE9: return 'E'; // é
        case 0xED: return 'I'; // í
        case 0xF3: return 'O'; // ó
        case 0xFA: return 'U'; // ú
        case 0xFC: return 'U'; // ü
        case 0xF1: return 'N'; // ñ
        
        // Latin-1 supplement characters (common in NFC cards)
        case 0x80: case 0x81: case 0x82: case 0x83: return 'A'; // Various A accents
        case 0x88: case 0x89: case 0x8A: case 0x8B: return 'E'; // Various E accents
        case 0x8C: case 0x8D: case 0x8E: case 0x8F: return 'I'; // Various I accents
        case 0x92: case 0x93: case 0x94: case 0x95: return 'O'; // Various O accents
        case 0x96: case 0x97: case 0x98: case 0x99: return 'U'; // Various U accents
        
        // Special separators that might indicate word boundaries
        case 0x20: return ' '; // Standard space
        case 0x09: return ' '; // Tab -> space
        case 0x0A: case 0x0D: return 0; // Line breaks = end
        
        // Default: if it's a printable ASCII, return as-is, otherwise convert to ?
        default:
            if(byte >= 0x20 && byte <= 0x7E) {
                return (char)byte;
            } else if(byte >= 0x41 && byte <= 0x5A) {
                return (char)byte; // A-Z
            } else if(byte >= 0x61 && byte <= 0x7A) {
                return (char)byte; // a-z 
            }
            return 0; // Invalid character
    }
}

// Keys for RENFE Suma 10 cards - specific keys found in real dumps
const MfClassicKeyPair renfe_sum10_keys[16] = {
    {.a = 0xA8844B0BCA06, .b = 0xffffffffffff}, // Sector 0 - RENFE specific key
    {.a = 0xCB5ED0E57B08, .b = 0xffffffffffff}, // Sector 1 - RENFE specific key 
    {.a = 0x749934CC8ED3, .b = 0xffffffffffff}, // Sector 2
    {.a = 0xAE381EA0811B, .b = 0xffffffffffff}, // Sector 3
    {.a = 0x40454EE64229, .b = 0xffffffffffff}, // Sector 4 - Contains block 18 (history)
    {.a = 0x66A4932816D3, .b = 0xffffffffffff}, // Sector 5 - Contains block 22 (history)
    {.a = 0xB54D99618ADC, .b = 0xffffffffffff}, // Sector 6
    {.a = 0x08D6A7765640, .b = 0xffffffffffff}, // Sector 7 - Contains blocks 28,29,30 (history)
    {.a = 0x3E0557273982, .b = 0xffffffffffff}, // Sector 8
    {.a = 0xC0C1C2C3C4C5, .b = 0xffffffffffff}, // Sector 9
    {.a = 0xC0C1C2C3C4C5, .b = 0xffffffffffff}, // Sector 10
    {.a = 0xC0C1C2C3C4C5, .b = 0xffffffffffff}, // Sector 11 - Contains blocks 44,45,46 (history)
    {.a = 0xffffffffffff, .b = 0xffffffffffff}, // Sector 12
    {.a = 0xffffffffffff, .b = 0xffffffffffff}, // Sector 13
    {.a = 0xffffffffffff, .b = 0xffffffffffff}, // Sector 14
    {.a = 0xffffffffffff, .b = 0xffffffffffff}, // Sector 15
};

// Check if a station code is problematic (appears in empty cards)
static bool renfe_sum10_is_default_station_code(uint16_t station_code) {
    // Codes that appear in new/empty cards - these should be filtered out
    return (station_code == 0x2000 || // La Fonteta (appears in empty cards)
            station_code == 0x0000 || // Null code
            station_code == 0xFFFF);  // Fill pattern
}

// Detect RENFE card variant (SUMA 10, MOBILIS 30, etc.)
static const char* renfe_sum10_detect_card_variant(const MfClassicData* data) {
    if(!data) {
        return "SUMA 10";
    }
    
    // Check Block 9 for cardholder name pattern (indicates MOBILIS 30)
    if(mf_classic_is_block_read(data, 9)) {
        const uint8_t* block9 = data->block[9].data;
        
        // Check if block 9 contains printable ASCII characters (name pattern)
        // MOBILIS 30 cards have cardholder names stored in Block 9
        int printable_chars = 0;
        for(size_t i = 0; i < 10; i++) {
            if(block9[i] >= 0x20 && block9[i] <= 0x7E) { // Printable ASCII
                printable_chars++;
            } else if(block9[i] == 0x00) {
                break; // End of string
            }
        }
        
        // If we have 3 or more printable characters, likely a name
        if(printable_chars >= 3) {
            return "MOBILIS 30";
        }
    }
    
    // Check Block 14 for MOBILIS 30 general patterns
    if(mf_classic_is_block_read(data, 14)) {
        const uint8_t* block14 = data->block[14].data;
        
        // Look for general MOBILIS patterns (not specific names)
        // Check for any readable text pattern in the block
        int text_chars = 0;
        for(size_t i = 0; i < 16; i++) {
            if(block14[i] >= 0x41 && block14[i] <= 0x5A) { // A-Z
                text_chars++;
            }
        }
        
        // If we have several uppercase letters, likely MOBILIS 30
        if(text_chars >= 4) {
            return "MOBILIS 30";
        }
    }
    
    // Check Block 9 for other variant signatures
    if(mf_classic_is_block_read(data, 9)) {
        const uint8_t* block9 = data->block[9].data;
        
        // Check for personal name patterns that might indicate special cards
        bool has_name_pattern = false;
        for(int i = 0; i < 10; i++) {
            if(block9[i] >= 0x41 && block9[i] <= 0x5A) { // A-Z
                has_name_pattern = true;
                break;
            }
        }
        
        if(has_name_pattern) {
            // Could be MOBILIS or other personalized card
            // For now, we'll keep it as MOBILIS 30 if Block 14 matches
            // or check for other specific patterns in the future
        }
    }
    
    // Default to SUMA 10 for regular cards
    return "SUMA 10";
}

// Check if timestamp indicates a real transaction (not initialization data)
static bool renfe_sum10_is_valid_timestamp(const uint8_t* block_data) {
    if(!block_data) return false;
    
    // Check if timestamp is all zeros (initialization pattern)
    if(block_data[2] == 0x00 && block_data[3] == 0x00 && block_data[4] == 0x00) {
        return false;
    }
    
    // Check if timestamp is all 0xFF (empty pattern)
    if(block_data[2] == 0xFF && block_data[3] == 0xFF && block_data[4] == 0xFF) {
        return false;
    }
    
    return true;
}

// Check if a block contains a valid history entry with stricter validation
static bool renfe_sum10_is_history_entry(const uint8_t* block_data) {
    // Check for null pointer
    if(!block_data) {
        return false;
    }
    
    // Extract station code for validation
    uint16_t station_code = (block_data[9] << 8) | block_data[10];
    
    // Filter out problematic station codes that appear in empty cards
    if(renfe_sum10_is_default_station_code(station_code)) {
        return false;
    }
    
    // Validate timestamp - must not be initialization data
    if(!renfe_sum10_is_valid_timestamp(block_data)) {
        return false;
    }
    
    // Primary pattern: second byte is 0x98 and first byte is not 0x00
    if(block_data[1] == 0x98 && block_data[0] != 0x00) {
        return true;
    }
    
    // Stricter validation: Only accept CONFIRMED transaction types from real cards
    uint8_t transaction_type = block_data[0];
    bool is_confirmed_transaction = (transaction_type == 0x13 || // ENTRY - confirmed
                                   transaction_type == 0x1A || // EXIT - confirmed  
                                   transaction_type == 0x1E || // TRANSFER - confirmed
                                   transaction_type == 0x16 || // VALIDATION - confirmed
                                   transaction_type == 0x33 || // TOP-UP - confirmed
                                   transaction_type == 0x3A);  // CHARGE - confirmed
    
    // Only accept if it's a confirmed transaction type AND has valid data
    if(is_confirmed_transaction && 
       (block_data[1] != 0xFF && block_data[2] != 0xFF) &&
       station_code != 0x0000) {
        return true;
    }
    
    return false;
}

// Get the origin station from the first top-up/recharge found in the card
static const char* renfe_sum10_get_origin_station(const MfClassicData* data) {
    if(!data) return "Unknown";
    
    // Check if this is a MOBILIS 30 card - these are always from Valencia
    const char* card_variant = renfe_sum10_detect_card_variant(data);
    if(strcmp(card_variant, "MOBILIS 30") == 0) {
        return "Valencia";
    }
    
    // History blocks where recharges are typically stored
    int history_blocks[] = {18, 22, 28, 29, 30, 44, 45, 46};
    int num_blocks = sizeof(history_blocks) / sizeof(history_blocks[0]);
    
    // Get max blocks for this card type
    int max_blocks = (data->type == MfClassicType1k) ? 64 : 256;
    
    uint32_t earliest_topup_timestamp = 0xFFFFFFFF;
    uint16_t origin_station_code = 0x0000;
    
    // First pass: Look specifically for top-up entries
    for(int i = 0; i < num_blocks; i++) {
        int block = history_blocks[i];
        
        if(block >= max_blocks) continue;
        if(!mf_classic_is_block_read(data, block)) continue;
        
        const uint8_t* block_data = data->block[block].data;
        
        // Check if this is a top-up entry (prioritize actual recharges)
        uint8_t transaction_type = block_data[0];
        if(transaction_type == 0x33 || transaction_type == 0x3A || 
           transaction_type == 0x31 || transaction_type == 0x32) {
            uint32_t timestamp = renfe_sum10_extract_timestamp(block_data);
            uint16_t station_code = (block_data[9] << 8) | block_data[10];
            
            // If this is the earliest recharge timestamp we've seen
            if(timestamp < earliest_topup_timestamp && timestamp > 0 && 
               station_code != 0x0000 && !renfe_sum10_is_default_station_code(station_code)) {
                earliest_topup_timestamp = timestamp;
                origin_station_code = station_code;
            }
        }
    }
    
    // If we found a recharge station, return it
    if(origin_station_code != 0x0000) {
        const char* station_name = renfe_sum10_get_station_name_dynamic(origin_station_code);
        if(strcmp(station_name, "Unknown") != 0) {
            return station_name;
        }
    }
    
    // Second pass: Look for the earliest valid travel entry as fallback
    uint32_t earliest_travel_timestamp = 0xFFFFFFFF;
    uint16_t earliest_travel_station = 0x0000;
    
    for(int i = 0; i < num_blocks; i++) {
        int block = history_blocks[i];
        
        if(block >= max_blocks) continue;
        if(!mf_classic_is_block_read(data, block)) continue;
        
        const uint8_t* block_data = data->block[block].data;
        
        if(renfe_sum10_is_history_entry(block_data)) {
            uint32_t timestamp = renfe_sum10_extract_timestamp(block_data);
            uint16_t station_code = (block_data[9] << 8) | block_data[10];
            
            if(timestamp < earliest_travel_timestamp && timestamp > 0 && 
               station_code != 0x0000 && !renfe_sum10_is_default_station_code(station_code)) {
                earliest_travel_timestamp = timestamp;
                earliest_travel_station = station_code;
            }
        }
    }
    
    // Return the earliest travel station if found
    if(earliest_travel_station != 0x0000) {
        const char* station_name = renfe_sum10_get_station_name_dynamic(earliest_travel_station);
        if(strcmp(station_name, "Unknown") != 0) {
            return station_name;
        }
    }
    
    return "Unknown";
}

// Extract timestamp from block data for sorting purposes
static uint32_t renfe_sum10_extract_timestamp(const uint8_t* block_data) {
    if(!block_data) return 0;
    
    // Extract timestamp from bytes 2-4 and combine into a single value for comparison
    uint32_t timestamp = ((uint32_t)block_data[2] << 16) | 
                        ((uint32_t)block_data[3] << 8) | 
                        (uint32_t)block_data[4];
    
    return timestamp;
}
// Sort history entries manually (newest first)
static void renfe_sum10_sort_history_entries(HistoryEntry* entries, int count) {
    if(!entries || count <= 1) return;
    
    for(int i = 0; i < count - 1; i++) {
        for(int j = 0; j < count - 1 - i; j++) {
            bool should_swap = false;
            
            if(entries[j].timestamp < entries[j + 1].timestamp) {
                should_swap = true;
            } else if(entries[j].timestamp == entries[j + 1].timestamp) {
                if(entries[j].block_number < entries[j + 1].block_number) {
                    should_swap = true;
                }
            }
            
            if(should_swap) {
                // Swap entries
                HistoryEntry temp = entries[j];
                entries[j] = entries[j + 1];
                entries[j + 1] = temp;
            }
        }
    }
}

// Clear the station cache
static void renfe_sum10_clear_station_cache(void) {
    if(station_cache) {
        station_cache->count = 0;
        station_cache->loaded = false;
        memset(station_cache->current_region, 0, sizeof(station_cache->current_region));
        memset(station_cache->stations, 0, sizeof(station_cache->stations));
    }
}

// Load station names from file (Valencia-specific files)
static bool renfe_sum10_load_station_file(const char* region) {
    if(!region) {
        return false;
    }
    
    // Allocate cache if not already allocated
    if(!station_cache) {
        station_cache = malloc(sizeof(StationCache));
        if(!station_cache) {
            return false;
        }
        memset(station_cache, 0, sizeof(StationCache));
    }
    
    // Check if we already have this file loaded
    if(station_cache->loaded && strcmp(station_cache->current_region, region) == 0) {
        return true;
    }
    
    // Clear existing cache
    renfe_sum10_clear_station_cache();
    
    // Build file path
    FuriString* file_path = furi_string_alloc();
    furi_string_printf(file_path, "/ext/apps_assets/metroflip/renfe/stations/%s.txt", region);
    
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    
    bool success = false;
    if(storage_file_open(file, furi_string_get_cstr(file_path), FSAM_READ, FSOM_OPEN_EXISTING)) {
        char line_buffer[64];
        station_cache->count = 0;
        
        // Read file line by line using simple character reading
        while(station_cache->count < MAX_CACHED_STATIONS) {
            size_t line_pos = 0;
            bool end_of_file = false;
            
            // Read line character by character
            while(line_pos < sizeof(line_buffer) - 1) {
                char c;
                size_t bytes_read = storage_file_read(file, &c, 1);
                if(bytes_read == 0) {
                    // End of file
                    end_of_file = true;
                    break;
                }
                
                if(c == '\n') {
                    // End of line
                    break;
                }
                
                if(c != '\r') { // Skip carriage returns
                    line_buffer[line_pos++] = c;
                }
            }
            
            if(end_of_file && line_pos == 0) {
                // End of file with no more data
                break;
            }
            
            line_buffer[line_pos] = '\0';
            
            // Skip comments and empty lines
            if(line_buffer[0] == '#' || line_buffer[0] == '\0' || strlen(line_buffer) < 3) {
                if(end_of_file) break;
                continue;
            }
            
            // Parse format: 0xCODE,Station Name
            char* comma = strchr(line_buffer, ',');
            if(comma && station_cache->count < MAX_CACHED_STATIONS) {
                *comma = '\0';
                char* code_str = line_buffer;
                char* name_str = comma + 1;
                
                // Parse hex code
                uint16_t code = 0;
                if(sscanf(code_str, "0x%hX", &code) == 1) {
                    // Store in cache
                    station_cache->stations[station_cache->count].code = code;
                    strncpy(station_cache->stations[station_cache->count].name, name_str, MAX_STATION_NAME_LENGTH - 1);
                    station_cache->stations[station_cache->count].name[MAX_STATION_NAME_LENGTH - 1] = '\0';
                    station_cache->count++;
                }
            }
            
            if(end_of_file) break;
        }
        
        if(station_cache->count > 0) {
            strncpy(station_cache->current_region, region, sizeof(station_cache->current_region) - 1);
            station_cache->current_region[sizeof(station_cache->current_region) - 1] = '\0';
            station_cache->loaded = true;
            success = true;
        }
        
        storage_file_close(file);
    }
    
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    furi_string_free(file_path);
    
    return success;
}

// Get station name from cache
static const char* renfe_sum10_get_station_name_from_cache(uint16_t station_code) {
    if(!station_cache || !station_cache->loaded) {
        return "Unknown";
    }
    
    for(size_t i = 0; i < station_cache->count; i++) {
        if(station_cache->stations[i].code == station_code) {
            return station_cache->stations[i].name;
        }
    }
    
    return "Unknown";
}

// Dynamic station name lookup 
static const char* renfe_sum10_get_station_name_dynamic(uint16_t station_code) {
    // Handle special case
    if(station_code == 0x0000) {
        return "";
    }
    
    // Always use Valencia since these cards are Valencia-specific
    if(renfe_sum10_load_station_file("valencia")) {
        const char* station_name = renfe_sum10_get_station_name_from_cache(station_code);
        if(strcmp(station_name, "Unknown") != 0) {
            return station_name;
        }
    }
    
    // Try Valencia fallback files
    const char* fallback_files[] = {
        "cercanias_valencia",
        "metro_valencia", 
        "tranvia_valencia",
        NULL
    };
    
    for(int i = 0; fallback_files[i] != NULL; i++) {
        if(renfe_sum10_load_station_file(fallback_files[i])) {
            const char* station_name = renfe_sum10_get_station_name_from_cache(station_code);
            if(strcmp(station_name, "Unknown") != 0) {
                return station_name;
            }
        }
    }
    
    return "Unknown";
}

// Updated wrapper function to maintain API compatibility
static const char* renfe_sum10_get_station_name(uint16_t station_code) {
    return renfe_sum10_get_station_name_dynamic(station_code);
}
// Extract zone code from Block 5 data
static uint16_t renfe_sum10_extract_zone_code(const uint8_t* block5_data) {
    if(!block5_data) {
        return 0x0000;
    }
    
    // Zone is stored in bytes 5-6 of Block 5 (big-endian)
    uint16_t zone_code = (block5_data[5] << 8) | block5_data[6];
    
    return zone_code;
}

// Extract zone code specifically for MOBILIS 30 cards from different blocks
static uint16_t renfe_sum10_extract_mobilis_zone_code(const MfClassicData* data) {
    if(!data) return 0x0000;
    
    // Check Block 12 for MOBILIS 30 zone information
    if(mf_classic_is_block_read(data, 12)) {
        const uint8_t* block12 = data->block[12].data;
        // Block 12: 81 F8 01 02 FF 2D 00 00 00 E0 8F 71 30 02 00 93
        // Byte 3 (02) might be zone indicator
        if(block12[3] >= 0x01 && block12[3] <= 0x06) {
            return (uint16_t)(0x8100 + (block12[3] * 0x10));  // Convert to zone format
        }
        
        // Check byte 11-12 for zone pattern: 30 02
        if(block12[11] == 0x30 && block12[12] == 0x02) {
            return 0x8200; // Zone 1 pattern
        }
    }
    
    // Check Block 10 for alternative zone information
    if(mf_classic_is_block_read(data, 10)) {
        const uint8_t* block10 = data->block[10].data;
        // Block 10: 00 00 00 00 00 00 58 00 00 00 81 C4 9B B9 25 3B
        // Byte 6 (58) could be zone related
        if(block10[6] >= 0x50 && block10[6] <= 0x5F) {
            return (uint16_t)(0x6000 + ((block10[6] - 0x50) * 0x200)); // Convert to zone A-F format
        }
    }
    
    // Check Block 1 for zone information (sometimes stored there in MOBILIS)
    if(mf_classic_is_block_read(data, 1)) {
        const uint8_t* block1 = data->block[1].data;
        // Block 1: 00 00 00 00 00 02 42 20 00 00 00 00 00 00 00 05
        // Byte 5 (02) might indicate zone
        if(block1[5] >= 0x01 && block1[5] <= 0x06) {
            return (uint16_t)(0x6C00 - (block1[5] - 1) * 0x200); // Zone A=6C00, B=6A00, etc.
        }
    }
    
    return 0x0000; // No zone found
}

// Get zone name from zone code (simplified for memory efficiency)
static const char* renfe_sum10_get_zone_name(uint16_t zone_code) {
    switch(zone_code & 0xFF00) {
        case 0x6C00: return "ABC";
        case 0x6200: case 0x6250: case 0x6270: return "B";
        case 0x6A00: case 0x6A10: case 0x6A50: return "C";
        case 0x6800: case 0x6810: case 0x6850: return "D";
        case 0x6600: case 0x6610: case 0x6650: return "E";
        case 0x6400: case 0x6410: case 0x6450: return "F";
        case 0x4C00: return "CD";
        
        // Zonas combinadas
        case 0x6D00: return "AB";
        case 0x6E00: return "AC";
        case 0x6F00: return "BC";
        case 0x7000: return "ABC";
        case 0x7100: return "ABCD";
        case 0x7200: return "ABCDE";
        case 0x7300: return "ABCDEF";
        
        // Subzonas A
        case 0x6C50: return "A1";
        case 0x6C80: return "A2";
        case 0x6C90: return "A3";
        case 0x6CA0: return "A4";
        case 0x6CB0: return "A5";
        case 0x6CC0: return "A6";
        
        // Subzonas B
        case 0x6280: return "B2";
        case 0x6290: return "B3";
        case 0x62A0: return "B4";
        case 0x62B0: return "B5";
        case 0x62C0: return "B6";
        
        // Subzonas C
        case 0x6A80: return "C2";
        case 0x6A90: return "C3";
        case 0x6AA0: return "C4";
        case 0x6AB0: return "C5";
        case 0x6AC0: return "C6";
        case 0x7260: return "C7";
        
        // Zonas numericas
        case 0x8100: case 0x8200: return "1";
        case 0x8110: case 0x8210: return "2";
        case 0x8120: case 0x8220: return "3";
        case 0x8130: return "4";
        case 0x8140: return "5";
        case 0x8150: return "6";
        case 0x6700: return "7";
        case 0x6900: return "8";
        case 0x6B00: return "9";  // Changed from 0x6A00 to avoid duplicate
        
        // Lineas de Metro
        case 0x9200: case 0xA100: case 0x0C00: return "L1";
        case 0x9210: case 0xA110: case 0x0C10: return "L2";
        case 0xA120: return "L3";
        case 0x6040: return "L4";
        case 0x6050: return "L6";
        case 0x6060: return "L8";
        case 0x6070: return "L9";
        
        // Euskotren
        case 0xA200: return "E1";
        case 0xA210: return "E2";
        case 0xA220: return "E3";
        
        // Zonas especiales
        case 0x9100: case 0x9300: return "Centro";
        case 0x9110: return "Periférico";
        case 0x9310: return "Norte";
        case 0x0B00: return "M1";
        case 0x0B10: return "M2";
        
        // RENFE Especiales
        case 0xF000: return "Cercanías Nacional";
        case 0xF100: return "AVE Nacional";
        case 0xF200: return "Media Distancia";
        case 0xF300: return "Larga Distancia";
        
        case 0xEC00: return "ABC";
        case 0x0000: return "N/A";
        default: return "Unknown";
    }
}

// Parse a single history entry
static void renfe_sum10_parse_history_entry(FuriString* parsed_data, const uint8_t* block_data, int entry_num) {
    // Check for null pointers
    if(!parsed_data || !block_data) {
        return;
    }
    
    // Extract transaction type from first byte
    uint8_t transaction_type = block_data[0];
    
    // Transaction type mappings
    // 0x13=Entry, 0x1A=Exit, 0x1E=Transfer, 0x16=Validation, 0x33=Top-up, etc.
    
    // Extract station codes (bytes 9-10) - Read as big-endian
    uint16_t station_code = (block_data[9] << 8) | block_data[10];
    
    // Format the entry
    furi_string_cat_printf(parsed_data, "%d. ", entry_num);
    
    // Interpret transaction type
    switch(transaction_type) {
        case 0x13:
            furi_string_cat_printf(parsed_data, "Entry");
            break;
        case 0x1A:
            furi_string_cat_printf(parsed_data, "Exit");
            break;
        case 0x1E:
            furi_string_cat_printf(parsed_data, "Transfer");
            break;
        case 0x16:
            furi_string_cat_printf(parsed_data, "Validation");
            break;
        case 0x17:
            furi_string_cat_printf(parsed_data, "Inspection");
            break;
        case 0x23:
            furi_string_cat_printf(parsed_data, "Discount");
            break;
        case 0x2A:
            furi_string_cat_printf(parsed_data, "Penalty");
            break;
        case 0x33:
            furi_string_cat_printf(parsed_data, "Top-up");
            break;
        case 0x3A:
            furi_string_cat_printf(parsed_data, "Charge");
            break;
        case 0x18:
            furi_string_cat_printf(parsed_data, "Check");
            break;
        case 0x2B:
            furi_string_cat_printf(parsed_data, "Special");
            break;
        default:
            furi_string_cat_printf(parsed_data, "Unknown");
            break;
    }
    // Add station information
    const char* station_name = renfe_sum10_get_station_name(station_code);
    if(station_code != 0x0000 && strlen(station_name) > 0) {
        furi_string_cat_printf(parsed_data, " - %s", station_name);
        if(strcmp(station_name, "Unknown") == 0) {
            furi_string_cat_printf(parsed_data, " (Code: %04X)", station_code);
        }
    }
    
    furi_string_cat_printf(parsed_data, "\n");
}
// Parse travel history from the card data (with sorting and strict validation)
static void renfe_sum10_parse_travel_history(FuriString* parsed_data, const MfClassicData* data) {
    // Check for null pointers
    if(!parsed_data || !data) {
        return;
    }
    
    // Define specific blocks that contain history
    int history_blocks[] = {18, 22, 28, 29, 30, 44, 45, 46};
    int num_blocks = sizeof(history_blocks) / sizeof(history_blocks[0]);
    
    // Array to store found history entries for sorting
    HistoryEntry* history_entries = malloc(sizeof(HistoryEntry) * num_blocks);
    if(!history_entries) {
        furi_string_cat_printf(parsed_data, "Memory allocation error\n");
        return;
    }
    
    int history_count = 0;
    
    int max_blocks = (data->type == MfClassicType1k) ? 64 : 256;
    
    // Collect all valid history entries with strict validation
    for(int i = 0; i < num_blocks; i++) {
        int block = history_blocks[i];
        
        // Check if block number is within valid range for this card type
        if(block >= max_blocks) {
            continue;
        }
        
        // Check if block was actually read
        if(!mf_classic_is_block_read(data, block)) {
            continue;
        }
        
        const uint8_t* block_data = data->block[block].data;
        
        // Use strict block-specific validation to avoid false positives
        if(renfe_sum10_is_travel_history_block(block_data, block)) {
            
            // Store the entry for sorting
            memcpy(history_entries[history_count].block_data, block_data, 16);
            history_entries[history_count].timestamp = renfe_sum10_extract_timestamp(block_data);
            history_entries[history_count].block_number = block;
            
            history_count++;
        }
    }
    
    if(history_count == 0) {
        // Don't add any message here - let the calling function handle it
    } else {
        // Sort the history entries by timestamp (newest first)
        renfe_sum10_sort_history_entries(history_entries, history_count);
        
        // Display the sorted history
        furi_string_cat_printf(parsed_data, "Travel History (%d trips):\n", history_count);
        furi_string_cat_printf(parsed_data, "(Most recent first)\n\n");
        
        for(int i = 0; i < history_count; i++) {
            
            furi_string_cat_printf(parsed_data, "Trip %d:\n", i + 1);
            renfe_sum10_parse_history_entry(parsed_data, history_entries[i].block_data, i + 1);
            furi_string_cat_printf(parsed_data, "\n");
        }
    }
    
    // Free allocated memory
    free(history_entries);
}

// Parse only travel history (called when user presses LEFT button)
typedef struct {
    uint8_t data_sector;
    const MfClassicKeyPair* keys;
} RenfeSum10CardConfig;

static bool renfe_sum10_get_card_config(RenfeSum10CardConfig* config, MfClassicType type) {
    bool success = true;

    if(type == MfClassicType1k) {
        config->data_sector = 5; // Primary data sector for trips
        config->keys = renfe_sum10_keys;
    } else if(type == MfClassicType4k) {
        // Mifare Plus 2K/4K configured as 1K for RENFE Suma 10 compatibility
        // Treat it exactly like a 1K card - only use the first 1K sectors
        config->data_sector = 5; // Same as 1K - RENFE only uses first 1K sectors
        config->keys = renfe_sum10_keys;
    } else {
        success = false; 
    }

    return success;
}

// Parse only travel history (called when user presses LEFT button)
static bool renfe_sum10_parse_history_only(FuriString* parsed_data, const MfClassicData* data) {
    // Check for null pointers
    if(!parsed_data || !data) {
        return false;
    }
    
    // Detect card variant for correct header
    const char* card_variant = renfe_sum10_detect_card_variant(data);
    
    // Clear existing data and show only travel history
    furi_string_reset(parsed_data);
    furi_string_cat_printf(parsed_data, "\e#RENFE %s\n", card_variant);
    furi_string_cat_printf(parsed_data, "\e#Travel History\n\n");
    
    // Check if we have history data first using strict validation
    bool has_history = renfe_sum10_has_history_data(data);
    
    if(!has_history) {
        // Show a clear message when no history is found
        furi_string_cat_printf(parsed_data, "No travel history found\n\n");
        furi_string_cat_printf(parsed_data, "This card appears to be:\n");
        furi_string_cat_printf(parsed_data, "• New or unused\n");
        furi_string_cat_printf(parsed_data, "• Recently reset\n");
        furi_string_cat_printf(parsed_data, "• Used only for balance\n\n");
        furi_string_cat_printf(parsed_data, "Note: The system filters out\n");
        furi_string_cat_printf(parsed_data, "false positives to ensure\n");
        furi_string_cat_printf(parsed_data, "only real trips are shown.\n");
    } else {
        // Parse travel history with strict validation
        renfe_sum10_parse_travel_history(parsed_data, data);
    }
    
    furi_string_cat_printf(parsed_data, "\n Press LEFT to return");
    
    return true;
}

// Check if a specific block likely contains travel history (not configuration)
static bool renfe_sum10_is_travel_history_block(const uint8_t* block_data, int block_number) {
    if(!block_data) return false;
    
    // Blocks 4-5 are more likely to contain configuration than real travel history
    // Apply stricter validation for these blocks
    if(block_number <= 5) {
        // For configuration blocks, require confirmed transaction patterns
        uint8_t transaction_type = block_data[0];
        
        // Only accept very specific confirmed transaction types in config blocks
        bool is_definite_transaction = (transaction_type == 0x13 || // ENTRY
                                      transaction_type == 0x1A);   // EXIT
        
        // Must have valid timestamp AND confirmed transaction type
        return is_definite_transaction && 
               renfe_sum10_is_valid_timestamp(block_data) &&
               !renfe_sum10_is_default_station_code((block_data[9] << 8) | block_data[10]);
    }
    
    // For higher numbered blocks, use standard validation
    return renfe_sum10_is_history_entry(block_data);
}

// Simple check if we have enough data to show travel history
static bool renfe_sum10_has_history_data(const MfClassicData* data) {
    if(!data) {
        return false;
    }
    
    // Use the same block list as the parsing function
    int history_blocks[] = {18, 22, 28, 29, 30, 44, 45, 46};
    int num_blocks = sizeof(history_blocks) / sizeof(history_blocks[0]);
    
    int max_blocks = (data->type == MfClassicType1k) ? 64 : 256;
    int valid_entries_found = 0;
    
    for(int i = 0; i < num_blocks; i++) {
        int block = history_blocks[i];
        
        // Check if block number is within valid range for this card type
        if(block >= max_blocks) {
            continue;
        }
        
        // Check if block was actually read
        if(!mf_classic_is_block_read(data, block)) {
            continue;
        }
        
        const uint8_t* block_data = data->block[block].data;
        if(!block_data) {
            continue;
        }
        
        // Use block-specific validation
        if(renfe_sum10_is_travel_history_block(block_data, block)) {
            valid_entries_found++;
            
            // Require at least 1 valid entry, but be more strict about what counts as valid
            if(valid_entries_found >= 1) {
                return true;
            }
        }
    }
    
    return false;
}

// Parse function for RENFE Suma 10 cards
static bool renfe_sum10_parse(FuriString* parsed_data, const MfClassicData* data) {
    // Check for null pointers
    if(!parsed_data) {
        return false;
    }
    
    if(!data) {
        return false;
    }
    
    bool parsed = false;
    RenfeSum10CardConfig cfg;
    
    do {
        memset(&cfg, 0, sizeof(cfg));
        if(!renfe_sum10_get_card_config(&cfg, data->type)) {
            break;
        }

        // Detect card variant and show appropriate header
        const char* card_variant = renfe_sum10_detect_card_variant(data);
        furi_string_cat_printf(parsed_data, "\e#🚆 RENFE %s\n", card_variant);
        
        // 1. Show card type info
        if(data->type == MfClassicType1k) {
            furi_string_cat_printf(parsed_data, "Type: Mifare Classic 1K\n");
        } else if(data->type == MfClassicType4k) {
            furi_string_cat_printf(parsed_data, "Type: Mifare Plus 2K\n");
        } else {
            furi_string_cat_printf(parsed_data, "Type: Unknown\n");
        }
        
        // 2. Extract and show UID (SECURE VERSION - following renfe_regular approach)
        if(data && data->iso14443_3a_data && data->iso14443_3a_data->uid_len > 0) {
            // UID available from live reading - show actual UID
            const uint8_t* uid = data->iso14443_3a_data->uid;
            size_t uid_len = data->iso14443_3a_data->uid_len;
            
            furi_string_cat_printf(parsed_data, "UID: ");
            for(size_t i = 0; i < uid_len; i++) {
                furi_string_cat_printf(parsed_data, "%02X", uid[i]);
                if(i < uid_len - 1) {
                    furi_string_cat_printf(parsed_data, " ");
                }
            }
            furi_string_cat_printf(parsed_data, "\n");
        } else {
            // Try to extract UID from block 0 as fallback
            if(mf_classic_is_block_read(data, 0)) {
                const uint8_t* block0 = data->block[0].data;
                
                // Additional null pointer safety check
                if(block0 != NULL) {
                    furi_string_cat_printf(parsed_data, "UID: ");
                    
                    if(block0[0] == 0x88) {
                        // 7-byte UID: Skip cascade tag (0x88), take next 3 bytes
                        for(int i = 1; i < 4; i++) {
                            furi_string_cat_printf(parsed_data, "%02X ", block0[i]);
                        }
                        furi_string_cat_printf(parsed_data, "XX XX XX XX");
                    } else {
                        // 4-byte UID
                        for(int i = 0; i < 4; i++) {
                            furi_string_cat_printf(parsed_data, "%02X", block0[i]);
                            if(i < 3) {
                                furi_string_cat_printf(parsed_data, " ");
                            }
                        }
                    }
                    furi_string_cat_printf(parsed_data, "\n");
                } else {
                    furi_string_cat_printf(parsed_data, "UID: Block 0 unavailable\n");
                }
            } else {
                furi_string_cat_printf(parsed_data, "UID: N/A\n");
            }
        }

        // 3. Show card variant-specific information
        if(strcmp(card_variant, "MOBILIS 30") == 0) {
            furi_string_cat_printf(parsed_data, "Variant: Monthly Pass\n");
            
            // Extract cardholder name from Block 9 if available
            if(mf_classic_is_block_read(data, 9)) {
                const uint8_t* block9 = data->block[9].data;
                char name_buffer[16] = {0};
                bool has_valid_name = false;
                
                // Extract name (typically in first part of block 9, handling accents)
                for(size_t i = 0; i < 6 && i < sizeof(name_buffer) - 1; i++) {
                    char normalized_char = renfe_sum10_normalize_accent(block9[i]);
                    
                    if(normalized_char >= 'A' && normalized_char <= 'Z') {
                        // Keep uppercase letters as-is
                        name_buffer[i] = normalized_char;
                        has_valid_name = true;
                    } else if(normalized_char >= 'a' && normalized_char <= 'z') {
                        // Convert to proper case (first letter uppercase)
                        name_buffer[i] = (i == 0) ? (normalized_char - 'a' + 'A') : normalized_char;
                        has_valid_name = true;
                    } else if(normalized_char == 0) {
                        // End of string
                        break;
                    } else if(block9[i] >= 0x20 && block9[i] <= 0x7E) {
                        // Other printable ASCII
                        name_buffer[i] = (char)block9[i];
                        has_valid_name = true;
                    } else {
                        // Invalid character, stop processing
                        break;
                    }
                }
                
                if(has_valid_name) {
                    furi_string_cat_printf(parsed_data, "👤 Holder: %s", name_buffer);
                    
                    // Try to extract surname(s) from Block 14
                    if(mf_classic_is_block_read(data, 14)) {
                        const uint8_t* block14 = data->block[14].data;
                        char surname_buffer[32] = {0}; // Increased buffer for both surnames
                        bool has_valid_surname = false;
                        size_t surname_pos = 0;
                        int words_found = 0;
                        bool in_word = false;
                        
                        // First pass: scan entire block for text patterns
                        int consecutive_non_letters = 0;
                        for(size_t i = 0; i < 16 && surname_pos < sizeof(surname_buffer) - 2; i++) {
                            char normalized_char = renfe_sum10_normalize_accent(block14[i]);
                            
                            if(normalized_char >= 'A' && normalized_char <= 'Z') {
                                // Valid uppercase letter
                                consecutive_non_letters = 0;
                                if(!in_word && words_found > 0 && surname_pos > 0 && surname_buffer[surname_pos - 1] != ' ') {
                                    surname_buffer[surname_pos++] = ' '; // Add space before new word
                                }
                                surname_buffer[surname_pos++] = normalized_char;
                                has_valid_surname = true;
                                in_word = true;
                            } else if(normalized_char >= 'a' && normalized_char <= 'z') {
                                // Convert lowercase to uppercase for surnames
                                consecutive_non_letters = 0;
                                if(!in_word && words_found > 0 && surname_pos > 0 && surname_buffer[surname_pos - 1] != ' ') {
                                    surname_buffer[surname_pos++] = ' '; // Add space before new word
                                }
                                surname_buffer[surname_pos++] = normalized_char - 'a' + 'A';
                                has_valid_surname = true;
                                in_word = true;
                            } else if(normalized_char == ' ' && in_word) {
                                // Space - end of current word
                                consecutive_non_letters = 0;
                                if(surname_pos > 0 && surname_buffer[surname_pos - 1] != ' ') {
                                    surname_buffer[surname_pos++] = ' ';
                                }
                                words_found++;
                                in_word = false;
                            } else if((normalized_char == 0 || 
                                     (block14[i] != 0x20 && block14[i] < 0x20) ||
                                     (block14[i] > 0x7E && normalized_char == 0)) && in_word) {
                                // End of word due to non-printable character
                                if(surname_pos > 0 && surname_buffer[surname_pos - 1] != ' ') {
                                    surname_buffer[surname_pos++] = ' ';
                                }
                                words_found++;
                                in_word = false;
                                consecutive_non_letters++;
                            } else if(normalized_char == 0 && !in_word) {
                                // Skip null bytes between words
                                consecutive_non_letters++;
                                continue;
                            } else {
                                // Other characters (numbers, symbols, etc.)
                                consecutive_non_letters++;
                                if(consecutive_non_letters >= 2 && words_found >= 1) {
                                    // Stop if we have consecutive non-letter characters and at least one word
                                    break;
                                }
                                if(in_word) {
                                    // End current word if we encounter non-letter
                                    if(surname_pos > 0 && surname_buffer[surname_pos - 1] != ' ') {
                                        surname_buffer[surname_pos++] = ' ';
                                    }
                                    words_found++;
                                    in_word = false;
                                }
                            }
                        }
                        
                        // Mark end of last word if we were in one
                        if(in_word) {
                            words_found++;
                        }
                        
                        // Clean up trailing spaces and unwanted characters
                        while(surname_pos > 0 && 
                              (surname_buffer[surname_pos - 1] == ' ' || 
                               surname_buffer[surname_pos - 1] == 'U' || 
                               surname_buffer[surname_pos - 1] == 'u')) {
                            surname_buffer[--surname_pos] = '\0';
                        }
                        
                        // Additional cleanup: remove any single letter at the end if preceded by space
                        if(surname_pos >= 3 && surname_buffer[surname_pos - 2] == ' ' && 
                           ((surname_buffer[surname_pos - 1] >= 'A' && surname_buffer[surname_pos - 1] <= 'Z') ||
                            (surname_buffer[surname_pos - 1] >= 'a' && surname_buffer[surname_pos - 1] <= 'z'))) {
                            // Check if it's likely a stray character (single letter after space)
                            surname_buffer[surname_pos - 2] = '\0';
                            surname_pos -= 2;
                        }
                        
                        // Final cleanup of trailing spaces again
                        while(surname_pos > 0 && surname_buffer[surname_pos - 1] == ' ') {
                            surname_buffer[--surname_pos] = '\0';
                        }
                        
                        // Accept if we have at least 3 characters and at least one word
                        if(has_valid_surname && surname_pos >= 3) {
                            furi_string_cat_printf(parsed_data, " %s", surname_buffer);
                        }
                    }
                    
                    furi_string_cat_printf(parsed_data, "\n");
                    
                }
            }
        } else {
            furi_string_cat_printf(parsed_data, "Variant: Pay-per-trip\n");
        }
        
        // 4. Extract and show origin station information (where card was first topped up)
        const char* origin_station = renfe_sum10_get_origin_station(data);
        if(origin_station && strcmp(origin_station, "Unknown") != 0) {
            furi_string_cat_printf(parsed_data, "Origin: %s\n", origin_station);
        } else {
            furi_string_cat_printf(parsed_data, "Origin: Unknown\n");
        }
        
        // 5. Extract and show zone information
        uint16_t zone_code = 0x0000;
        const char* zone_name = "N/A";
        
        // For MOBILIS 30, use specialized zone extraction
        if(strcmp(card_variant, "MOBILIS 30") == 0) {
            zone_code = renfe_sum10_extract_mobilis_zone_code(data);
            zone_name = renfe_sum10_get_zone_name(zone_code);
        } else {
            // For SUMA 10, use standard Block 5 extraction
            if(mf_classic_is_block_read(data, 5)) {
                const uint8_t* block5 = data->block[5].data;
                if(block5 != NULL) {
                    zone_code = renfe_sum10_extract_zone_code(block5);
                    zone_name = renfe_sum10_get_zone_name(zone_code);
                }
            }
        }
        
        if(zone_code != 0x0000) {
            furi_string_cat_printf(parsed_data, "Zone: %s\n", zone_name ? zone_name : "Error");
        } else {
            furi_string_cat_printf(parsed_data, "Zone: %s\n", zone_name ? zone_name : "Error");
        }
        
        // 6. Extract and show trips from Block 5
        if(mf_classic_is_block_read(data, 5)) {
            const uint8_t* block5 = data->block[5].data;
            if(block5 != NULL && block5[0] == 0x01 && block5[1] == 0x00 && block5[2] == 0x00 && block5[3] == 0x00) {
                // Extract trip count from byte 4
                int viajes = (int)block5[4];
                furi_string_cat_printf(parsed_data, "Trips: %d\n", viajes);
            } else {
                furi_string_cat_printf(parsed_data, "Trips: N/A\n");
            }
        } else {
            furi_string_cat_printf(parsed_data, "Trips: N/A\n");
        }
        
        // Add travel history status prominently (visible above buttons)
        furi_string_cat_printf(parsed_data, "\n");
        if(renfe_sum10_has_history_data(data)) {
            furi_string_cat_printf(parsed_data, "History: Available\n");
            furi_string_cat_printf(parsed_data, "   ⬅ Press LEFT to view details\n");
        } else {
            furi_string_cat_printf(parsed_data, "History: Empty/Not found\n");
            furi_string_cat_printf(parsed_data, "   (New card or cleared history)\n");
        }
        
        parsed = true;
        
    } while(false);

    return parsed;
}

// Widget callback function for handling button events (only for History button)
static void renfe_sum10_widget_callback(GuiButtonType result, InputType type, void* context) {
    if(!context) {
        return;
    }
    
    Metroflip* app = context;
    if(!app->view_dispatcher) {
        return;
    }
    
    if(type == InputTypeShort) {
        view_dispatcher_send_custom_event(app->view_dispatcher, result);
    }
}

static NfcCommand renfe_sum10_poller_callback(NfcGenericEvent event, void* context) {
    // Check for null pointers instead of asserting
    if(!context) {
        return NfcCommandStop;
    }
    
    if(!event.event_data) {
        return NfcCommandStop;
    }
    
    if(event.protocol != NfcProtocolMfClassic) {
        return NfcCommandStop;
    }

    NfcCommand command = NfcCommandContinue;
    const MfClassicPollerEvent* mfc_event = event.event_data;
    Metroflip* app = context;

    if(mfc_event->type == MfClassicPollerEventTypeCardDetected) {
        // Ensure nfc_device is allocated
        if(!app->nfc_device) {
            app->nfc_device = nfc_device_alloc();
        }
        
        view_dispatcher_send_custom_event(app->view_dispatcher, MetroflipCustomEventCardDetected);
        command = NfcCommandContinue;
    } else if(mfc_event->type == MfClassicPollerEventTypeCardLost) {
        view_dispatcher_send_custom_event(app->view_dispatcher, MetroflipCustomEventCardLost);
        app->sec_num = 0;
        command = NfcCommandStop;
    } else if(mfc_event->type == MfClassicPollerEventTypeRequestMode) {
        mfc_event->data->poller_mode.mode = MfClassicPollerModeRead;

    } else if(mfc_event->type == MfClassicPollerEventTypeRequestReadSector) {
        // Set data during sector reading (like working version)
        nfc_device_set_data(app->nfc_device, NfcProtocolMfClassic, nfc_poller_get_data(app->poller));
        const MfClassicData* mfc_data = nfc_device_get_data(app->nfc_device, NfcProtocolMfClassic);
        
        // Determine maximum sectors based on card type
        uint8_t max_sectors = 16; // Default for 1K
        if(mfc_data && mfc_data->type == MfClassicType4k) {
            max_sectors = 16; // RENFE Suma 10 only uses first 16 sectors even on 4K cards
        }
        
        // Check if we have more sectors to read
        if(app->sec_num < max_sectors) {
            MfClassicKey key = {0};
            MfClassicKeyType key_type = MfClassicKeyTypeA;
            
            // Verify sector number is within key array bounds
            if(app->sec_num >= sizeof(renfe_sum10_keys) / sizeof(renfe_sum10_keys[0])) {
                return NfcCommandStop;
            }
            
            // Use the correct key for this sector
            bit_lib_num_to_bytes_be(renfe_sum10_keys[app->sec_num].a, COUNT_OF(key.data), key.data);
            
            mfc_event->data->read_sector_request_data.sector_num = app->sec_num;
            mfc_event->data->read_sector_request_data.key = key;
            mfc_event->data->read_sector_request_data.key_type = key_type;
            mfc_event->data->read_sector_request_data.key_provided = true;
            
            app->sec_num++;
        } else {
            // No more sectors to read
            mfc_event->data->read_sector_request_data.key_provided = false;
            app->sec_num = 0;
        }
    } else if(mfc_event->type == MfClassicPollerEventTypeSuccess) {
        // Handle successful completion - parse and display immediately (like working version)
        
        if(!app->nfc_device) {
            return NfcCommandStop;
        }
        
        const MfClassicData* mfc_data = nfc_device_get_data(app->nfc_device, NfcProtocolMfClassic);
        if(!mfc_data) {
            return NfcCommandStop;
        }
        
        FuriString* parsed_data = furi_string_alloc();
        if(!parsed_data) {
            return NfcCommandStop;
        }
        
        Widget* widget = app->widget;
        if(!widget) {
            furi_string_free(parsed_data);
            return NfcCommandStop;
        }
        
        if(!renfe_sum10_parse(parsed_data, mfc_data)) {
            furi_string_reset(app->text_box_store);
            furi_string_printf(parsed_data, "\e#Unknown card\n");
        }
        
        widget_add_text_scroll_element(widget, 0, 0, 128, 52, furi_string_get_cstr(parsed_data));
        
        // Always show History button - if no history found, we'll show a message
        // Use the standard Metroflip callbacks in the poller success
        widget_add_button_element(widget, GuiButtonTypeLeft, "History", renfe_sum10_widget_callback, app);
        widget_add_button_element(widget, GuiButtonTypeCenter, "Save", metroflip_save_widget_callback, app);
        widget_add_button_element(widget, GuiButtonTypeRight, "Exit", metroflip_exit_widget_callback, app);

        furi_string_free(parsed_data);
        
        if(app->view_dispatcher) {
            view_dispatcher_switch_to_view(app->view_dispatcher, MetroflipViewWidget);
        }
        metroflip_app_blink_stop(app);
        command = NfcCommandStop;
    } else if(mfc_event->type == MfClassicPollerEventTypeFail) {
        command = NfcCommandStop;
    }

    return command;
}

static void renfe_sum10_on_enter(Metroflip* app) {
    if(!app) {
        return;
    }
    
    dolphin_deed(DolphinDeedNfcRead);

    app->sec_num = 0;

    if(app->data_loaded) {
        MfClassicData* mfc_data = NULL;
        bool should_free_mfc_data = false;
        
        // Load from file (original behavior)
        Storage* storage = furi_record_open(RECORD_STORAGE);
        FlipperFormat* ff = flipper_format_file_alloc(storage);
        if(flipper_format_file_open_existing(ff, app->file_path)) {
            mfc_data = mf_classic_alloc();
            mf_classic_load(mfc_data, ff, 2);
            should_free_mfc_data = true;
        }
        flipper_format_free(ff);
        furi_record_close(RECORD_STORAGE);
        
        
        if(mfc_data) {
            FuriString* parsed_data = furi_string_alloc();
            Widget* widget = app->widget;

            if(!widget) {
                FURI_LOG_E(TAG, "RENFE: Widget is NULL!");
                if(should_free_mfc_data) mf_classic_free(mfc_data);
                furi_string_free(parsed_data);
                return;
            }

            if(!app->text_box_store) {
                if(should_free_mfc_data) mf_classic_free(mfc_data);
                furi_string_free(parsed_data);
                return;
            }

            furi_string_reset(app->text_box_store);
            if(!renfe_sum10_parse(parsed_data, mfc_data)) {
                furi_string_reset(app->text_box_store);
                furi_string_printf(parsed_data, "\e#Unknown card\n");
            }
            widget_add_text_scroll_element(widget, 0, 0, 128, 52, furi_string_get_cstr(parsed_data));

            // Use the standard Metroflip callbacks instead of custom ones
            widget_add_button_element(widget, GuiButtonTypeLeft, "History", renfe_sum10_widget_callback, app);
            widget_add_button_element(widget, GuiButtonTypeCenter, "Delete", metroflip_delete_widget_callback, app);
            widget_add_button_element(widget, GuiButtonTypeRight, "Exit", metroflip_exit_widget_callback, app);
            
            if(!app->view_dispatcher) {
                if(should_free_mfc_data) mf_classic_free(mfc_data);
                furi_string_free(parsed_data);
                return;
            }
            
            if(should_free_mfc_data) mf_classic_free(mfc_data);
            furi_string_free(parsed_data);
            
            view_dispatcher_switch_to_view(app->view_dispatcher, MetroflipViewWidget);
        } else {
        }
    } else {
        // Setup view
        Popup* popup = app->popup;
        if(!popup) {
            return;
        }
        
        if(!app->view_dispatcher) {
            return;
        }
        
        popup_set_header(popup, "Apply\n card to\nthe back", 68, 30, AlignLeft, AlignTop);
        popup_set_icon(popup, 0, 3, &I_RFIDDolphinReceive_97x61);

        // Start worker
        view_dispatcher_switch_to_view(app->view_dispatcher, MetroflipViewPopup);
        
        app->poller = nfc_poller_alloc(app->nfc, NfcProtocolMfClassic);
        if(!app->poller) {
            return;
        }
        
        nfc_poller_start(app->poller, renfe_sum10_poller_callback, app);

        metroflip_app_blink_start(app);
    }
}

static bool renfe_sum10_on_event(Metroflip* app, SceneManagerEvent event) {
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == MetroflipCustomEventCardDetected) {
            if(app->popup) {
                Popup* popup = app->popup;
                popup_set_header(popup, "DON'T\nMOVE", 68, 30, AlignLeft, AlignTop);
            }
            consumed = true;
        } else if(event.event == MetroflipCustomEventCardLost) {
            if(app->popup) {
                Popup* popup = app->popup;
                popup_set_header(popup, "Card \n lost", 68, 30, AlignLeft, AlignTop);
            }
            consumed = true;
        } else if(event.event == MetroflipCustomEventWrongCard) {
            if(app->popup) {
                Popup* popup = app->popup;
                popup_set_header(popup, "WRONG \n CARD", 68, 30, AlignLeft, AlignTop);
            }
            consumed = true;
        } else if(event.event == MetroflipCustomEventPollerFail) {
            if(app->popup) {
                Popup* popup = app->popup;
                popup_set_header(popup, "Failed", 68, 30, AlignLeft, AlignTop);
            }
            consumed = true;
        } else if(event.event == GuiButtonTypeLeft) {
            // Handle LEFT button press to show travel history
            if(app->data_loaded) {
                MfClassicData* mfc_data = NULL;
                bool should_free_mfc_data = false;
                
                // Load from file (original behavior)
                Storage* storage = furi_record_open(RECORD_STORAGE);
                FlipperFormat* ff = flipper_format_file_alloc(storage);
                if(flipper_format_file_open_existing(ff, app->file_path)) {
                    mfc_data = mf_classic_alloc();
                    mf_classic_load(mfc_data, ff, 2);
                    should_free_mfc_data = true;
                }
                flipper_format_free(ff);
                furi_record_close(RECORD_STORAGE);

                
                if(mfc_data) {
                    FuriString* parsed_data = furi_string_alloc();
                    Widget* widget = app->widget;

                    if(widget) {
                        // Reset widget and show only travel history
                        widget_reset(widget);
                        if(!renfe_sum10_parse_history_only(parsed_data, mfc_data)) {
                            furi_string_reset(parsed_data);
                            furi_string_printf(parsed_data, "\e#Travel History\nNo history found\n");
                        }
                        widget_add_text_scroll_element(widget, 0, 0, 128, 52, furi_string_get_cstr(parsed_data));
                        widget_add_button_element(widget, GuiButtonTypeRight, "Back", renfe_sum10_widget_callback, app);
                        view_dispatcher_switch_to_view(app->view_dispatcher, MetroflipViewWidget);
                    }

                    if(should_free_mfc_data) mf_classic_free(mfc_data);
                    furi_string_free(parsed_data);
                }
            } else {
                // Card is connected - use already read data
                const MfClassicData* mfc_data = nfc_device_get_data(app->nfc_device, NfcProtocolMfClassic);
                if(mfc_data && app->widget) {
                    FuriString* parsed_data = furi_string_alloc();
                    Widget* widget = app->widget;
                    
                    // Reset widget and show travel history
                    widget_reset(widget);
                    if(!renfe_sum10_parse_history_only(parsed_data, mfc_data)) {
                        furi_string_reset(parsed_data);
                        furi_string_printf(parsed_data, "\e#Travel History\nNo history found\n");
                    }
                    widget_add_text_scroll_element(widget, 0, 0, 128, 52, furi_string_get_cstr(parsed_data));
                    widget_add_button_element(widget, GuiButtonTypeRight, "Back", renfe_sum10_widget_callback, app);
                    
                    furi_string_free(parsed_data);
                    view_dispatcher_switch_to_view(app->view_dispatcher, MetroflipViewWidget);
                }
            }
            consumed = true;
        } else if(event.event == GuiButtonTypeRight) {
            // Handle RIGHT button press - go back to main card view from history
            if(app->data_loaded) {
                MfClassicData* mfc_data = NULL;
                bool should_free_mfc_data = false;
                
                // Load from file (original behavior)
                Storage* storage = furi_record_open(RECORD_STORAGE);
                FlipperFormat* ff = flipper_format_file_alloc(storage);
                if(flipper_format_file_open_existing(ff, app->file_path)) {
                    mfc_data = mf_classic_alloc();
                    mf_classic_load(mfc_data, ff, 2);
                    should_free_mfc_data = true;
                }
                flipper_format_free(ff);
                furi_record_close(RECORD_STORAGE);
                
                
                if(mfc_data) {
                    FuriString* parsed_data = furi_string_alloc();
                    Widget* widget = app->widget;

                    if(widget) {
                        widget_reset(widget);
                        if(!renfe_sum10_parse(parsed_data, mfc_data)) {
                            furi_string_printf(parsed_data, "\e#Unknown card\n");
                        }
                        widget_add_text_scroll_element(widget, 0, 0, 128, 52, furi_string_get_cstr(parsed_data));

                        widget_add_button_element(widget, GuiButtonTypeLeft, "History", renfe_sum10_widget_callback, app);
                        widget_add_button_element(widget, GuiButtonTypeCenter, "Delete", metroflip_delete_widget_callback, app);
                        widget_add_button_element(widget, GuiButtonTypeRight, "Exit", metroflip_exit_widget_callback, app);
                        
                        view_dispatcher_switch_to_view(app->view_dispatcher, MetroflipViewWidget);
                    }
                    
                    if(should_free_mfc_data) mf_classic_free(mfc_data);
                    furi_string_free(parsed_data);
                }
            } else {
                // Card is connected - use already read data
                const MfClassicData* mfc_data = nfc_device_get_data(app->nfc_device, NfcProtocolMfClassic);
                if(mfc_data && app->widget) {
                    FuriString* parsed_data = furi_string_alloc();
                    Widget* widget = app->widget;
                    
                    widget_reset(widget);
                    if(!renfe_sum10_parse(parsed_data, mfc_data)) {
                        furi_string_printf(parsed_data, "\e#Unknown card\n");
                    }
                    widget_add_text_scroll_element(widget, 0, 0, 128, 52, furi_string_get_cstr(parsed_data));
                    
                    widget_add_button_element(widget, GuiButtonTypeLeft, "History", renfe_sum10_widget_callback, app);
                    widget_add_button_element(widget, GuiButtonTypeCenter, "Save", metroflip_save_widget_callback, app);
                    widget_add_button_element(widget, GuiButtonTypeRight, "Exit", metroflip_exit_widget_callback, app);
                    
                    view_dispatcher_switch_to_view(app->view_dispatcher, MetroflipViewWidget);
                    furi_string_free(parsed_data);
                }
            }
            consumed = true;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        scene_manager_search_and_switch_to_previous_scene(app->scene_manager, MetroflipSceneStart);
        consumed = true;
    }

    return consumed;
}

static void renfe_sum10_on_exit(Metroflip* app) {
    if(!app) {
        return;
    }

    if(app->widget) {
        widget_reset(app->widget);
    }

    if(app->poller && !app->data_loaded) {
        nfc_poller_stop(app->poller);
        nfc_poller_free(app->poller);
        app->poller = NULL;
    }

    // Clear view
    if(app->popup) {
        popup_reset(app->popup);
    }

    // Clear station cache to free memory
    renfe_sum10_clear_station_cache();
    
    // Free the dynamically allocated station cache
    if(station_cache) {
        free(station_cache);
        station_cache = NULL;
    }

    metroflip_app_blink_stop(app);
}

static const MetroflipPlugin renfe_sum10_plugin = {
    .card_name = "RENFE Suma 10",
    .plugin_on_enter = renfe_sum10_on_enter,
    .plugin_on_event = renfe_sum10_on_event,
    .plugin_on_exit = renfe_sum10_on_exit,
};

static const FlipperAppPluginDescriptor renfe_sum10_plugin_descriptor = {
    .appid = METROFLIP_SUPPORTED_CARD_PLUGIN_APP_ID,
    .ep_api_version = METROFLIP_SUPPORTED_CARD_PLUGIN_API_VERSION,
    .entry_point = &renfe_sum10_plugin,
};

const FlipperAppPluginDescriptor* renfe_sum10_plugin_ep(void) {
    return &renfe_sum10_plugin_descriptor;
}
