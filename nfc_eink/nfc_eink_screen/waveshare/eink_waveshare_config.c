#include "eink_waveshare_config.h"

#define TAG "WSH_Config"

EinkScreenTypeWaveshare eink_waveshare_config_get_protocol_screen_type_by_name(FuriString* name) {
    UNUSED(name);
    EinkScreenTypeWaveshare type = EinkScreenTypeWaveshareInvalid;

    const NfcEinkScreenInfo* item = nfc_eink_descriptor_get_by_name(name);
    if(item) {
        type = item->protocol_type_field;
    }

    return type;
}

const char* eink_waveshare_config_get_screen_name_by_protocol_screen_type(
    EinkScreenTypeWaveshare protocol_screen_type) {
    const char* name = NFC_EINK_SCREEN_UNKNOWN;

    const NfcEinkScreenInfo* item =
        nfc_eink_descriptor_get_by_protocol_field(protocol_screen_type);
    if(item) {
        name = item->name;
    }

    return name;
}
