#include "eink_goodisplay_config.h"

#define TAG "GD_Config"

const uint8_t default_config[] = {
    0xf0, 0xdb, 0x00, 0x00, 0x67, 0xa0, 0x06, 0x00, 0x20, 0x00, 128,  0x01, 0x28, 0xa4, 0x01, 0x0c,
    0xa5, 0x02, 0x00, 0x0a, 0xa4, 0x01, 0x08, 0xa5, 0x02, 0x00, 0x0a, 0xa4, 0x01, 0x0c, 0xa5, 0x02,
    0x00, 0x0a, 0xa4, 0x01, 0x02, 0xa1, 0x01, 0x12, 0xa4, 0x01, 0x02, 0xa1, 0x04, 0x01, 0x27, 0x01,
    0x01, 0xa1, 0x02, 0x11, 0x01, 0xa1, 0x03, 0x44, 0x00, 0x0f, 0xa1, 0x05, 0x45, 0x27, 0x01, 0x00,
    0x00, 0xa1, 0x02, 0x3c, 0x05, 0xa1, 0x03, 0x21, 0x00, 0x80, 0xa1, 0x02, 0x18, 0x80, 0xa1, 0x02,
    0x4e, 0x00, 0xa1, 0x03, 0x4f, 0x27, 0x01, 0xa3, 0x01, 0x24, 0xa2, 0x02, 0x22, 0xf7, 0xa2, 0x01,
    0x20, 0xa4, 0x01, 0x02, 0xa2, 0x02, 0x10, 0x01, 0xa5, 0x02, 0x00, 0x0a};

EinkGoodisplayConfigPack* eink_goodisplay_config_pack_alloc(size_t* config_length) {
    furi_assert(config_length);

    uint8_t config_size = sizeof(default_config);
    uint8_t* config_ptr = malloc(config_size);
    *config_length = config_size;
    memcpy(config_ptr, default_config, config_size);

    EinkGoodisplayConfigPack* pack = (EinkGoodisplayConfigPack*)config_ptr;
    return pack;
}

void eink_goodisplay_config_pack_free(EinkGoodisplayConfigPack* pack) {
    furi_assert(pack);
    free(pack);
}

static NfcEinkGoodisplayScreenResolution
    eink_goodisplay_config_get_resolution(NfcEinkScreenSize screen_size) {
    switch(screen_size) {
    case NfcEinkScreenSize1n54inch:
        return NfcEinkGoodisplayScreenResolution1n54inch;
    case NfcEinkScreenSize2n13inch:
        return NfcEinkGoodisplayScreenResolution2n13inch;
    case NfcEinkScreenSize2n9inch:
        return NfcEinkGoodisplayScreenResolution2n9inch;
    case NfcEinkScreenSize3n71inch:
        return NfcEinkGoodisplayScreenResolution3n71inch;
    default:
        FURI_LOG_E(TAG, "Resolution %02X is not supported", screen_size);
        furi_crash();
    }
}

void eink_goodisplay_config_pack_set_by_screen_info(
    EinkGoodisplayConfigPack* pack,
    const NfcEinkScreenInfo* info) {
    furi_assert(pack);
    furi_assert(info);
    pack->eink_size_config.screen_data.height = __builtin_bswap16(info->height);
    pack->eink_size_config.screen_data.width = __builtin_bswap16(info->width);
    pack->eink_size_config.screen_data.screen_resolution =
        eink_goodisplay_config_get_resolution(info->screen_size);
    /*Additional goodisplay screen configs can be done here*/
    //pack->update_ctrl1.ram_bypass_inverse_option = 0x08;
}

const char* eink_goodisplay_config_get_screen_name(const uint8_t* data, uint8_t data_length) {
    furi_assert(data);
    furi_assert(data_length >= sizeof(EinkGoodisplaySizeConfigPack));
    const char* name = NFC_EINK_SCREEN_UNKNOWN;

    const EinkGoodisplaySizeConfigPack* config = (EinkGoodisplaySizeConfigPack*)(data);

    if(config->screen_data.screen_resolution == NfcEinkGoodisplayScreenResolution2n13inch) {
        name = NFC_EINK_SCREEN_GDEY0213B74;
    } else if(config->screen_data.screen_resolution == NfcEinkGoodisplayScreenResolution2n9inch) {
        name = NFC_EINK_SCREEN_GDEY029T94;
    } else if(config->screen_data.screen_resolution == NfcEinkGoodisplayScreenResolution1n54inch) {
        name = NFC_EINK_SCREEN_GDEY0154D67;
    } else if(config->screen_data.screen_resolution == NfcEinkGoodisplayScreenResolution3n71inch) {
        name = NFC_EINK_SCREEN_GDEY037T03;
    }

    return name;
}
