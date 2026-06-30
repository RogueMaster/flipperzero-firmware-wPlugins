
#include <flipper_application.h>
#include "../../metroflip_i.h"

#include <nfc/protocols/st25tb/st25tb_poller.h>
#include <nfc/protocols/st25tb/st25tb.h>

#include <dolphin/dolphin.h>
#include <bit_lib.h>
#include <furi_hal.h>
#include <furi.h>
#include <nfc/nfc.h>
#include <nfc/nfc_device.h>
#include <nfc/nfc_listener.h>
#include "../../api/metroflip/metroflip_api.h"
#include "../../metroflip_plugins.h"
#include <machine/endian.h>
#include <stdio.h>
#include <applications/services/locale/locale.h>
#include <datetime.h>
#include <stdint.h>
#define ST25TB_UID_SIZE (8U)
#define ST25TB_TOTAL_BYTES 64
#define DISTRIBUTION_DATA_START 0
#define DISTRIBUTION_DATA_END 20



#define TAG "Metroflip:Scene:Star"


typedef struct {
    int outer_key; // e.g., 0x000
    int inner_key; // e.g., 5
    const char* city;
    const char* system;
    const char* usage; // e.g., "_1_1", "_2", NULL if not specified
} InterticEntry;

InterticEntry FRA_OrganizationalAuthority_Contract_Provider[] = {
    {0x000, 5, "Lille", "Ilévia / Keolis", "_1_1"},
    {0x000, 7, "Lens-Béthune", "Tadao / Transdev", "_1_1"},
    {0x006, 1, "Amiens", "Ametis / Keolis", NULL},
    {0x008, 15, "Angoulême", "STGA", "_1_1"},
    {0x021, 1, "Bordeaux", "TBM / Keolis", "_1_1"},
    {0x057, 1, "Lyon", "TCL / Keolis", "_1"},
    {0x072, 1, "Tours", "filbleu / Keolis", "_1_1"},
    {0x078, 4, "Reims", "Citura / Transdev", "_1_2"},
    {0x091, 1, "Strasbourg", "CTS", "_4"},
    {0x502, 83, "Annecy", "Sibra", "_2"},
    {0x502, 10, "Clermont-Ferrand", "T2C", NULL},
    {0x907, 1, "Dijon", "Divia / Keolis", NULL},
    {0x908, 1, "Rennes", "STAR / Keolis", "_2"},
    {0x908, 8, "Saint-Malo", "MAT / RATP", "_1_1"},
    {0x911, 5, "Besançon", "Ginko / Keolis", NULL},
    {0x912, 3, "Le Havre", "Lia / Transdev", "_1_1"},
    {0x912, 35, "Cherbourg-en-Cotentin", "Cap Cotentin / Transdev", NULL},
    {0x913, 3, "Nîmes", "Tango / Transdev", "_3"},
    {0x915, 1, "Metz", "Le Met' / TAMM", NULL},
    {0x917, 4, "Angers", "Irigo / RATP", "_1_2"},
    {0x917, 7, "Saint-Nazaire", "Stran", NULL},
};

#define NUM_ENTRIES (sizeof(FRA_OrganizationalAuthority_Contract_Provider)/sizeof(FRA_OrganizationalAuthority_Contract_Provider[0]))

InterticEntry* find_entry(int outer, int inner) {
    for (size_t i = 0; i < NUM_ENTRIES; i++) {
        if (FRA_OrganizationalAuthority_Contract_Provider[i].outer_key == outer &&
            FRA_OrganizationalAuthority_Contract_Provider[i].inner_key == inner) {
            return &FRA_OrganizationalAuthority_Contract_Provider[i];
        }
    }
    return NULL; // not found
}


uint8_t st25tb_get_block_byte(const St25tbData* data, size_t block_index, uint8_t byte_index) {
    if(block_index >= st25tb_get_block_count(data->type) || byte_index > 3) return 0;
    uint32_t block = __bswap32(data->blocks[block_index]);
    return (block >> (8 * (3 - byte_index))) & 0xFF;
}

