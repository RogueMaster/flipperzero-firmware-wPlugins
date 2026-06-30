#include "nfc_eink_screen_i.h"

#include <../lib/flipper_format/flipper_format.h>
#include <../applications/services/storage/storage.h>

#define TAG "NfcEinkScreen"

#define NFC_EINK_FORMAT_VERSION               (1)
#define NFC_EINK_FILE_HEADER                  "Flipper NFC Eink screen"
#define NFC_EINK_DEVICE_UID_KEY               "UID"
#define NFC_EINK_DEVICE_TYPE_KEY              "NFC type"
#define NFC_EINK_SCREEN_MANUFACTURER_TYPE_KEY "Manufacturer type"
#define NFC_EINK_SCREEN_MANUFACTURER_NAME_KEY "Manufacturer name"
#define NFC_EINK_SCREEN_TYPE_KEY              "Screen type"
#define NFC_EINK_SCREEN_NAME_KEY              "Screen name"
#define NFC_EINK_SCREEN_WIDTH_KEY             "Width"
#define NFC_EINK_SCREEN_HEIGHT_KEY            "Height"
#define NFC_EINK_SCREEN_DATA_BLOCK_SIZE_KEY   "Data block size"
#define NFC_EINK_SCREEN_DATA_TOTAL_KEY        "Data total"
#define NFC_EINK_SCREEN_BLOCK_DATA_KEY        "Block"

extern const NfcEinkScreenHandlers waveshare_handlers;
extern const NfcEinkScreenHandlers goodisplay_handlers;

typedef struct {
    const NfcEinkScreenHandlers* handlers;
    const char* name;
} NfcEinkScreenManufacturerDescriptor;

static const NfcEinkScreenManufacturerDescriptor manufacturers[NfcEinkManufacturerNum] = {
    [NfcEinkManufacturerWaveshare] =
        {
            .handlers = &waveshare_handlers,
            .name = "Waveshare",
        },
    [NfcEinkManufacturerGoodisplay] =
        {
            .handlers = &goodisplay_handlers,
            .name = "Goodisplay",
        },
};

const char* nfc_eink_screen_get_manufacturer_name(NfcEinkManufacturer manufacturer) {
    furi_assert(manufacturer < NfcEinkManufacturerNum);
    return manufacturers[manufacturer].name;
}

NfcEinkScreen* nfc_eink_screen_alloc(NfcEinkManufacturer manufacturer) {
    furi_check(manufacturer < NfcEinkManufacturerNum);

    NfcEinkScreen* screen = malloc(sizeof(NfcEinkScreen));
    screen->error = NfcEinkScreenErrorNone;
    screen->name = furi_string_alloc();
    screen->handlers = manufacturers[manufacturer].handlers;

    screen->device = screen->handlers->alloc();

    screen->data = malloc(sizeof(NfcEinkScreenData));

    screen->tx_buf = bit_buffer_alloc(300);
    screen->rx_buf = bit_buffer_alloc(300);
    return screen;
}

static inline uint16_t nfc_eink_screen_calculate_image_size(const NfcEinkScreenInfo* const info) {
    furi_assert(info);
    return info->width * (info->height % 8 == 0 ? (info->height / 8) : (info->height / 8 + 1));
}

static bool nfc_eink_screen_init_internal(NfcEinkScreen* screen) {
    NfcEinkScreenData* data = screen->data;
    NfcEinkScreenDevice* device = screen->device;

    bool result = false;
    do {
        if(furi_string_equal_str(screen->name, "Unknown")) {
            FURI_LOG_E(TAG, "Cannot init %s screen", furi_string_get_cstr(screen->name));
            break;
        }

        const NfcEinkScreenInfo* info = nfc_eink_descriptor_get_by_name(screen->name);
        if(!info) {
            FURI_LOG_E(
                TAG,
                "Screen %s was not found, unable to init!",
                furi_string_get_cstr(screen->name));
            break;
        }

        memcpy(&data->base, info, sizeof(NfcEinkScreenInfo));

        data->image_size = nfc_eink_screen_calculate_image_size(info);

        device->block_total = data->image_size / info->data_block_size;
        if(data->image_size % info->data_block_size != 0) device->block_total += 1;
        size_t memory_size = device->block_total * data->base.data_block_size;

        data->image_data = malloc(memory_size);
        result = true;
    } while(false);

    return result;
}

