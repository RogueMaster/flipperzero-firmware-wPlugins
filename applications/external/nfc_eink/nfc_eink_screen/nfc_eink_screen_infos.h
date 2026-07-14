#pragma once
#include <furi.h>
#include <m-array.h>

typedef enum {
    NfcEinkScreenSizeUnknown,
    NfcEinkScreenSize2n13inch,
    NfcEinkScreenSize2n9inch,
    NfcEinkScreenSize1n54inch,
    NfcEinkScreenSize3n71inch,
    NfcEinkScreenSize2n7inch,
    NfcEinkScreenSize4n2inch,
    NfcEinkScreenSize7n5inch,

    NfcEinkScreenSizeNum
} NfcEinkScreenSize;

typedef enum {
    NfcEinkManufacturerWaveshare,
    NfcEinkManufacturerGoodisplay,

    NfcEinkManufacturerNum,
    NfcEinkManufacturerUnknown
} NfcEinkManufacturer;

#define NFC_EINK_SCREEN_UNKNOWN        "Unknown"
#define NFC_EINK_SCREEN_WAVESHARE_2n13 "Waveshare 2.13 inch"
#define NFC_EINK_SCREEN_WAVESHARE_2n7  "Waveshare 2.7 inch"
#define NFC_EINK_SCREEN_WAVESHARE_2n9  "Waveshare 2.9 inch"
#define NFC_EINK_SCREEN_WAVESHARE_4n2  "Waveshare 4.2 inch"
#define NFC_EINK_SCREEN_WAVESHARE_7n5  "Waveshare 7.5 inch"
#define NFC_EINK_SCREEN_GDEY0154D67    "GDEY0154D67"
#define NFC_EINK_SCREEN_GDEY0213B74    "GDEY0213B74"
#define NFC_EINK_SCREEN_GDEY029T94     "GDEY029T94"
#define NFC_EINK_SCREEN_GDEY037T03     "GDEY037T03"

typedef struct {
    uint16_t width;
    uint16_t height;
    uint8_t data_block_size;
    uint32_t protocol_type_field;
    NfcEinkScreenSize screen_size;
    NfcEinkManufacturer screen_manufacturer;
    const char* name;
} NfcEinkScreenInfo;

ARRAY_DEF(EinkScreenInfoArray, const NfcEinkScreenInfo*, M_PTR_OPLIST);

const NfcEinkScreenInfo* nfc_eink_descriptor_get_by_name(const FuriString* name);
const NfcEinkScreenInfo* nfc_eink_descriptor_get_by_index(size_t index);
const NfcEinkScreenInfo* nfc_eink_descriptor_get_by_protocol_field(uint32_t protocol_field);

uint8_t nfc_eink_descriptor_get_all_usable(EinkScreenInfoArray_t result);

uint8_t nfc_eink_descriptor_filter_by_manufacturer(
    EinkScreenInfoArray_t result,
    NfcEinkManufacturer manufacturer);

uint8_t nfc_eink_descriptor_filter_by_screen_size(
    EinkScreenInfoArray_t result,
    NfcEinkScreenSize screen_size);

uint8_t nfc_eink_descriptor_filter_by_name(EinkScreenInfoArray_t result, const char* name);