void big_endian_version(uint8_t* dst, const St25tbData* data, size_t total_bytes) { // god I hate endianess
    for(size_t i = 0; i < total_bytes; i += 4) {
        uint8_t b0 = st25tb_get_block_byte(data, i/4, 0);
        uint8_t b1 = st25tb_get_block_byte(data, i/4, 1);
        uint8_t b2 = st25tb_get_block_byte(data, i/4, 2);
        uint8_t b3 = st25tb_get_block_byte(data, i/4, 3);
        dst[i]     = b3;
        dst[i + 1] = b2;
        dst[i + 2] = b1;
        dst[i + 3] = b0;
    }

}

// Converts selected bytes to a bit string
static void bytes_to_bit_string(const uint8_t* buffer, size_t start_byte, size_t end_byte, char* bit_str) {
    char* dst = bit_str;
    for(size_t i = start_byte; i <= end_byte; i++) {
        for(int bit = 7; bit >= 0; bit--) {
            *dst++ = (buffer[i] & (1 << bit)) ? '1': '0';
        }
    }
    *dst = '\0';
}

// Extract arbitrary bits from buffer using a string of bits
uint64_t extract_bits(const uint8_t* buffer, size_t start_byte, size_t end_byte, size_t start_bit, size_t num_bits) {
    size_t bytes_count = end_byte - start_byte + 1;
    char bit_string[bytes_count * 8 + 1];
    bytes_to_bit_string(buffer, start_byte, end_byte, bit_string);

    uint64_t val = 0;
    for(size_t i = 0; i < num_bits; i++) {
        val <<= 1;
        if(bit_string[start_bit + i] == '1') val |= 1;
    }
    return val;
}

void append_bytes(uint8_t* dest, size_t* dest_size, size_t dest_max,
                  const uint8_t* src, size_t src_size,
                  size_t start, size_t length) {
    if(start >= src_size) return; // invalid start
    if(start + length > src_size) length = src_size - start; // clamp length
    if(*dest_size + length > dest_max) length = dest_max - *dest_size; // clamp to dest space
    if(length == 0) return;

    memcpy(dest + *dest_size, src + start, length);
    *dest_size += length;
}

bool parse_timestamp(uint64_t days_since_1997, FuriString* parsed_data) {
    if(!parsed_data) return false;

    uint64_t unixTimestamp = days_since_1997 * 24 * 60 * 60 + 852076800ULL;

    DateTime dt = {0};
    datetime_timestamp_to_datetime(unixTimestamp, &dt);

    FuriString* timestamp_str = furi_string_alloc();
    locale_format_date(timestamp_str, &dt, locale_get_date_format(), "-");

    furi_string_cat_printf(parsed_data, "%s", furi_string_get_cstr(timestamp_str));
    furi_string_free(timestamp_str);

    return true;
}

void Describe_Usage_1(uint8_t* UsageAB, uint64_t ContractMediumEndDate, FuriString* parsed_data) {
    uint64_t EventDateStamp = extract_bits(UsageAB, 0, 20, 0, 10);
    uint64_t EventTimeStamp = extract_bits(UsageAB, 0, 20, 10, 11);
    uint64_t EventValidityTimeFirstStamp = extract_bits(UsageAB, 0, 20, 86, 11);

    furi_string_cat_printf(parsed_data, "\n  EventDateStamp: ");
    parse_timestamp(ContractMediumEndDate - EventDateStamp, parsed_data);
    furi_string_cat_printf(parsed_data, "\n  EventTimeStamp: %02llu:%02llu",
                           EventTimeStamp / 60, EventTimeStamp % 60);
    furi_string_cat_printf(parsed_data, "\n  EventValidityTimeFirstStamp:\n  %02llu:%02llu\n",
                           EventValidityTimeFirstStamp / 60, EventValidityTimeFirstStamp % 60);
}

