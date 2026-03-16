#include <lib/nfc/protocols/mf_desfire/mf_desfire.h>
#include "../metroflip_i.h"
#include "desfire.h"
#include <lib/toolbox/strint.h>
#include <stdio.h>
#include <string.h> // memcmp, strcmp

bool app_id_matches(uint32_t input, const MfDesfireApplicationId* expected) {
    if(!expected) return false; // don't check expected->data (it's a fixed array)
    // AIDs in the cards[] array are stored as big-endian values
    // e.g., AID 0x010000 means bytes: 01 00 00
    uint8_t bytes[MF_DESFIRE_APP_ID_SIZE] = {
        (input >> 16) & 0xFF, (input >> 8) & 0xFF, input & 0xFF};
    return memcmp(bytes, expected->data, MF_DESFIRE_APP_ID_SIZE) == 0;
}

typedef struct {
    uint32_t aid;
    const char* card_name;
    const char* company;
    bool locked;
} TransitCardInfo;

TransitCardInfo cards[90] = {
    {0x000001, "MAD TTP / MNL beep", "CRTM / AFPI", true},
    {0x000002, "MNL beep", "AFPI", true},
    {0x000003, "MNL beep", "AFPI", true},
    {0x000004, "MNL beep", "AFPI", true},
    {0x0011F2, "myki", "TV", false},
    {0x002000, "YYZ Presto", "Metrolinx", true},
    {0x004048, "GDL Mi Movilidad", "SITEUR", true},
    {0x004055, "AUK AT HOP", "Auckland Transport", true},
    {0x004063, "DOH Travel Pass", "Qatar Rail", true},
    {0x004078, "nol", "RTA", false},
    {0x008057, "NORTIC", "NRPA", true},
    {0x010000,
     "Breeze / Compass / EASY / FREEDOM / Urbana",
     "MARTA / TransLink / MIA County / PATCO / LPP",
     true},
    {0x012340, "ECN motion", "MoTCW", true},
    {0x012350, "ECN motion", "MoTCW", true},
    {0x012360, "ECN motion", "MoTCW", true},
    {0x018057, "NORTIC", "NRPA", true},
    {0x0112F2, "Tap-N-Go / peggo", "GBMT / YWG Transit", true},
    {0x014D44, "DEL DMTC", "DMRCL", true},
    {0x020000, "LJU Urbana", "LPP", true},
    {0x0212F2, "GRB Tap-N-Go", "GBM Transit", true},
    {0x024D44, "DEL DMTC", "DMRCL", true},
    {0x034D44, "DEL DMTC", "DMRCL", true},
    {0x044D44, "DEL DMTC", "DMRCL", true},
    {0x050000, "BCN T-mobilitat / LJU Urbana", "TMB / LPP", true},
    {0x054D44, "DEL DMTC", "DMRCL", true},
    {0x064D44, "DEL DMTC", "DMRCL", true},
    {0x074D44, "DEL DMTC", "DMRCL", true},
    {0x1101F4, "itso", "ITSO (UK)", false},
    {0x1120EF, "HEL HSL", "HRT", true},
    {0x1201F4, "itso", "ITSO (UK)", false},
    {0x1301F4, "itso", "ITSO (UK)", false},
    {0x1401F4, "itso", "ITSO (UK)", false},
    {0x1602A0, "itso", "ITSO (UK)", false},
    {0x171108, "MNL TRIPKO", "JourneyTech", true},
    {0x227508, "Umo", "Cubic", true},
    {0x3010F2, "SEA ORCA", "ORCA", true},
    {0xF013F2, "SEA ORCA", "ORCA", true},
    {0x314553, "opal", "Opal", false},
    {0x315441, "ATH ATH.ENA", "OASA", true},
    {0x31594F, "LHR Oyster", "TfL", true},
    {0x4012F2, "Connect (SMF)", "SACOG", true},
    {0x422201, "IST Istanbulkart", "BELBIM", true},
    {0x422204, "IST Istanbulkart", "BELBIM", true},
    {0x422205, "IST Istanbulkart", "BELBIM", true},
    {0x422206, "IST Istanbulkart", "BELBIM", true},
    {0x425301, "BKK MRT SVC / BKK Rabbit", "BEM / BTS", true},
    {0x425302, "BKK MRT SVC / BKK Rabbit", "BEM / BTS", true},
    {0x425303, "BKK MRT SVC / BKK Rabbit", "BEM / BTS", true},
    {0x425304, "BKK MRT SVC / BKK Rabbit", "BEM / BTS", true},
    {0x425305, "BKK MRT SVC / BKK Rabbit", "BEM / BTS", true},
    {0x425306, "BKK MRT SVC / BKK Rabbit", "BEM / BTS", true},
    {0x425307, "BKK MRT SVC / BKK Rabbit", "BEM / BTS", true},
    {0x425308, "BKK MRT SVC / BKK Rabbit", "BEM / BTS", true},
    {0x425309, "BKK MRT SVC / BKK Rabbit", "BEM / BTS", true},
    {0x42530A, "BKK MRT SVC", "BEM", true},
    {0x42530B, "BKK MRT SVC", "BEM", true},
    {0x42530C, "BKK MRT SVC", "BEM", true},
    {0x42530D, "BKK MRT SVC", "BEM", true},
    {0x42530E, "BKK MRT SVC", "BEM", true},
    {0x42530F, "BKK MRT SVC", "BEM", true},
    {0x425310, "BKK MRT SVC", "BEM", true},
    {0x425311, "BKK MRT SVC", "BEM", true},
    {0x5010F2, "CHC Metrocard", "ECan", true},
    {0x5011F2, "PRG Litacka", "Haguess", true},
    {0x6013F2, "HNL HOLO", "Honolulu County", true},
    {0x7A007A, "LAS TAP & GO", "RTC", true},
    {0x7D23A4, "Umo", "Cubic", true},
    {0x805BC6, "Umo", "Cubic", true},
    {0x8E7F67, "Umo", "Cubic", true},
    {0x8113F2, "ORD Ventra", "CTA", true},
    {0x9011F2, "clipper", "Clipper", false},
    {0x9013F2, "DUD Bee", "Otago RC", true},
    {0x9111F2, "clipper", "Clipper", false},
    {0xA012F2, "BDL Go CT", "CTtransit", true},
    {0xA013F2, "PVD Wave", "RIPTA", true},
    {0xAF1122, "DUB Leap (DUB)", "TFI", true},
    {0xB006F2, "ADL metroCARD", "Adelaide Metro", true},
    {0xB52C99, "Umo", "Cubic", true},
    {0xCA3490, "SOF City Card", "UMC", true},
    {0xCC00CC, "CMH Smartcard", "COTA", true},
    {0xD000D0, "DAY Tapp Pay", "RTA", true},
    {0xDD00DD, "DEN MyRide", "RTD", true},
    {0xD001F0, "VIT BAT", "Euskotren", true},
    {0xE010F2, "PDX Hop Fastpass", "TriMet", true},
    {0xF00000, "JFK OMNY", "MTA", true},
    {0xF010F2, "myki", "myki", false},
    {0xF18301, "WRO URBANCARD", "UTS", true},
    {0xF18302, "WRO URBANCARD", "UTS", true},
    {0xF18303, "WRO URBANCARD", "UTS", true},
    {0xFF30FF, "YYZ Presto", "Metrolinx", true},
};

