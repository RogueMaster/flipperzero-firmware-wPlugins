#include "formats/mfc.h"

char* saflok_log_entry_description(LogEntry entry) {
    // TODO: Log entry descriptions aren't yet known
    switch(entry.diagnostic_code) {
    default:
        return NULL;
    }
}

bool saflok_parse(
    NfcDevice* nfc_device,
    uint8_t* uid,
    size_t* uid_len,
    BasicAccessData* saflok_data) {
    switch(nfc_device_get_protocol(nfc_device)) {
    case NfcProtocolMfClassic:
        return saflok_parse_mf_classic(nfc_device, uid, uid_len, saflok_data);
    default:
        // Unknown format, unable to parse
        return false;
    }

    return true;
}

bool saflok_generate(
    NfcDevice* nfc_device,
    uint8_t* uid,
    size_t uid_len,
    BasicAccessData* saflok_data) {
    switch(saflok_data->format) {
    case SaflipFormatMifareClassic:
        saflok_generate_mf_classic(nfc_device, uid, uid_len, saflok_data);
        break;
    default:
        // Unknown format, unable to generate
        return false;
    }

    return true;
}