void Describe_Usage_1_1(uint8_t* UsageAB, uint64_t ContractMediumEndDate, FuriString* parsed_data) {
    uint64_t EventDateStamp = extract_bits(UsageAB, 0, 20, 0, 10);
    uint64_t EventTimeStamp = extract_bits(UsageAB, 0, 20, 10, 11);
    uint64_t EventCode_Nature = extract_bits(UsageAB, 0, 20, 29, 5);
    uint64_t EventCode_Type = extract_bits(UsageAB, 0, 20, 34, 5);
    uint64_t EventGeoVehicleId = extract_bits(UsageAB, 0, 20, 50, 16);
    uint64_t EventGeoRouteId = extract_bits(UsageAB, 0, 20, 66, 14);
    uint64_t EventGeoRoute_Direction = extract_bits(UsageAB, 0, 20, 80, 2);
    uint64_t EventCountPassengers_mb = extract_bits(UsageAB, 0, 20, 82, 4);
    uint64_t EventValidityTimeFirstStamp = extract_bits(UsageAB, 0, 20, 86, 11);

    furi_string_cat_printf(parsed_data, "\n  DateStamp: ");
    parse_timestamp(ContractMediumEndDate - EventDateStamp, parsed_data);
    furi_string_cat_printf(parsed_data, "\n  TimeStamp: %02llu:%02llu\n",
                           EventTimeStamp / 60, EventTimeStamp % 60);
    furi_string_cat_printf(parsed_data, "\n  Code/Nature: 0x%llx", EventCode_Nature);
    furi_string_cat_printf(parsed_data, "\n  Code/Type: 0x%llx", EventCode_Type);
    furi_string_cat_printf(parsed_data, "\n  GeoVehicleId: %llu", EventGeoVehicleId);
    furi_string_cat_printf(parsed_data, "\n  GeoRouteId: %llu", EventGeoRouteId);
    furi_string_cat_printf(parsed_data, "\n  Direction: %llu", EventGeoRoute_Direction);
    furi_string_cat_printf(parsed_data, "\n  Passengers: %llu", EventCountPassengers_mb);
    furi_string_cat_printf(parsed_data, "\n  ValidityTimeFirstStamp:\n      %02llu:%02llu\n",
                           EventValidityTimeFirstStamp / 60, EventValidityTimeFirstStamp % 60);
}

void Describe_Usage_2(uint8_t* UsageAB, uint64_t ContractMediumEndDate, FuriString* parsed_data) {
    uint64_t EventDateStamp = extract_bits(UsageAB, 0, 20, 0, 10);
    uint64_t EventTimeStamp = extract_bits(UsageAB, 0, 20, 10, 11);
    uint64_t EventCode_Nature = extract_bits(UsageAB, 0, 20, 29, 5);
    uint64_t EventCode_Type = extract_bits(UsageAB, 0, 20, 34, 5);
    uint64_t EventGeoRouteId = extract_bits(UsageAB, 0, 20, 50, 14);
    uint64_t EventGeoRoute_Direction = extract_bits(UsageAB, 0, 20, 64, 2);
    uint64_t EventCountPassengers_mb = extract_bits(UsageAB, 0, 20, 66, 4);
    uint64_t EventValidityTimeFirstStamp = extract_bits(UsageAB, 0, 20, 70, 11);

    furi_string_cat_printf(parsed_data, "\n  DateStamp: ");
    parse_timestamp(ContractMediumEndDate - EventDateStamp, parsed_data);
    furi_string_cat_printf(parsed_data, "\n  TimeStamp: %02llu:%02llu",
                           EventTimeStamp / 60, EventTimeStamp % 60);
    furi_string_cat_printf(parsed_data, "\n  Code/Nature: 0x%llx", EventCode_Nature);
    furi_string_cat_printf(parsed_data, "\n  Code/Type: 0x%llx", EventCode_Type);
    furi_string_cat_printf(parsed_data, "\n  GeoRouteId: %llu", EventGeoRouteId);
    furi_string_cat_printf(parsed_data, "\n  Direction: %llu", EventGeoRoute_Direction);
    furi_string_cat_printf(parsed_data, "\n  Passengers: %llu", EventCountPassengers_mb);
    furi_string_cat_printf(parsed_data, "\n  ValidityTimeFirstStamp:\n     %02llu:%02llu\n",
                           EventValidityTimeFirstStamp / 60, EventValidityTimeFirstStamp % 60);
}