void nfc_eink_screen_init(NfcEinkScreen* screen, const char* name) {
    furi_assert(screen);
    furi_assert(name);
    furi_string_set_str(screen->name, name);
    nfc_eink_screen_init_internal(screen);
}

void nfc_eink_screen_free(NfcEinkScreen* screen) {
    furi_check(screen);
    furi_string_free(screen->name);
    screen->handlers->free(screen->device);
    free(screen->device);

    NfcEinkScreenData* data = screen->data;
    free(data->image_data);
    free(data);

    bit_buffer_free(screen->tx_buf);
    bit_buffer_free(screen->rx_buf);
    screen->handlers = NULL;
    free(screen);
}

void nfc_eink_screen_set_callback(
    NfcEinkScreen* screen,
    NfcEinkScreenEventCallback event_callback,
    NfcEinkScreenEventContext event_context) {
    furi_assert(screen);
    furi_assert(event_callback);
    screen->event_callback = event_callback;
    screen->event_context = event_context;
}

NfcDevice* nfc_eink_screen_get_nfc_device(const NfcEinkScreen* screen) {
    furi_assert(screen);
    return screen->device->nfc_device;
}

const NfcEinkScreenInfo* nfc_eink_screen_get_image_info(const NfcEinkScreen* screen) {
    furi_assert(screen);
    return &screen->data->base;
}

const uint8_t* nfc_eink_screen_get_image_data(const NfcEinkScreen* screen) {
    furi_assert(screen);
    return screen->data->image_data;
}

uint16_t nfc_eink_screen_get_image_size(const NfcEinkScreen* screen) {
    furi_assert(screen);
    return screen->data->image_size;
}

uint16_t nfc_eink_screen_get_received_size(const NfcEinkScreen* screen) {
    furi_assert(screen);
    return screen->device->received_data;
}

NfcGenericCallback nfc_eink_screen_get_nfc_callback(const NfcEinkScreen* screen, NfcMode mode) {
    furi_assert(screen);
    furi_assert(mode < NfcModeNum);

    const NfcEinkScreenHandlers* handlers = screen->handlers;
    return (mode == NfcModeListener) ? handlers->listener_callback : handlers->poller_callback;
}

void nfc_eink_screen_get_progress(const NfcEinkScreen* screen, size_t* current, size_t* total) {
    furi_assert(screen);
    furi_assert(current);
    furi_assert(total);

    *current = screen->device->block_current;
    *total = screen->device->block_total;
}

const char* nfc_eink_screen_get_name(const NfcEinkScreen* screen) {
    furi_assert(screen);
    return screen->data->base.name;
}

static void nfc_eink_screen_event_invoke(NfcEinkScreen* instance, NfcEinkScreenEventType type) {
    furi_assert(instance);
    if(instance->event_callback != NULL) {
        instance->event_callback(type, instance->event_context);
    }
}

NfcEinkScreenError nfc_eink_screen_get_error(const NfcEinkScreen* screen) {
    return screen->error;
}

void nfc_eink_screen_set_error(NfcEinkScreen* instance, NfcEinkScreenError error) {
    instance->error = error;
    nfc_eink_screen_event_invoke(instance, NfcEinkScreenEventTypeError);
}

bool nfc_eink_screen_load_info(const char* file_path, const NfcEinkScreenInfo** info) {
    furi_assert(info);
    furi_assert(file_path);
    bool loaded = false;
    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* ff = flipper_format_buffered_file_alloc(storage);
    FuriString* name = furi_string_alloc();

    do {
        if(!flipper_format_buffered_file_open_existing(ff, file_path)) break;

        uint32_t tmp = 0;
        if(!flipper_format_read_uint32(ff, NFC_EINK_SCREEN_MANUFACTURER_TYPE_KEY, &tmp, 1)) break;
        NfcEinkManufacturer manufacturer = tmp;

        if(!flipper_format_read_string(ff, NFC_EINK_SCREEN_NAME_KEY, name)) break;

        uint32_t width = 0;
        if(!flipper_format_read_uint32(ff, NFC_EINK_SCREEN_WIDTH_KEY, &width, 1)) break;

        uint32_t height = 0;
        if(!flipper_format_read_uint32(ff, NFC_EINK_SCREEN_HEIGHT_KEY, &height, 1)) break;

        const NfcEinkScreenInfo* inf_tmp = nfc_eink_descriptor_get_by_name(name);
        if(!inf_tmp) {
            FURI_LOG_E(TAG, "Screen %s was not found", furi_string_get_cstr(name));
            break;
        }

        if(inf_tmp->screen_manufacturer != manufacturer) {
            FURI_LOG_E(TAG, "Loaded manufacturer doesn't match with info field");
            break;
        }

        if(inf_tmp->width != width || inf_tmp->height != height) {
            FURI_LOG_E(TAG, "Loaded screen size doesn't match with info field");
            break;
        }

        *info = inf_tmp;
        loaded = true;
    } while(false);

    furi_string_free(name);
    flipper_format_free(ff);
    furi_record_close(RECORD_STORAGE);

    return loaded;
}

