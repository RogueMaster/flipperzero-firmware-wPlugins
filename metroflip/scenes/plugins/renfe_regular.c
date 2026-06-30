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

#define TAG "Metroflip:Scene:RenfeRegular"

typedef struct {
    uint8_t block_data[16];  
    uint32_t timestamp;     
    int block_number;       
} HistoryEntry;

// Station name cache structure
#define MAX_STATION_NAME_LENGTH 28
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

// Global station cache
static StationCache station_cache = {0};

// Forward declarations for helper functions
static bool renfe_regular_is_history_entry(const uint8_t* block_data);
static void renfe_regular_parse_history_entry(FuriString* parsed_data, const uint8_t* block_data, int entry_num);
static void renfe_regular_parse_travel_history(FuriString* parsed_data, const MfClassicData* data);
static uint32_t renfe_regular_extract_timestamp(const uint8_t* block_data);
static void renfe_regular_sort_history_entries(HistoryEntry* entries, int count);
static bool renfe_regular_has_history_data(const MfClassicData* data);
static bool renfe_regular_load_station_file(const char* region);
static const char* renfe_regular_get_station_name_from_cache(uint16_t station_code);
static void renfe_regular_clear_station_cache(void);
static const char* renfe_regular_get_station_name_dynamic(uint16_t station_code);
static bool renfe_regular_is_travel_history_block(const uint8_t* block_data, int block_number);
static bool renfe_regular_is_default_station_code(uint16_t station_code);
static bool renfe_regular_is_valid_timestamp(const uint8_t* block_data);
static const char* renfe_regular_get_region_from_data(const MfClassicData* data);
static const char* renfe_regular_detect_card_type(const MfClassicData* data);
static int renfe_regular_get_bono_trip_count(const MfClassicData* data);
static bool renfe_regular_is_ida_vuelta_card(const MfClassicData* data);

// Keys for RENFE Regular cards
const MfClassicKeyPair renfe_regular_keys[16] = {
    {.a = 0xffffffffffff, .b = 0xffffffffffff}, // Sector 0
    {.a = 0xffffffffffff, .b = 0xffffffffffff}, // Sector 1
    {.a = 0xffffffffffff, .b = 0xffffffffffff}, // Sector 2
    {.a = 0x747734CC8ED3, .b = 0x78778869ffff}, // Sector 3
    {.a = 0xffffffffffff, .b = 0xffffffffffff}, // Sector 4
    {.a = 0xffffffffffff, .b = 0xffffffffffff}, // Sector 5
    {.a = 0xffffffffffff, .b = 0xffffffffffff}, // Sector 6
    {.a = 0xffffffffffff, .b = 0x78778869ffff}, // Sector 7
    {.a = 0xffffffffffff, .b = 0xffffffffffff}, // Sector 8
    {.a = 0xffffffffffff, .b = 0xffffffffffff}, // Sector 9
    {.a = 0xffffffffffff, .b = 0xffffffffffff}, // Sector 10
    {.a = 0x749934CC8ED3, .b = 0x78778869ffff}, // Sector 11
    {.a = 0xffffffffffff, .b = 0xffffffffffff}, // Sector 12
    {.a = 0xffffffffffff, .b = 0xffffffffffff}, // Sector 13
    {.a = 0xffffffffffff, .b = 0xffffffffffff}, // Sector 14
    {.a = 0xffffffffffff, .b = 0xffffffffffff}, // Sector 15
};

// Check if a station code is problematic (appears in empty cards)
static bool renfe_regular_is_default_station_code(uint16_t station_code) {
    return (station_code == 0x0000 || // Null code
            station_code == 0xFFFF);  // Fill pattern
}

// Check if timestamp indicates a real transaction
static bool renfe_regular_is_valid_timestamp(const uint8_t* block_data) {
    if(!block_data) return false;
    
    // Check if timestamp bytes are all zeros
    if(block_data[2] == 0x00 && block_data[3] == 0x00 && block_data[4] == 0x00) {
        return false;
    }
    
    // Check if timestamp bytes are all 0xFF
    if(block_data[2] == 0xFF && block_data[3] == 0xFF && block_data[4] == 0xFF) {
        return false;
    }
    
    return true;
}

// Check if a block contains a valid history entry
static bool renfe_regular_is_history_entry(const uint8_t* block_data) {
    if(!block_data) {
        return false;
    }
    
    // Look for patterns
    // Pattern 1: 3D 80 A6 EF (Entry pattern)
    if(block_data[0] == 0x3D && block_data[1] == 0x80 && 
       block_data[2] == 0xA6 && block_data[3] == 0xEF) {
        return true;
    }
    
    // Pattern 2: 3C 80 DE EF (Exit pattern)
    if(block_data[0] == 0x3C && block_data[1] == 0x80 && 
       block_data[2] == 0xDE && block_data[3] == 0xEF) {
        return true;
    }
    
    // Pattern 3: 3D 80 DE EF (Transfer pattern)
    if(block_data[0] == 0x3D && block_data[1] == 0x80 && 
       block_data[2] == 0xDE && block_data[3] == 0xEF) {
        return true;
    }
    
    // Pattern 4: 3C 80 A6 EF (Alternative pattern)
    if(block_data[0] == 0x3C && block_data[1] == 0x80 && 
       block_data[2] == 0xA6 && block_data[3] == 0xEF) {
        return true;
    }
    
    // Additional validation: check if there's meaningful data in station bytes
    uint16_t station_code = (block_data[9] << 8) | block_data[10];
    if(renfe_regular_is_default_station_code(station_code)) {
        return false;
    }
    
    // If we have valid patterns, also check timestamp for additional validation
    if((block_data[0] == 0x3D || block_data[0] == 0x3C) && 
       (block_data[1] == 0x80) &&
       !renfe_regular_is_valid_timestamp(block_data)) {
        return false;
    }
    
    return false;
}