void Describe_Usage_1_2(uint8_t* UsageAB, uint64_t ContractMediumEndDate, FuriString* parsed_data) {
    uint64_t EventDateStamp = extract_bits(UsageAB, 0, 20, 0, 10);
    uint64_t EventTimeStamp = extract_bits(UsageAB, 0, 20, 10, 11);
    uint64_t EventCount_mb = extract_bits(UsageAB, 0, 20, 21, 6);
    uint64_t EventCode_Nature_mb = extract_bits(UsageAB, 0, 20, 30, 4);
    uint64_t EventCode_Type_mb = extract_bits(UsageAB, 0, 20, 34, 4);
    uint64_t EventGeoVehicleId = extract_bits(UsageAB, 0, 20, 49, 16);
    uint64_t EventGeoRouteId = extract_bits(UsageAB, 0, 20, 65, 14);
    uint64_t EventGeoRoute_Direction = extract_bits(UsageAB, 0, 20, 79, 2);
    uint64_t EventCountPassengers_mb = extract_bits(UsageAB, 0, 20, 81, 4);
    uint64_t EventValidityTimeFirstStamp = extract_bits(UsageAB, 0, 20, 85, 11);

    furi_string_cat_printf(parsed_data, "\n  DateStamp: ");
    parse_timestamp(ContractMediumEndDate - EventDateStamp, parsed_data);
    furi_string_cat_printf(parsed_data, "\n  TimeStamp: %02llu:%02llu", EventTimeStamp / 60, EventTimeStamp % 60);
    furi_string_cat_printf(parsed_data, "\n  Count: %llu", EventCount_mb);

    // Proper switch for EventCode_Nature_mb
    switch(EventCode_Nature_mb) {
        case 0x4: furi_string_cat_printf(parsed_data, "\n  Code/Nature:\n0x%llx (urban bus)", EventCode_Nature_mb); break;
        case 0x1: furi_string_cat_printf(parsed_data, "\n  Code/Nature:\n0x%llx (tramway)", EventCode_Nature_mb); break;
        default:  furi_string_cat_printf(parsed_data, "\n  Code/Nature:\n0x%llx (?)", EventCode_Nature_mb); break;
    }

    furi_string_cat_printf(parsed_data, "\n  Code/Type: 0x%llu", EventCode_Type_mb);
    furi_string_cat_printf(parsed_data, "\n  GeoVehicleId: %llu", EventGeoVehicleId);
    furi_string_cat_printf(parsed_data, "\n  GeoRouteId: %llu", EventGeoRouteId);
    furi_string_cat_printf(parsed_data, "\n  Direction: %llu", EventGeoRoute_Direction);
    furi_string_cat_printf(parsed_data, "\n  Passengers: %llu", EventCountPassengers_mb);
    furi_string_cat_printf(parsed_data, "\n  ValidityTimeFirstStamp: %02llu:%02llu\n",
                           EventValidityTimeFirstStamp / 60, EventValidityTimeFirstStamp % 60);
}

void Describe_Usage_3(uint8_t* UsageAB, uint64_t ContractMediumEndDate, FuriString* parsed_data) {
    uint64_t EventDateStamp = extract_bits(UsageAB, 0, 20, 0, 10);
    uint64_t EventTimeStamp = extract_bits(UsageAB, 0, 20, 10, 11);
    uint64_t EventValidityTimeFirstStamp = extract_bits(UsageAB, 0, 20, 48, 11);

    furi_string_cat_printf(parsed_data, "\n  EventDateStamp: ");
    parse_timestamp(ContractMediumEndDate - EventDateStamp, parsed_data);
    furi_string_cat_printf(parsed_data, "\n  EventTimeStamp: %02llu:%02llu",
                           EventTimeStamp / 60, EventTimeStamp % 60);
    furi_string_cat_printf(parsed_data, "\n  EventValidityTimeFirstStamp:\n    %02llu:%02llu\n",
                           EventValidityTimeFirstStamp / 60, EventValidityTimeFirstStamp % 60);
}