static uint8_t nfc_eink_screen_get_bytes_per_row(uint16_t height) {
    return height % 8 == 0 ? (height / 8) : (height / 8 + 1);
}

static bool nfc_eink_screen_save_block(
    FlipperFormat* ff,
    const uint16_t block_num,
    const uint8_t* block,
    const uint8_t block_size) {
    FuriString* temp_str = furi_string_alloc();
    furi_string_printf(temp_str, "%s %d", NFC_EINK_SCREEN_BLOCK_DATA_KEY, block_num);
    bool block_saved =
        flipper_format_write_hex(ff, furi_string_get_cstr(temp_str), block, block_size);
    free(temp_str);
    return block_saved;
}

static bool nfc_eink_screen_load_block(
    FlipperFormat* ff,
    const uint16_t block_num,
    uint8_t* block,
    const uint8_t block_size) {
    FuriString* temp_str = furi_string_alloc();
    furi_string_printf(temp_str, "%s %d", NFC_EINK_SCREEN_BLOCK_DATA_KEY, block_num);
    bool block_loaded =
        flipper_format_read_hex(ff, furi_string_get_cstr(temp_str), block, block_size);
    furi_string_free(temp_str);
    return block_loaded;
}

bool nfc_eink_screen_load_data(
    const char* file_path,
    NfcEinkScreen* destination,
    const NfcEinkScreenInfo* source_info) {
    furi_assert(destination);
    furi_assert(destination->data->image_data);
    furi_assert(source_info);

    bool loaded = false;
    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* ff = flipper_format_buffered_file_alloc(storage);

    do {
        if(!flipper_format_buffered_file_open_existing(ff, file_path)) break;

        uint32_t block_data_size = 0;
        if(!flipper_format_read_uint32(
               ff, NFC_EINK_SCREEN_DATA_BLOCK_SIZE_KEY, &block_data_size, 1)) {
            FURI_LOG_E(TAG, "Block size load failed");
            break;
        }

        const uint8_t source_bytes_per_row =
            nfc_eink_screen_get_bytes_per_row(source_info->height);
        if(block_data_size != source_bytes_per_row) {
            FURI_LOG_E(TAG, "Block size doesn't match to screen size");
            break;
        }

        NfcEinkScreenData* data = destination->data;

        uint16_t dest_offset = 0;
        uint8_t dest_bytes_per_row = nfc_eink_screen_get_bytes_per_row(data->base.height);
        dest_bytes_per_row = MIN(dest_bytes_per_row, source_bytes_per_row);

        const uint16_t row_cnt = MIN(data->base.width, source_info->width);
        uint8_t* block_buf = malloc(source_bytes_per_row);
        for(uint16_t i = 0; i < row_cnt; i++) {
            loaded = nfc_eink_screen_load_block(ff, i, block_buf, source_bytes_per_row);
            if(!loaded) break;

            memcpy(&data->image_data[dest_offset], block_buf, dest_bytes_per_row);
            dest_offset += dest_bytes_per_row;
        }
        free(block_buf);
    } while(false);

    flipper_format_free(ff);
    furi_record_close(RECORD_STORAGE);

    return loaded;
}