int num_cards = sizeof(cards) / sizeof(cards[0]);

const char* desfire_type(const MfDesfireData* data) {
    if(!data) return "Unknown Card";
    if(!data->application_ids) return "Unknown Card";

    const uint32_t card_app_id_count = simple_array_get_count(data->application_ids);
    FURI_LOG_I(
        "Metroflip:DesfireManager", "Found %lu application IDs", (unsigned long)card_app_id_count);

    // Log all AIDs found on the card for debugging
    for(uint32_t j = 0; j < card_app_id_count; ++j) {
        const MfDesfireApplicationId* app_ptr =
            (const MfDesfireApplicationId*)simple_array_cget(data->application_ids, j);
        if(app_ptr) {
            FURI_LOG_I(
                "Metroflip:DesfireManager",
                "AID[%lu]: %02X %02X %02X",
                (unsigned long)j,
                app_ptr->data[0],
                app_ptr->data[1],
                app_ptr->data[2]);
        }
    }

    for(int i = 0; i < num_cards; i++) {
        for(uint32_t j = 0; j < card_app_id_count; ++j) {
            const MfDesfireApplicationId* app_ptr =
                (const MfDesfireApplicationId*)simple_array_cget(data->application_ids, j);
            if(!app_ptr) continue; // defensive: skip invalid entries

            if(app_id_matches(cards[i].aid, app_ptr)) {
                FURI_LOG_I("Metroflip:DesfireManager", "matches with %s!", cards[i].card_name);
                return cards[i].card_name;
            }
        }
    }
    return "Unknown Card";
}

bool is_desfire_locked(const char* card_name) {
    if(!card_name) return true; // treat null/unknown as locked (conservative)
    for(int i = 0; i < num_cards; i++) {
        if(strcmp(card_name, cards[i].card_name) == 0) {
            return cards[i].locked;
        }
    }
    return true;
}