void Describe_Usage_4(uint8_t* UsageAB, uint64_t ContractMediumEndDate, FuriString* parsed_data) {
    uint64_t EventDateStamp = extract_bits(UsageAB, 0, 20, 0, 10);
    uint64_t EventTimeStamp = extract_bits(UsageAB, 0, 20, 10, 11);
    uint64_t EventValidityTimeFirstStamp = extract_bits(UsageAB, 0, 20, 84, 11);

    furi_string_cat_printf(parsed_data, "\n  EventDateStamp: ");
    parse_timestamp(ContractMediumEndDate - EventDateStamp, parsed_data);
    furi_string_cat_printf(parsed_data, "\n  EventTimeStamp: %02llu:%02llu",
                           EventTimeStamp / 60, EventTimeStamp % 60);
    furi_string_cat_printf(parsed_data, "\n  EventValidityTimeFirstStamp:\n   %02llu:%02llu\n",
                           EventValidityTimeFirstStamp / 60, EventValidityTimeFirstStamp % 60);
}



static bool intertic_parse(FuriString* parsed_data, const St25tbData* data) {
    bool parsed = false;

    do {
        furi_string_cat_printf(
            parsed_data,
            "\e#Star\n");
        furi_string_cat_printf(parsed_data, "UID:");

        for(size_t i = 0; i < ST25TB_UID_SIZE; i++) {
            furi_string_cat_printf(parsed_data, " %02X", data->uid[i]);
        }
        //do some check here
        //let's get distribution data:
        uint8_t big_endian_file_buffer[64];
        big_endian_version(big_endian_file_buffer, data, ST25TB_TOTAL_BYTES);
        uint64_t PID = extract_bits(big_endian_file_buffer, DISTRIBUTION_DATA_START, DISTRIBUTION_DATA_END, 27, 5);

        uint8_t distributionData[20];
        size_t distributionData_size = 0;
        uint8_t usageA[20];
        size_t usageA_size = 0;
        uint8_t reloading1[1];
        size_t reloading1_size = 0;
        uint8_t counter1[3];
        size_t counter1_size = 0;
        uint8_t SWAP[4];
        size_t SWAP_size = 0;
        uint8_t usageB[20];
        size_t usageB_size = 0;

        switch(PID) {
            case 0x10:
                FURI_LOG_I(TAG, "PID 0x10");
                append_bytes(distributionData, &distributionData_size, sizeof(distributionData), big_endian_file_buffer, sizeof(big_endian_file_buffer), 4, 8);
                append_bytes(distributionData, &distributionData_size, sizeof(distributionData), big_endian_file_buffer, sizeof(big_endian_file_buffer), 0, 3);
                append_bytes(usageA, &usageA_size, sizeof(usageA), big_endian_file_buffer, sizeof(big_endian_file_buffer), 12, 8);
                append_bytes(reloading1, &reloading1_size, sizeof(reloading1), big_endian_file_buffer, sizeof(big_endian_file_buffer), 20, 1);
                append_bytes(counter1, &counter1_size, sizeof(counter1), big_endian_file_buffer, sizeof(big_endian_file_buffer), 21, 3);
                append_bytes(SWAP, &SWAP_size, sizeof(SWAP), big_endian_file_buffer, sizeof(big_endian_file_buffer), 24, 4);
                append_bytes(usageA, &usageA_size, sizeof(usageA), big_endian_file_buffer, sizeof(big_endian_file_buffer), 28, 12);
                append_bytes(usageB, &usageB_size, sizeof(usageB), big_endian_file_buffer, sizeof(big_endian_file_buffer), 40, 20);
                break;
            case 0x11 | 0x19:
                FURI_LOG_I(TAG, "PID 0x11|0x19");
                append_bytes(distributionData, &distributionData_size, sizeof(distributionData), big_endian_file_buffer, sizeof(big_endian_file_buffer), 4, 16);
                append_bytes(distributionData, &distributionData_size, sizeof(distributionData), big_endian_file_buffer, sizeof(big_endian_file_buffer), 0, 3);
                append_bytes(reloading1, &reloading1_size, sizeof(reloading1), big_endian_file_buffer, sizeof(big_endian_file_buffer), 20, 1);
                append_bytes(counter1, &counter1_size, sizeof(counter1), big_endian_file_buffer, sizeof(big_endian_file_buffer), 21, 3);
                append_bytes(SWAP, &SWAP_size, sizeof(SWAP), big_endian_file_buffer, sizeof(big_endian_file_buffer), 24, 4);
                append_bytes(usageA, &usageA_size, sizeof(usageA), big_endian_file_buffer, sizeof(big_endian_file_buffer), 28, 16);
                append_bytes(usageB, &usageB_size, sizeof(usageB), big_endian_file_buffer, sizeof(big_endian_file_buffer), 44, 16);
                break;
            default:
                parsed = true;
                FURI_LOG_I(TAG, "Unkown PID");
                furi_string_cat_printf(parsed_data, "\nUNKNOWN PID:\n0x%02llX\n", PID);
                continue;

        }
        uint64_t countryISOCode = extract_bits(distributionData, DISTRIBUTION_DATA_START, sizeof(distributionData), 0, 12);
        if (countryISOCode != 0x250) break; // FRANCE
        uint64_t organizationalAuthority = extract_bits(distributionData, DISTRIBUTION_DATA_START, sizeof(distributionData), 12, 12);
        uint64_t contractApplicationVersionNumber = extract_bits(distributionData, DISTRIBUTION_DATA_START, sizeof(distributionData), 24, 6);
        uint64_t contractProvider = extract_bits(distributionData, DISTRIBUTION_DATA_START, sizeof(distributionData), 30, 8);
        uint64_t ContractMediumEndDate = extract_bits(distributionData, DISTRIBUTION_DATA_START, sizeof(distributionData), 54, 14);

        furi_string_cat_printf(
            parsed_data,
            "\nCountry Code:\n0x%02llX (France\n",
            countryISOCode);
        furi_string_cat_printf(
            parsed_data,
            "\nOrganizational Authority:\n0x%02llX\n",
            organizationalAuthority);
        
        furi_string_cat_printf(
            parsed_data,
            "\nContract Application\nVersion Number: %lld\n",
            contractApplicationVersionNumber);

        
        furi_string_cat_printf(
            parsed_data,
            "\nContract Provider: %lld\n",
            contractProvider);
        
        furi_string_cat_printf(
            parsed_data,
            "\nContract End Date:\n");

        parse_timestamp(ContractMediumEndDate, parsed_data);

        InterticEntry* entry = find_entry(organizationalAuthority, contractProvider);
        if (entry) {
            furi_string_cat_printf(parsed_data, "\n\nCity: \n%s\n", entry->city); 
            furi_string_cat_printf(parsed_data, "\nSystem: \n%s\n", entry->system);
        } else {
            furi_string_cat_printf(
            parsed_data,"\n\nCity code not indexed\n");
        }
        uint32_t swap_value = (SWAP[0] << 24) | (SWAP[1] << 16) | (SWAP[2] << 8) | SWAP[3];
        uint32_t counter_value = (counter1[0] << 16) | (counter1[1] << 8) | counter1[2];

        furi_string_cat_printf(parsed_data, "\nLast usage: USAGE_%c\n", (swap_value & 0b1) ? 'B': 'A');

        
        furi_string_cat_printf(parsed_data, "\nRemaining Trips:\n%lu\n", (unsigned long)counter_value);

 

        furi_string_cat_printf(parsed_data, "\nUSAGE A:\n");
        if(strcmp(entry->usage, "_1") == 0) {
            Describe_Usage_1(usageA, ContractMediumEndDate, parsed_data);
        } else if(strcmp(entry->usage, "_1_1") == 0) {
            Describe_Usage_1_1(usageA, ContractMediumEndDate, parsed_data);
        } else if(strcmp(entry->usage, "_1_2") == 0) {
            Describe_Usage_1_2(usageA, ContractMediumEndDate, parsed_data);
        } else if(strcmp(entry->usage, "_2") == 0) {
            Describe_Usage_2(usageA, ContractMediumEndDate, parsed_data);
        } else if(strcmp(entry->usage, "_3") == 0) {
            Describe_Usage_3(usageA, ContractMediumEndDate, parsed_data);
        } else if(strcmp(entry->usage, "_4") == 0) {
            Describe_Usage_4(usageA, ContractMediumEndDate, parsed_data);
        } else {
            Describe_Usage_1(usageA, ContractMediumEndDate, parsed_data);
        }

        furi_string_cat_printf(parsed_data, "\nUSAGE B:\n");
        if(strcmp(entry->usage, "_1") == 0) {
            Describe_Usage_1(usageB, ContractMediumEndDate, parsed_data);
        } else if(strcmp(entry->usage, "_1_1") == 0) {
            Describe_Usage_1_1(usageB, ContractMediumEndDate, parsed_data);
        } else if(strcmp(entry->usage, "_1_2") == 0) {
            Describe_Usage_1_2(usageB, ContractMediumEndDate, parsed_data);
        } else if(strcmp(entry->usage, "_2") == 0) {
            Describe_Usage_2(usageB, ContractMediumEndDate, parsed_data);
        } else if(strcmp(entry->usage, "_3") == 0) {
            Describe_Usage_3(usageB, ContractMediumEndDate, parsed_data);
        } else if(strcmp(entry->usage, "_4") == 0) {
            Describe_Usage_4(usageB, ContractMediumEndDate, parsed_data);
        } else {
            Describe_Usage_1(usageB, ContractMediumEndDate, parsed_data);
        }
        

        parsed = true;
    } while(false);

    return parsed;
}


