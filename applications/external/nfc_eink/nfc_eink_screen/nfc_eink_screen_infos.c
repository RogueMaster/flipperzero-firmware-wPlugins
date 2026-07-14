#include "nfc_eink_screen_infos.h"
#include "nfc_eink_screen/waveshare/eink_waveshare_config.h"

typedef bool (*NfcEinkDescriptorCompareDelegate)(
    const NfcEinkScreenInfo* const a,
    const NfcEinkScreenInfo* const b);

#define NFC_EINK_SCREEN_PROTOCOL_FIELD_UNUSED (0)

const NfcEinkScreenInfo screen_descriptors[] = {
    {
        .name = NFC_EINK_SCREEN_UNKNOWN,
        .width = 0,
        .height = 0,
        .screen_size = NfcEinkScreenSizeUnknown,
        .screen_manufacturer = NfcEinkManufacturerUnknown,
        .protocol_type_field = NFC_EINK_SCREEN_PROTOCOL_FIELD_UNUSED,
        .data_block_size = 0,
    },
    {
        .name = NFC_EINK_SCREEN_WAVESHARE_2n13,
        .width = 250,
        .height = 122,
        .screen_size = NfcEinkScreenSize2n13inch,
        .screen_manufacturer = NfcEinkManufacturerWaveshare,
        .protocol_type_field = EinkScreenTypeWaveshare2n13inch,
        .data_block_size = 16,
    },
    {
        .name = NFC_EINK_SCREEN_WAVESHARE_2n7,
        .width = 264,
        .height = 176,
        .screen_size = NfcEinkScreenSize2n7inch,
        .screen_manufacturer = NfcEinkManufacturerWaveshare,
        .protocol_type_field = EinkScreenTypeWaveshare2n7inch,
        .data_block_size = 121,
    },
    {
        .name = NFC_EINK_SCREEN_WAVESHARE_2n9,
        .width = 296,
        .height = 128,
        .screen_size = NfcEinkScreenSize2n9inch,
        .screen_manufacturer = NfcEinkManufacturerWaveshare,
        .protocol_type_field = EinkScreenTypeWaveshare2n9inch,
        .data_block_size = 16,
    },
    {
        .name = NFC_EINK_SCREEN_WAVESHARE_4n2,
        .width = 300,
        .height = 400,
        .screen_size = NfcEinkScreenSize4n2inch,
        .screen_manufacturer = NfcEinkManufacturerWaveshare,
        .protocol_type_field = EinkScreenTypeWaveshare4n2inch,
        .data_block_size = 100,
    },
    {
        .name = NFC_EINK_SCREEN_WAVESHARE_7n5,
        .width = 480,
        .height = 800,
        .screen_size = NfcEinkScreenSize7n5inch,
        .screen_manufacturer = NfcEinkManufacturerWaveshare,
        .protocol_type_field = EinkScreenTypeWaveshare7n5inch,
        .data_block_size = 120,
    },
    {
        .name = NFC_EINK_SCREEN_GDEY0154D67,
        .width = 200,
        .height = 200,
        .screen_size = NfcEinkScreenSize1n54inch,
        .screen_manufacturer = NfcEinkManufacturerGoodisplay,
        .protocol_type_field = NFC_EINK_SCREEN_PROTOCOL_FIELD_UNUSED,
        .data_block_size = 0xFA,
    },
    {
        .name = NFC_EINK_SCREEN_GDEY0213B74,
        .width = 250,
        .height = 122,
        .screen_size = NfcEinkScreenSize2n13inch,
        .screen_manufacturer = NfcEinkManufacturerGoodisplay,
        .protocol_type_field = NFC_EINK_SCREEN_PROTOCOL_FIELD_UNUSED,
        .data_block_size = 0xFA,
    },
    {
        .name = NFC_EINK_SCREEN_GDEY029T94,
        .width = 296,
        .height = 128,
        .screen_size = NfcEinkScreenSize2n9inch,
        .screen_manufacturer = NfcEinkManufacturerGoodisplay,
        .protocol_type_field = NFC_EINK_SCREEN_PROTOCOL_FIELD_UNUSED,
        .data_block_size = 0xFA,
    },
    {
        .name = NFC_EINK_SCREEN_GDEY037T03,
        .width = 416,
        .height = 240,
        .screen_size = NfcEinkScreenSize3n71inch,
        .screen_manufacturer = NfcEinkManufacturerGoodisplay,
        .protocol_type_field = NFC_EINK_SCREEN_PROTOCOL_FIELD_UNUSED,
        .data_block_size = 0xFA,
    },
};

const NfcEinkScreenInfo* nfc_eink_descriptor_get_by_index(size_t index) {
    furi_assert(index < COUNT_OF(screen_descriptors));
    return &screen_descriptors[index];
}