bool nfc_eink_screen_save(const NfcEinkScreen* screen, const char* file_path) {
    furi_assert(screen);
    furi_assert(file_path);

    Storage* storage = furi_record_open(RECORD_STORAGE);

    FlipperFormat* ff = flipper_format_buffered_file_alloc(storage);
    FuriString* temp_str = furi_string_alloc();
    const NfcEinkScreenInfo* source_info = &screen->data->base;

    bool saved = false;
    do {
        if(!flipper_format_buffered_file_open_always(ff, file_path)) break;

        // Write header
        if(!flipper_format_write_header_cstr(ff, NFC_EINK_FILE_HEADER, NFC_EINK_FORMAT_VERSION))
            break;

        // Write device type
        NfcProtocol protocol = nfc_device_get_protocol(screen->device->nfc_device);
        if(!flipper_format_write_string_cstr(
               ff, NFC_EINK_DEVICE_TYPE_KEY, nfc_device_get_protocol_name(protocol)))
            break;

        // Write UID
        furi_string_printf(
            temp_str, "%s size can be different depending on type", NFC_EINK_DEVICE_UID_KEY);
        if(!flipper_format_write_comment(ff, temp_str)) break;

        size_t uid_len;
        const uint8_t* uid = nfc_device_get_uid(screen->device->nfc_device, &uid_len);
        if(!flipper_format_write_hex(ff, NFC_EINK_DEVICE_UID_KEY, uid, uid_len)) break;

        // Write manufacturer type
        uint32_t buf = screen->data->base.screen_manufacturer;
        if(!flipper_format_write_uint32(ff, NFC_EINK_SCREEN_MANUFACTURER_TYPE_KEY, &buf, 1)) break;

        // Write screen name
        if(!flipper_format_write_string_cstr(ff, NFC_EINK_SCREEN_NAME_KEY, screen->data->base.name))
            break;

        // Write screen width
        buf = screen->data->base.width;
        if(!flipper_format_write_uint32(ff, NFC_EINK_SCREEN_WIDTH_KEY, &buf, 1)) break;

        // Write screen height
        buf = screen->data->base.height;
        if(!flipper_format_write_uint32(ff, NFC_EINK_SCREEN_HEIGHT_KEY, &buf, 1)) break;

        // Write data block size
        furi_string_printf(
            temp_str,
            "%s may be different depending on type",
            NFC_EINK_SCREEN_DATA_BLOCK_SIZE_KEY);
        if(!flipper_format_write_comment(ff, temp_str)) break;

        const uint8_t source_bytes_per_row =
            nfc_eink_screen_get_bytes_per_row(source_info->height);
        buf = source_bytes_per_row;
        if(!flipper_format_write_uint32(ff, NFC_EINK_SCREEN_DATA_BLOCK_SIZE_KEY, &buf, 1)) break;

        // Write image total size
        buf = screen->data->image_size;
        if(!flipper_format_write_uint32(ff, NFC_EINK_SCREEN_DATA_TOTAL_KEY, &buf, 1)) break;

        // Write image data
        furi_string_printf(temp_str, "Data");
        if(!flipper_format_write_comment(ff, temp_str)) break;

        const uint16_t source_rows_cnt = source_info->width;
        uint16_t source_offset = 0;
        bool block_saved = false;
        for(uint16_t i = 0; i < source_rows_cnt; i++) {
            block_saved = nfc_eink_screen_save_block(
                ff, i, &screen->data->image_data[source_offset], source_bytes_per_row);
            if(!block_saved) break;
            source_offset += source_bytes_per_row;
        }
        saved = block_saved;
    } while(false);

    flipper_format_free(ff);
    furi_string_free(temp_str);
    furi_record_close(RECORD_STORAGE);

    return saved;
}

bool nfc_eink_screen_delete(const char* file_path) {
    furi_assert(file_path);
    Storage* storage = furi_record_open(RECORD_STORAGE);
    bool deleted = storage_simply_remove(storage, file_path);
    furi_record_close(RECORD_STORAGE);
    return deleted;
}

void nfc_eink_screen_vendor_callback(NfcEinkScreen* instance, NfcEinkScreenEventType type) {
    furi_assert(instance);

    if(type == NfcEinkScreenEventTypeConfigurationReceived) {
        FURI_LOG_D(TAG, "Config received");

        if(!nfc_eink_screen_init_internal(instance)) {
            nfc_eink_screen_set_error(instance, NfcEinkScreenErrorUnsupportedScreen);
        }
    } else
        nfc_eink_screen_event_invoke(instance, type);
}