static NfcCommand intertic_poller_callback(NfcGenericEvent event, void* context) {
    furi_assert(context);
    furi_assert(event.event_data);
    furi_assert(event.protocol == NfcProtocolSt25tb);

    Metroflip* app = context;
    const St25tbPollerEvent* st25tb_event = event.event_data;

    if(st25tb_event->type == St25tbPollerEventTypeRequestMode) {
        st25tb_event->data->mode_request.mode = St25tbPollerModeRead;
    } else if(st25tb_event->type == St25tbPollerEventTypeSuccess) {
        nfc_device_set_data(
            app->nfc_device, NfcProtocolSt25tb, nfc_poller_get_data(app->poller));
        view_dispatcher_send_custom_event(app->view_dispatcher, MetroflipCustomEventPollerSuccess);
        return NfcCommandStop;
    }

    return NfcCommandContinue;
}

static void intertic_on_enter(Metroflip* app) {
    dolphin_deed(DolphinDeedNfcRead);

    if(app->data_loaded) {
        FURI_LOG_I(TAG, "Star loaded");
        Storage* storage = furi_record_open(RECORD_STORAGE);
        FlipperFormat* ff = flipper_format_file_alloc(storage);
        if(flipper_format_file_open_existing(ff, app->file_path)) {
            St25tbData* st25tb_data = st25tb_alloc();
            st25tb_load(st25tb_data, ff, 4);
            FuriString* parsed_data = furi_string_alloc();
            Widget* widget = app->widget;

            furi_string_reset(app->text_box_store);
            if(!intertic_parse(parsed_data, st25tb_data)) {
                furi_string_reset(app->text_box_store);
                FURI_LOG_I(TAG, "Unknown card type");
                furi_string_printf(parsed_data, "\e#Unknown card\n");
            }
            widget_add_text_scroll_element(
                widget, 0, 0, 128, 64, furi_string_get_cstr(parsed_data));

            widget_add_button_element(
                widget, GuiButtonTypeRight, "Exit", metroflip_exit_widget_callback, app);
            widget_add_button_element(
                widget, GuiButtonTypeCenter, "Delete", metroflip_delete_widget_callback, app);
            st25tb_free(st25tb_data);
            furi_string_free(parsed_data);
            view_dispatcher_switch_to_view(app->view_dispatcher, MetroflipViewWidget);
        }
        flipper_format_free(ff);
    } else {
        FURI_LOG_I(TAG, "Star not loaded");
        // Setup view
        Popup* popup = app->popup;
        popup_set_header(popup, "Apply\n card to\nthe back", 68, 30, AlignLeft, AlignTop);
        popup_set_icon(popup, 0, 3, &I_RFIDDolphinReceive_97x61);

        // Start worker
        view_dispatcher_switch_to_view(app->view_dispatcher, MetroflipViewPopup);
        app->poller = nfc_poller_alloc(app->nfc, NfcProtocolSt25tb);
        nfc_poller_start(app->poller, intertic_poller_callback, app);

        metroflip_app_blink_start(app);
    }
}