// Extract timestamp from block data for sorting
static uint32_t renfe_regular_extract_timestamp(const uint8_t* block_data) {
    if(!block_data) return 0;
    
    // Extract timestamp from bytes 4-6
    uint32_t timestamp = ((uint32_t)block_data[4] << 16) | 
                        ((uint32_t)block_data[5] << 8) | 
                        (uint32_t)block_data[6];
    
    return timestamp;
}

// Sort history entries (newest first)
static void renfe_regular_sort_history_entries(HistoryEntry* entries, int count) {
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
                HistoryEntry temp = entries[j];
                entries[j] = entries[j + 1];
                entries[j + 1] = temp;
            }
        }
    }
}

// Clear the station cache
static void renfe_regular_clear_station_cache(void) {
    station_cache.count = 0;
    station_cache.loaded = false;
    memset(station_cache.current_region, 0, sizeof(station_cache.current_region));
    memset(station_cache.stations, 0, sizeof(station_cache.stations));
}

// Load station names from file
static bool renfe_regular_load_station_file(const char* region) {
    if(!region) {
        return false;
    }
    
    // Check if we already have this file loaded
    if(station_cache.loaded && strcmp(station_cache.current_region, region) == 0) {
        return true;
    }
    
    // Clear existing cache
    renfe_regular_clear_station_cache();
    
    // Build file path
    FuriString* file_path = furi_string_alloc();
    furi_string_printf(file_path, "/ext/apps_assets/metroflip/renfe/stations/%s.txt", region);
    
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    
    bool success = false;
    if(storage_file_open(file, furi_string_get_cstr(file_path), FSAM_READ, FSOM_OPEN_EXISTING)) {
        char line_buffer[128];
        station_cache.count = 0;
        
        while(station_cache.count < MAX_CACHED_STATIONS) {
            size_t line_pos = 0;
            bool end_of_file = false;
            
            // Read line character by character
            while(line_pos < sizeof(line_buffer) - 1) {
                char c;
                size_t bytes_read = storage_file_read(file, &c, 1);
                if(bytes_read == 0) {
                    end_of_file = true;
                    break;
                }
                
                if(c == '\n') {
                    break;
                }
                
                if(c != '\r') {
                    line_buffer[line_pos++] = c;
                }
            }
            
            if(end_of_file && line_pos == 0) {
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
            if(comma && station_cache.count < MAX_CACHED_STATIONS) {
                *comma = '\0';
                char* code_str = line_buffer;
                char* name_str = comma + 1;
                
                // Parse hex code
                uint16_t code = 0;
                if(sscanf(code_str, "0x%hX", &code) == 1) {
                    station_cache.stations[station_cache.count].code = code;
                    strncpy(station_cache.stations[station_cache.count].name, name_str, MAX_STATION_NAME_LENGTH - 1);
                    station_cache.stations[station_cache.count].name[MAX_STATION_NAME_LENGTH - 1] = '\0';
                    station_cache.count++;
                }
            }
            
            if(end_of_file) break;
        }
        
        if(station_cache.count > 0) {
            strncpy(station_cache.current_region, region, sizeof(station_cache.current_region) - 1);
            station_cache.current_region[sizeof(station_cache.current_region) - 1] = '\0';
            station_cache.loaded = true;
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
static const char* renfe_regular_get_station_name_from_cache(uint16_t station_code) {
    if(!station_cache.loaded) {
        return "Unknown";
    }
    
    for(size_t i = 0; i < station_cache.count; i++) {
        if(station_cache.stations[i].code == station_code) {
            return station_cache.stations[i].name;
        }
    }
    
    return "Unknown";
}

// Dynamic station name lookup (optimized for Valencia)
static const char* renfe_regular_get_station_name_dynamic(uint16_t station_code) {
    if(station_code == 0x0000) {
        return "";
    }
    
    // Only load valencia once at startup if not already loaded
    if(!station_cache.loaded || strcmp(station_cache.current_region, "valencia") != 0) {
        renfe_regular_load_station_file("valencia");
    }
    
    // Search in currently loaded cache
    const char* station_name = renfe_regular_get_station_name_from_cache(station_code);
    if(strcmp(station_name, "Unknown") != 0) {
        return station_name;
    }
    
    return "Unknown";
}

// Detect card type
static const char* renfe_regular_detect_card_type(const MfClassicData* data) {
    if(!data) return "Unknown";
    
    // Check for Bono Regular 10 trips pattern FIRST (most specific)
    if(mf_classic_is_block_read(data, 12)) {
        const uint8_t* block12 = data->block[12].data;
        // Pattern for BONO: E8 03 XX (less strict detection)
        // Original pattern: E8 03 04 00 00 00 00 00 04 00 60 3A 01 00 00 B0
        // Relaxed pattern: E8 03 and byte 8 = trip counter
        if(block12[0] == 0xE8 && block12[1] == 0x03) {
            return "Bono Regular 10 trips";
        }
    }
    
    // Check for Cercanías patterns (more specific than regular)
    if(mf_classic_is_block_read(data, 2)) {
        const uint8_t* block2 = data->block[2].data;
        if(block2[2] == 0x5C && block2[3] == 0x9F) {
            return "RENFE Cercanias";

        }
    }
    
    // Check Block 8 for Regular patterns (less specific)
    if(mf_classic_is_block_read(data, 8)) {
        const uint8_t* block8 = data->block[8].data;
        if(block8[0] == 0x81 && block8[1] == 0xE2) {
            return "RENFE Regular";
        }
    }
    
    return "RENFE Tarjeta Regular";
}

// Check if card is ida/vuelta type (shouldn't show history button)
static bool renfe_regular_is_ida_vuelta_card(const MfClassicData* data) {
    if(!data) return false;
    
    if(mf_classic_is_block_read(data, 12)) {
        const uint8_t* block12 = data->block[12].data;
        // E4 02 pattern indicates ida/vuelta card
        if(block12[0] == 0xE4 && block12[1] == 0x02) {
            return true;
        }
        // 04 01 pattern also indicates ida/vuelta card (alternative format)
        if(block12[0] == 0x04 && block12[1] == 0x01) {
            return true;
        }
    }
    
    return false;
}

// Extract trip counter for all RENFE regular cards
static int renfe_regular_get_bono_trip_count(const MfClassicData* data) {
    if(!data) return -1;
    
    // Check Block 12 first - this is where the trip counter usually is
    if(mf_classic_is_block_read(data, 12)) {
        const uint8_t* block12 = data->block[12].data;
        
        // Detect card type from Block 12
        if(block12[0] == 0xE8 && block12[1] == 0x03) {
            // This is a BONO (multi-trip card)
            int trip_count = (int)block12[2];
            if(trip_count >= 0 && trip_count <= 10) {
                return trip_count;
            }
        } else if(block12[0] == 0xE4 && block12[1] == 0x02) {
            // This is a regular IDA/VUELTA card (format 1)
            int usage_status = (int)block12[2];
            
            // For ida/vuelta cards, we need to interpret the usage differently
            if(usage_status == 0x02) {
                return -2; // Special code to indicate "used"
            } else if(usage_status == 0x00) {
                return -1; // Special code to indicate "unused"
            } else {
                return -3; // Special code for unknown status
            }
        } else if(block12[0] == 0x04 && block12[1] == 0x01) {
            // This is a regular IDA/VUELTA card (format 2)
            int usage_status = (int)block12[2];
            
            // For ida/vuelta cards, we need to interpret the usage differently
            if(usage_status == 0x02) {
                return -2; // Special code to indicate "used"
            } else if(usage_status == 0x00) {
                return -1; // Special code to indicate "unused"
            } else {
                return -3; // Special code for unknown status
            }
        }
        
        // Fallback: try to interpret as regular trip counter
        int trip_count = (int)block12[2];
        if(trip_count >= 0 && trip_count <= 10) {
            return trip_count;
        }
    }
    
    // Check other common blocks for trip counter
    for(int block_num = 8; block_num <= 16; block_num++) {
        if(!mf_classic_is_block_read(data, block_num)) continue;
        
        const uint8_t* block_data = data->block[block_num].data;
        
        // Check byte 2 specifically as it's common for counters
        int potential_count = (int)block_data[2];
        if(potential_count >= 0 && potential_count <= 10) {
            return potential_count;
        }
    }
    
    return 0;
}

// Get region from card data
static const char* renfe_regular_get_region_from_data(const MfClassicData* data) {
    if(!data) return "Unknown";
    
    // Try to detect region from data patterns
    // Check specific blocks for regional patterns
    
    // Check Block 2 for regional indicators (Cercanías patterns)
    if(mf_classic_is_block_read(data, 2)) {
        const uint8_t* block2 = data->block[2].data;
        
        // Valencia region indicators
        if(block2[2] == 0x5C && block2[3] == 0x9F) {
            return "valencia";
        }
        
        // Madrid Cercanías patterns
        if(block2[2] == 0x5D && block2[3] == 0xA0) {
            return "madrid";
        }
        
        // Barcelona/Catalunya patterns  
        if(block2[2] == 0x5A && block2[3] == 0x9D) {
            return "cataluna";
        }
        
        // Sevilla/Andalucía patterns
        if(block2[2] == 0x5E && block2[3] == 0xA1) {
            return "andalucia";
        }
        
        // Bilbao/País Vasco patterns
        if(block2[2] == 0x5B && block2[3] == 0x9E) {
            return "pais_vasco";
        }
    }
    
    // Check Block 12-14 for regional patterns (Main line patterns)
    if(mf_classic_is_block_read(data, 12)) {
        const uint8_t* block12 = data->block[12].data;
        
        // Madrid region patterns (Atocha, Chamartín)
        if(block12[0] == 0xE4 && block12[1] == 0x02) {
            // Check for specific Madrid codes
            uint16_t code = (block12[10] << 8) | block12[11];
            if(code >= 0x1000 && code <= 0x1FFF) {
                return "madrid";
            }
        }
        
        // Catalunya patterns (Barcelona Sants, Girona)
        if(block12[2] == 0x02 && block12[3] == 0x00) {
            return "cataluna";
        }
        
        // Andalucía patterns (Sevilla, Córdoba, Málaga)
        if(block12[0] == 0xE5 || block12[0] == 0xE6) {
            return "andalucia";
        }
        
        // Aragón patterns (Zaragoza)
        if(block12[0] == 0xE3 && block12[1] == 0x01) {
            return "aragon";
        }
        
        // Castilla y León patterns (Valladolid, León, Salamanca)
        if(block12[0] == 0xE7 && (block12[1] == 0x03 || block12[1] == 0x04)) {
            return "castilla_leon";
        }
        
        // Galicia patterns (Santiago, Vigo, A Coruña)
        if(block12[0] == 0xE8 && block12[1] == 0x05) {
            return "galicia";
        }
        
        // Asturias patterns (Oviedo, Gijón)
        if(block12[0] == 0xE9 && block12[1] == 0x06) {
            return "asturias";
        }
        
        // País Vasco patterns (Bilbao, San Sebastián)
        if(block12[0] == 0xEA && block12[1] == 0x07) {
            return "pais_vasco";
        }
        
        // Cantabria patterns (Santander)
        if(block12[0] == 0xEB && block12[1] == 0x08) {
            return "cantabria";
        }
        
        // La Rioja patterns (Logroño)
        if(block12[0] == 0xEC && block12[1] == 0x09) {
            return "la_rioja";
        }
        
        // Navarra patterns (Pamplona)
        if(block12[0] == 0xED && block12[1] == 0x0A) {
            return "navarra";
        }
        
        // Murcia patterns (Murcia, Cartagena)
        if(block12[0] == 0xEE && block12[1] == 0x0B) {
            return "murcia";
        }
        
        // Castilla-La Mancha patterns (Toledo, Ciudad Real)
        if(block12[0] == 0xEF && block12[1] == 0x0C) {
            return "castilla_la_mancha";
        }
        
        // Extremadura patterns (Cáceres, Badajoz)
        if(block12[0] == 0xF0 && block12[1] == 0x0D) {
            return "extremadura";
        }
    }
    
    // Check Block 13-14 for additional regional indicators
    if(mf_classic_is_block_read(data, 13)) {
        const uint8_t* block13 = data->block[13].data;
        
        // País Vasco patterns (EuskoTren integration)
        if(block13[6] == 0x4D && block13[7] == 0xDF) {
            return "pais_vasco";
        }
        
        // Galicia patterns (FEVE integration)
        if(block13[0] >= 0x90 && block13[0] <= 0x9F) {
            return "galicia";
        }
        
        // Canarias patterns (when applicable)
        if(block13[8] == 0x60 && block13[9] == 0x70) {
            return "canarias";
        }
        
        // Baleares patterns (when applicable)  
        if(block13[8] == 0x61 && block13[9] == 0x71) {
            return "baleares";
        }
        
        // Ceuta patterns
        if(block13[10] == 0x80 && block13[11] == 0x90) {
            return "ceuta";
        }
        
        // Melilla patterns
        if(block13[10] == 0x81 && block13[11] == 0x91) {
            return "melilla";
        }
    }
    
    // Check Block 1 for additional region hints
    if(mf_classic_is_block_read(data, 1)) {
        const uint8_t* block1 = data->block[1].data;
        
        // Additional Catalunya detection (FGC integration)
        if(block1[12] == 0xCA && block1[13] == 0xAA) {
            return "cataluna";
        }
        
        // Additional Valencia detection (FGV integration)
        if(block1[12] == 0xBA && block1[13] == 0xEE) {
            return "valencia";
        }
    }
    
    // Default to valencia for the provided dumps (since user has Valencia dumps)
    return "valencia";
}

// Parse a single history entry
static void renfe_regular_parse_history_entry(FuriString* parsed_data, const uint8_t* block_data, int entry_num) {
    if(!parsed_data || !block_data) {
        return;
    }
    
    // Extract transaction type from pattern
    uint8_t pattern1 = block_data[0];
    uint8_t pattern2 = block_data[1];
    uint8_t pattern3 = block_data[2];
    uint8_t pattern4 = block_data[3];
    
    // Extract station code from different positions
    uint16_t station_code = 0;
    
    // Position candidates for station codes
    uint16_t pos_std = (block_data[9] << 8) | block_data[10];  // Standard position bytes 9-10
    uint16_t pos_end_single = block_data[15];  // Single byte 15
    uint16_t pos_end_pair = (block_data[14] << 8) | block_data[15];  // bytes 14-15
    uint16_t pos_alt1 = (block_data[10] << 8) | block_data[11];  // bytes 10-11
    uint16_t pos_alt2 = (block_data[11] << 8) | block_data[12];  // bytes 11-12
    uint16_t pos_mid = (block_data[7] << 8) | block_data[8];     // bytes 7-8
    
    // Try to find a reasonable station code using various strategies
    uint16_t candidates[] = {pos_end_single, pos_alt1, pos_alt2, pos_mid, pos_end_pair, pos_std};
    int num_candidates = sizeof(candidates) / sizeof(candidates[0]);
    
    // Strategy 1: Look for small values that could be station IDs (prefer single byte values under 0x100)
    for(int i = 0; i < num_candidates; i++) {
        uint16_t candidate = candidates[i];
        if(candidate > 0x00 && candidate < 0x100 && candidate != 0xFF) {
            station_code = candidate;
            break;
        }
    }
    
    // Strategy 2: If no small values found, look for any reasonable non-zero values
    if(station_code == 0) {
        for(int i = 0; i < num_candidates; i++) {
            uint16_t candidate = candidates[i];
            if(candidate > 0x00 && candidate < 0x8000 && candidate != 0xFFFF) {
                station_code = candidate;
                break;
            }
        }
    }
    
    // Strategy 3: Default to standard position if nothing else works
    if(station_code == 0) {
        station_code = pos_std;
    }
    
    // Extract additional transaction details
    uint8_t detail_byte = block_data[7];
    
    furi_string_cat_printf(parsed_data, "%d. ", entry_num);
    
    // Interpret transaction type
    if(pattern1 == 0x3D && pattern2 == 0x80 && pattern3 == 0xA6 && pattern4 == 0xEF) {
        furi_string_cat_printf(parsed_data, "Entry");
    } else if(pattern1 == 0x3C && pattern2 == 0x80 && pattern3 == 0xDE && pattern4 == 0xEF) {
        furi_string_cat_printf(parsed_data, "Exit");
    } else if(pattern1 == 0x3D && pattern2 == 0x80 && pattern3 == 0xDE && pattern4 == 0xEF) {
        furi_string_cat_printf(parsed_data, "Transfer");
    } else if(pattern1 == 0x3C && pattern2 == 0x80 && pattern3 == 0xA6 && pattern4 == 0xEF) {
        furi_string_cat_printf(parsed_data, "Validation");
    } else {
        furi_string_cat_printf(parsed_data, "Transaction");
    }
    
    // Add station information
    const char* station_name = renfe_regular_get_station_name_dynamic(station_code);
    
    if(station_code != 0x0000 && strlen(station_name) > 0) {
        furi_string_cat_printf(parsed_data, " - %s", station_name);
    } else {
        // Station name not available
    }
    
    // Add transaction details if available
    if(detail_byte != 0x00 && detail_byte != 0xFF) {
        if(detail_byte == 0x10) {
            furi_string_cat_printf(parsed_data, " (Standard)");
        } else if(detail_byte == 0x00) {
            furi_string_cat_printf(parsed_data, " (Free)");
        } else {
            furi_string_cat_printf(parsed_data, " (Special)");
        }
    }
    
    furi_string_cat_printf(parsed_data, "\n");
}

// Parse travel history from card data
static void renfe_regular_parse_travel_history(FuriString* parsed_data, const MfClassicData* data) {
    if(!parsed_data || !data) {
        return;
    }
    
    // Get the detected region for better station name resolution
    const char* detected_region = renfe_regular_get_region_from_data(data);
    const char* card_type = renfe_regular_detect_card_type(data);
    
    // Pre-load the region-specific station file for faster lookups
    renfe_regular_load_station_file(detected_region);
    
    // History blocks where transactions are stored
    // Different blocks for different card types
    int history_blocks[25];  // Reduced from 20 to 25 for safety but more efficient than before
    int num_blocks = 0;
    
    if(strcmp(card_type, "Bono Regular 10 trips") == 0) {
        // For Bono Regular 10 trips, check additional blocks where trip history might be stored
        int bono_blocks[] = {4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 20, 21, 40};
        num_blocks = sizeof(bono_blocks) / sizeof(bono_blocks[0]);
        for(int i = 0; i < num_blocks; i++) {
            history_blocks[i] = bono_blocks[i];
        }
    } else {
        // For regular RENFE cards, use original blocks
        int regular_blocks[] = {18, 20, 21, 40};
        num_blocks = sizeof(regular_blocks) / sizeof(regular_blocks[0]);
        for(int i = 0; i < num_blocks; i++) {
            history_blocks[i] = regular_blocks[i];
        }
    }
    
    // Array to store found history entries
    HistoryEntry* history_entries = malloc(sizeof(HistoryEntry) * num_blocks);
    if(!history_entries) {
        furi_string_cat_printf(parsed_data, "Memory allocation error\n");
        return;
    }
    
    int history_count = 0;
    int max_blocks = (data->type == MfClassicType1k) ? 64 : 256;
    
    // Collect all valid history entries
    for(int i = 0; i < num_blocks; i++) {
        int block = history_blocks[i];
        
        if(block >= max_blocks) {
            continue;
        }
        
        if(!mf_classic_is_block_read(data, block)) {
            continue;
        }
        
        const uint8_t* block_data = data->block[block].data;
        
        // For Bono Regular 10 trips, use different history detection logic
        bool is_history = false;
        if(strcmp(card_type, "Bono Regular 10 trips") == 0) {
            // Check for any non-zero, non-FF patterns that might indicate usage
            bool has_data = false;
            for(int j = 0; j < 16; j++) {
                if(block_data[j] != 0x00 && block_data[j] != 0xFF) {
                    has_data = true;
                    break;
                }
            }
            
            if(has_data) {
                // Check for specific patterns that might indicate trip usage
                if(block_data[0] >= 0x20 && block_data[0] <= 0x40) {
                    is_history = true;
                }
                else if(block_data[15] > 0x00 && block_data[15] < 0x80) {
                    is_history = true;
                }
                else if(block_data[2] > 0x00 && block_data[2] <= 0x10) {
                    is_history = true;
                }
            }
        } else {
            // Use regular history detection for other card types
            is_history = renfe_regular_is_travel_history_block(block_data, block);
        }
        
        if(is_history) {
            memcpy(history_entries[history_count].block_data, block_data, 16);
            history_entries[history_count].timestamp = renfe_regular_extract_timestamp(block_data);
            history_entries[history_count].block_number = block;
            history_count++;
        }
    }
    
    if(history_count == 0) {
        // If no history found but we know there should be trips (for Bono 10 trips)
        if(strcmp(card_type, "Bono Regular 10 trips") == 0) {
            furi_string_cat_printf(parsed_data, "Bono Trip History:\n");
            furi_string_cat_printf(parsed_data, "Card shows trips used but\n");
            furi_string_cat_printf(parsed_data, "detailed history format not\n");
            furi_string_cat_printf(parsed_data, "yet decoded.\n\n");
            
            // Show raw data from blocks that might contain trip info
            furi_string_cat_printf(parsed_data, "Raw trip data analysis:\n");
            for(int i = 0; i < num_blocks && i < 5; i++) {
                int block = history_blocks[i];
                if(block < max_blocks && mf_classic_is_block_read(data, block)) {
                    const uint8_t* block_data = data->block[block].data;
                    furi_string_cat_printf(parsed_data, "B%02d: ", block);
                    for(int j = 0; j < 16; j++) {
                        furi_string_cat_printf(parsed_data, "%02X", block_data[j]);
                        if(j == 7) furi_string_cat_printf(parsed_data, " ");
                    }
                    furi_string_cat_printf(parsed_data, "\n");
                }
            }
        }
        // Empty - let calling function handle message for regular cards
    } else {
        // Sort and display history
        renfe_regular_sort_history_entries(history_entries, history_count);
        
        furi_string_cat_printf(parsed_data, "Travel History (%d entries):\n", history_count);
        furi_string_cat_printf(parsed_data, "Region: %s\n", detected_region);
        furi_string_cat_printf(parsed_data, "Card Type: %s\n", card_type);
        furi_string_cat_printf(parsed_data, "(Most recent first)\n\n");
        
        for(int i = 0; i < history_count; i++) {
            furi_string_cat_printf(parsed_data, "Entry %d (Block %d):\n", 
                                 i + 1, history_entries[i].block_number);
            renfe_regular_parse_history_entry(parsed_data, history_entries[i].block_data, i + 1);
            furi_string_cat_printf(parsed_data, "\n");
        }
    }
    
    free(history_entries);
}

// Check if a specific block contains travel history
static bool renfe_regular_is_travel_history_block(const uint8_t* block_data, int block_number) {
    UNUSED(block_number);
    if(!block_data) return false;
    
    return renfe_regular_is_history_entry(block_data);
}

// Check if we have history data
static bool renfe_regular_has_history_data(const MfClassicData* data) {
    if(!data) {
        return false;
    }
    
    // Get card type to determine which blocks to check
    const char* card_type = renfe_regular_detect_card_type(data);
    
    int history_blocks[25];  // Reduced array size for memory efficiency
    int num_blocks = 0;
    
    if(strcmp(card_type, "Bono Regular 10 trips") == 0) {
        // For Bono Regular 10 trips, check additional blocks
        int bono_blocks[] = {4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 20, 21, 40};
        num_blocks = sizeof(bono_blocks) / sizeof(bono_blocks[0]);
        for(int i = 0; i < num_blocks; i++) {
            history_blocks[i] = bono_blocks[i];
        }
    } else {
        // For regular RENFE cards, use original blocks
        int regular_blocks[] = {18, 20, 21, 40};
        num_blocks = sizeof(regular_blocks) / sizeof(regular_blocks[0]);
        for(int i = 0; i < num_blocks; i++) {
            history_blocks[i] = regular_blocks[i];
        }
    }
    
    int max_blocks = (data->type == MfClassicType1k) ? 64 : 256;
    
    for(int i = 0; i < num_blocks; i++) {
        int block = history_blocks[i];
        
        if(block >= max_blocks) {
            continue;
        }
        
        if(!mf_classic_is_block_read(data, block)) {
            continue;
        }
        
        const uint8_t* block_data = data->block[block].data;
        if(!block_data) {
            continue;
        }
        
        // For Bono Regular 10 trips, use different detection logic
        if(strcmp(card_type, "Bono Regular 10 trips") == 0) {
            // Check for any meaningful data (not all zeros or all FFs)
            bool has_meaningful_data = false;
            for(int j = 0; j < 16; j++) {
                if(block_data[j] != 0x00 && block_data[j] != 0xFF) {
                    has_meaningful_data = true;
                    break;
                }
            }
            
            if(has_meaningful_data) {
                // Additional checks for trip-like patterns
                if((block_data[0] >= 0x20 && block_data[0] <= 0x40) ||
                   (block_data[15] > 0x00 && block_data[15] < 0x80) ||
                   (block_data[2] > 0x00 && block_data[2] <= 0x10)) {
                    return true;
                }
            }
        } else {
            // Use regular history detection for other card types
            if(renfe_regular_is_travel_history_block(block_data, block)) {
                return true;
            }
        }
    }
    
    return false;
}

// Parse only travel history (called when user presses LEFT button)
static bool renfe_regular_parse_history_only(FuriString* parsed_data, const MfClassicData* data) {
    if(!parsed_data || !data) {
        return false;
    }
    
    furi_string_reset(parsed_data);
    furi_string_cat_printf(parsed_data, "\e#RENFE REGULAR\n");
    furi_string_cat_printf(parsed_data, "\e#Travel History\n\n");
    
    bool has_history = renfe_regular_has_history_data(data);
    
    if(!has_history) {
        furi_string_cat_printf(parsed_data, "No travel history found\n\n");
        furi_string_cat_printf(parsed_data, "This card appears to be:\n");
        furi_string_cat_printf(parsed_data, "• New or unused\n");
        furi_string_cat_printf(parsed_data, "• Recently cleared\n");
        furi_string_cat_printf(parsed_data, "• Load dump from Saved\n\n");
    } else {
        renfe_regular_parse_travel_history(parsed_data, data);
    }
    
    furi_string_cat_printf(parsed_data, "\nPress LEFT to return");
    
    return true;
}

typedef struct {
    uint8_t data_sector;
    const MfClassicKeyPair* keys;
} RenfeRegularCardConfig;

static bool renfe_regular_get_card_config(RenfeRegularCardConfig* config, MfClassicType type) {
    bool success = true;

    if(type == MfClassicType1k) {
        config->data_sector = 5;
        config->keys = renfe_regular_keys;
    } else if(type == MfClassicType4k) {
        config->data_sector = 5;
        config->keys = renfe_regular_keys;
    } else {
        success = false; 
    }

    return success;
}

// Parse function for RENFE Regular cards
static bool renfe_regular_parse(FuriString* parsed_data, const MfClassicData* data) {
    if(!parsed_data) {
        return false;
    }
    
    if(!data) {
        return false;
    }
    
    bool parsed = false;
    RenfeRegularCardConfig cfg;
    
    do {
        memset(&cfg, 0, sizeof(cfg));
        if(!renfe_regular_get_card_config(&cfg, data->type)) {
            break;
        }

        furi_string_cat_printf(parsed_data, "\e#RENFE REGULAR\n");
        
        // 1. Show card type
        const char* card_type = renfe_regular_detect_card_type(data);
        furi_string_cat_printf(parsed_data, "Type: %s\n", card_type);
        
        // 1.1. Show trip counter or usage status
        if(strcmp(card_type, "Bono Regular 10 trips") == 0) {
            // Multi-trip BONO card
            int trip_count = renfe_regular_get_bono_trip_count(data);
            if(trip_count >= 0) {
                furi_string_cat_printf(parsed_data, "Trips: %d/10\n", trip_count);
            } else {
                furi_string_cat_printf(parsed_data, "Trips: Unknown/10\n");
            }
        } else {
            // Regular ida/vuelta card
            int trip_count = renfe_regular_get_bono_trip_count(data);
            if(trip_count == -2) {
                furi_string_cat_printf(parsed_data, "Status: USED\n");
            } else if(trip_count == -1) {
                furi_string_cat_printf(parsed_data, "Status: UNUSED\n");
            } else if(trip_count == -3) {
                furi_string_cat_printf(parsed_data, "Status: UNKNOWN\n");
            } else if(trip_count >= 0) {
                furi_string_cat_printf(parsed_data, "Trips: %d\n", trip_count);
            } else {
                furi_string_cat_printf(parsed_data, "Status: Cannot read\n");
            }
        }
        
        // 2. Show card format
        if(data->type == MfClassicType1k) {
            furi_string_cat_printf(parsed_data, "Format: Mifare Classic 1K\n");
        } else if(data->type == MfClassicType4k) {
            furi_string_cat_printf(parsed_data, "Format: Mifare Classic 4K\n");
        } else {
            furi_string_cat_printf(parsed_data, "Format: Unknown\n");
        }
        
        // 3. Extract and show UID
        // Attempting to extract UID
        
        if(data->iso14443_3a_data && data->iso14443_3a_data->uid_len > 0) {
            const uint8_t* uid = data->iso14443_3a_data->uid;
            size_t uid_len = data->iso14443_3a_data->uid_len;
            
            // UID data found
            
            furi_string_cat_printf(parsed_data, "UID: ");
            for(size_t i = 0; i < uid_len; i++) {
                furi_string_cat_printf(parsed_data, "%02X", uid[i]);
                if(i < uid_len - 1) furi_string_cat_printf(parsed_data, " ");
            }
            furi_string_cat_printf(parsed_data, "\n");
        } else {
            // Extract UID from block 0

            if(mf_classic_is_block_read(data, 0)) {
                const uint8_t* block0 = data->block[0].data;
                furi_string_cat_printf(parsed_data, "UID: %02X %02X %02X %02X\n", block0[0], block0[1], block0[2], block0[3]);
            } else {
                furi_string_cat_printf(parsed_data, "UID: N/A\n");
            }
        }
        
        // 4. Show region
        const char* region = renfe_regular_get_region_from_data(data);
        // Capitalize first letter for display
        char display_region[32];
        strncpy(display_region, region, sizeof(display_region) - 1);
        display_region[sizeof(display_region) - 1] = '\0';
        if(strlen(display_region) > 0) {
            display_region[0] = toupper(display_region[0]);
            // Replace underscores with spaces and capitalize after spaces
            for(size_t i = 1; i < strlen(display_region); i++) {
                if(display_region[i] == '_') {
                    display_region[i] = ' ';
                    if(i + 1 < strlen(display_region)) {
                        display_region[i + 1] = toupper(display_region[i + 1]);
                    }
                }
            }
        }
        furi_string_cat_printf(parsed_data, "Region: %s\n", display_region);
        
        // Add travel history status (only for non-ida/vuelta cards)
        bool is_ida_vuelta = renfe_regular_is_ida_vuelta_card(data);
        if(!is_ida_vuelta) {
            furi_string_cat_printf(parsed_data, "\n");
            if(renfe_regular_has_history_data(data)) {
                furi_string_cat_printf(parsed_data, "History: Available\n");
                furi_string_cat_printf(parsed_data, "   Press LEFT to view details\n");
            } else {
                furi_string_cat_printf(parsed_data, "History: Empty/Not found\n");
                furi_string_cat_printf(parsed_data, "   Load dump from Saved\n");
            }
        }
        
        parsed = true;
        
    } while(false);

    return parsed;
}

// Widget callback function for handling button events
static void renfe_regular_widget_callback(GuiButtonType result, InputType type, void* context) {
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

// This plugin only works with saved dumps, not live card reading
static void renfe_regular_on_enter(Metroflip* app) {
    if(!app) {
        return;
    }
    
    // RENFE Regular plugin entry
    
    dolphin_deed(DolphinDeedNfcRead);

    if(app->data_loaded) {
        MfClassicData* mfc_data = NULL;
        bool should_free_mfc_data = false;

        // Load from file
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
            // RENFE Regular: MFC data available
            FuriString* parsed_data = furi_string_alloc();
            Widget* widget = app->widget;

            if(!widget) {
                // RENFE Regular: Widget is NULL
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
            if(!renfe_regular_parse(parsed_data, mfc_data)) {
                furi_string_reset(app->text_box_store);
                furi_string_printf(parsed_data, "\e#Unknown card\n");
            }
            widget_add_text_scroll_element(widget, 0, 0, 128, 52, furi_string_get_cstr(parsed_data));

            // Check if it's ida/vuelta card to decide whether to show History button
            bool is_ida_vuelta = renfe_regular_is_ida_vuelta_card(mfc_data);
            
            if(!is_ida_vuelta) {
                // Only show History button for non-ida/vuelta cards
                widget_add_button_element(widget, GuiButtonTypeLeft, "History", renfe_regular_widget_callback, app);
            }
            
            widget_add_button_element(widget, GuiButtonTypeCenter, "Delete", metroflip_delete_widget_callback, app);
            widget_add_button_element(widget, GuiButtonTypeRight, "Exit", metroflip_exit_widget_callback, app);
            
            if(!app->view_dispatcher) {
                if(should_free_mfc_data) mf_classic_free(mfc_data);
                furi_string_free(parsed_data);
                return;
            }
            
            if(should_free_mfc_data) mf_classic_free(mfc_data);
            furi_string_free(parsed_data);
            
            // RENFE Regular: Switching to widget view
            view_dispatcher_switch_to_view(app->view_dispatcher, MetroflipViewWidget);
        } else {
            // RENFE Regular: Failed to load MFC data
        }
    } else {
        // Card reading not supported - show message to load dump
        Widget* widget = app->widget;
        if(!widget) {
            return;
        }
        
        if(!app->view_dispatcher) {
            return;
        }
        
        FuriString* message = furi_string_alloc();
        furi_string_printf(message, "\e# RENFE REGULAR\n\n");
        furi_string_cat_printf(message, " Live reading not supported\n\n");
        furi_string_cat_printf(message, "These cards use specific keys\n");
        furi_string_cat_printf(message, "that vary by card.\n\n");
        furi_string_cat_printf(message, " Load your dump from:\n");
        furi_string_cat_printf(message, "   Saved -> [filename].nfc\n\n");
        furi_string_cat_printf(message, " Supports all 18 regions:\n");
        furi_string_cat_printf(message, "   Valencia, Madrid, Catalunya,\n");
        furi_string_cat_printf(message, "   Andalucia, Pais Vasco,\n");
        furi_string_cat_printf(message, "   Galicia, and 12 more...\n\n");
        furi_string_cat_printf(message, " Use Proxmark3 or similar\n");
        furi_string_cat_printf(message, "   to dump the card first.");
        
        widget_add_text_scroll_element(widget, 0, 0, 128, 52, furi_string_get_cstr(message));
        widget_add_button_element(widget, GuiButtonTypeRight, "Exit", metroflip_exit_widget_callback, app);
        
        furi_string_free(message);
        view_dispatcher_switch_to_view(app->view_dispatcher, MetroflipViewWidget);
    }
}

static bool renfe_regular_on_event(Metroflip* app, SceneManagerEvent event) {
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == GuiButtonTypeLeft) {
            // Handle LEFT button press to show travel history
            if(app->data_loaded) {
                MfClassicData* mfc_data = NULL;
                bool should_free_mfc_data = false;
        
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
                        if(!renfe_regular_parse_history_only(parsed_data, mfc_data)) {
                            furi_string_reset(parsed_data);
                            furi_string_printf(parsed_data, "\e#Travel History\nNo history found\n");
                        }
                        widget_add_text_scroll_element(widget, 0, 0, 128, 52, furi_string_get_cstr(parsed_data));
                        widget_add_button_element(widget, GuiButtonTypeRight, "Back", renfe_regular_widget_callback, app);
                        view_dispatcher_switch_to_view(app->view_dispatcher, MetroflipViewWidget);
                    }

                    if(should_free_mfc_data) mf_classic_free(mfc_data);
                    furi_string_free(parsed_data);
                }
            }
            consumed = true;
        } else if(event.event == GuiButtonTypeRight) {
            if(app->data_loaded) {
                MfClassicData* mfc_data = NULL;
                bool should_free_mfc_data = false;
                

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
                        if(!renfe_regular_parse(parsed_data, mfc_data)) {
                            furi_string_printf(parsed_data, "\e#Unknown card\n");
                        }
                        widget_add_text_scroll_element(widget, 0, 0, 128, 52, furi_string_get_cstr(parsed_data));

                        // Check if it's ida/vuelta card to decide whether to show History button
                        bool is_ida_vuelta = renfe_regular_is_ida_vuelta_card(mfc_data);
                        
                        if(!is_ida_vuelta) {
                            // Only show History button for non-ida/vuelta cards
                            widget_add_button_element(widget, GuiButtonTypeLeft, "History", renfe_regular_widget_callback, app);
                        }
                        
                        widget_add_button_element(widget, GuiButtonTypeCenter, "Delete", metroflip_delete_widget_callback, app);
                        widget_add_button_element(widget, GuiButtonTypeRight, "Exit", metroflip_exit_widget_callback, app);
                        
                        view_dispatcher_switch_to_view(app->view_dispatcher, MetroflipViewWidget);
                    }
                    
                    if(should_free_mfc_data) mf_classic_free(mfc_data);
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

static void renfe_regular_on_exit(Metroflip* app) {
    if(!app) {
        // renfe_regular_on_exit: app is NULL
        return;
    }

    if(app->widget) {
        widget_reset(app->widget);
    }

    // Clear station cache to free memory
    renfe_regular_clear_station_cache();
}

static const MetroflipPlugin renfe_regular_plugin = {
    .card_name = "RENFE Regular",
    .plugin_on_enter = renfe_regular_on_enter,
    .plugin_on_event = renfe_regular_on_event,
    .plugin_on_exit = renfe_regular_on_exit,
};

static const FlipperAppPluginDescriptor renfe_regular_plugin_descriptor = {
    .appid = METROFLIP_SUPPORTED_CARD_PLUGIN_APP_ID,
    .ep_api_version = METROFLIP_SUPPORTED_CARD_PLUGIN_API_VERSION,
    .entry_point = &renfe_regular_plugin,
};

const FlipperAppPluginDescriptor* renfe_regular_plugin_ep(void) {
    return &renfe_regular_plugin_descriptor;
}