static const NfcEinkScreenInfo* nfc_eink_descriptor_search_by(
    const NfcEinkScreenInfo* sample,
    NfcEinkDescriptorCompareDelegate compare) {
    const NfcEinkScreenInfo* item = NULL;

    for(uint8_t i = 0; i < COUNT_OF(screen_descriptors); i++) {
        if(compare(&screen_descriptors[i], sample)) {
            item = &screen_descriptors[i];
            break;
        }
    }
    return item;
}

static uint8_t nfc_eink_descriptor_filter_by(
    EinkScreenInfoArray_t result,
    const NfcEinkScreenInfo* sample,
    NfcEinkDescriptorCompareDelegate compare) {
    uint8_t count = 0;
    for(uint8_t i = 0; i < COUNT_OF(screen_descriptors); i++) {
        if(compare(&screen_descriptors[i], sample)) {
            EinkScreenInfoArray_push_back(result, &screen_descriptors[i]);
            count++;
        }
    }
    return count;
}

static inline bool nfc_eink_descriptor_compare_by_manufacturer(
    const NfcEinkScreenInfo* const a,
    const NfcEinkScreenInfo* const b) {
    return a->screen_manufacturer == b->screen_manufacturer;
}

static inline bool nfc_eink_descriptor_compare_by_screen_size(
    const NfcEinkScreenInfo* const a,
    const NfcEinkScreenInfo* const b) {
    return a->screen_size == b->screen_size;
}

static inline bool nfc_eink_descriptor_compare_by_protocol_field(
    const NfcEinkScreenInfo* const a,
    const NfcEinkScreenInfo* const b) {
    return a->protocol_type_field == b->protocol_type_field;
}

static inline bool nfc_eink_descriptor_compare_by_name(
    const NfcEinkScreenInfo* const a,
    const NfcEinkScreenInfo* const b) {
    FuriString* a_name = furi_string_alloc_set_str(a->name);
    FuriString* b_name = furi_string_alloc_set_str(b->name);

    bool result = furi_string_equal(a_name, b_name);
    furi_string_free(a_name);
    furi_string_free(b_name);
    return result;
}

const NfcEinkScreenInfo* nfc_eink_descriptor_get_by_name(const FuriString* name) {
    furi_assert(name);
    NfcEinkScreenInfo dummy = {.name = furi_string_get_cstr(name)};
    return nfc_eink_descriptor_search_by(&dummy, nfc_eink_descriptor_compare_by_name);
}

const NfcEinkScreenInfo* nfc_eink_descriptor_get_by_protocol_field(uint32_t protocol_field) {
    furi_assert(protocol_field != NFC_EINK_SCREEN_PROTOCOL_FIELD_UNUSED);
    NfcEinkScreenInfo dummy = {.protocol_type_field = protocol_field};
    return nfc_eink_descriptor_search_by(&dummy, nfc_eink_descriptor_compare_by_protocol_field);
}

uint8_t nfc_eink_descriptor_filter_by_manufacturer(
    EinkScreenInfoArray_t result,
    NfcEinkManufacturer manufacturer) {
    furi_assert(result);
    furi_assert(manufacturer < NfcEinkManufacturerNum);
    furi_assert(manufacturer != NfcEinkManufacturerUnknown);

    NfcEinkScreenInfo dummy = {.screen_manufacturer = manufacturer};
    return nfc_eink_descriptor_filter_by(
        result, &dummy, nfc_eink_descriptor_compare_by_manufacturer);
}

uint8_t nfc_eink_descriptor_filter_by_screen_size(
    EinkScreenInfoArray_t result,
    NfcEinkScreenSize screen_size) {
    furi_assert(result);
    furi_assert(screen_size != NfcEinkScreenSizeUnknown);
    furi_assert(screen_size < NfcEinkScreenSizeNum);

    NfcEinkScreenInfo dummy = {.screen_size = screen_size};
    return nfc_eink_descriptor_filter_by(
        result, &dummy, nfc_eink_descriptor_compare_by_screen_size);
}

uint8_t nfc_eink_descriptor_filter_by_name(EinkScreenInfoArray_t result, const char* name) {
    furi_assert(result);
    furi_assert(name);

    FuriString* str = furi_string_alloc_set(name);
    EinkScreenInfoArray_push_back(result, nfc_eink_descriptor_get_by_name(str));
    furi_string_free(str);
    return 1;
}

uint8_t nfc_eink_descriptor_get_all_usable(EinkScreenInfoArray_t result) {
    furi_assert(result);

    uint8_t count = 0;
    for(uint8_t i = 1; i < COUNT_OF(screen_descriptors); i++) {
        EinkScreenInfoArray_push_back(result, &screen_descriptors[i]);
        count++;
    }
    return count;
}