static bool intertic_on_event(Metroflip* app, SceneManagerEvent event) {
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == MetroflipCustomEventCardDetected) {
            Popup* popup = app->popup;
            popup_set_header(popup, "DON'T\nMOVE", 68, 30, AlignLeft, AlignTop);
            consumed = true;
        } else if(event.event == MetroflipCustomEventPollerSuccess) {
            const St25tbData* st25tb_data = nfc_device_get_data(app->nfc_device, NfcProtocolSt25tb);
            FuriString* parsed_data = furi_string_alloc();
            Widget* widget = app->widget;

            furi_string_reset(app->text_box_store);
            if(!intertic_parse(parsed_data, st25tb_data)) {
                furi_string_reset(app->text_box_store);
                FURI_LOG_I(TAG, "Unknown card type");
                furi_string_printf(parsed_data, "\e#Unknown card\n");
            }
            widget_add_text_scroll_element(
                widget, 0, 0, 128, 64, furi_string_get_cstr(parsed_data));
            widget_add_button_element(
                widget, GuiButtonTypeLeft, "Exit", metroflip_exit_widget_callback, app);
            widget_add_button_element(
                widget, GuiButtonTypeRight, "Save", metroflip_save_widget_callback, app);
            widget_add_button_element(
                widget, GuiButtonTypeCenter, "Delete", metroflip_delete_widget_callback, app);
            furi_string_free(parsed_data);
            view_dispatcher_switch_to_view(app->view_dispatcher, MetroflipViewWidget);
            metroflip_app_blink_stop(app);
            consumed = true;
        } else if(event.event == MetroflipCustomEventCardLost) {
            Popup* popup = app->popup;
            popup_set_header(popup, "Card \n lost", 68, 30, AlignLeft, AlignTop);
            consumed = true;
        } else if(event.event == MetroflipCustomEventWrongCard) {
            Popup* popup = app->popup;
            popup_set_header(popup, "WRONG \n CARD", 68, 30, AlignLeft, AlignTop);
            consumed = true;
        } else if(event.event == MetroflipCustomEventPollerFail) {
            Popup* popup = app->popup;
            popup_set_header(popup, "Failed", 68, 30, AlignLeft, AlignTop);
            consumed = true;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        scene_manager_search_and_switch_to_previous_scene(app->scene_manager, MetroflipSceneStart);
        consumed = true;
    }

    return consumed;
}

static void intertic_on_exit(Metroflip* app) {
    widget_reset(app->widget);

    if(app->poller && !app->data_loaded) {
        nfc_poller_stop(app->poller);
        nfc_poller_free(app->poller);
    }

    // Clear view
    popup_reset(app->popup);

    metroflip_app_blink_stop(app);
}

/* Actual implementation of app<>plugin interface */
static const MetroflipPlugin intertic_plugin = {
    .card_name = "Star",
    .plugin_on_enter = intertic_on_enter,
    .plugin_on_event = intertic_on_event,
    .plugin_on_exit = intertic_on_exit,

};

/* Plugin descriptor to comply with basic plugin specification */
static const FlipperAppPluginDescriptor intertic_plugin_descriptor = {
    .appid = METROFLIP_SUPPORTED_CARD_PLUGIN_APP_ID,
    .ep_api_version = METROFLIP_SUPPORTED_CARD_PLUGIN_API_VERSION,
    .entry_point = &intertic_plugin,
};

/* Plugin entry point - must return a pointer to const descriptor  */
const FlipperAppPluginDescriptor* intertic_plugin_ep(void) {
    return &intertic_plugin_descriptor;
}
