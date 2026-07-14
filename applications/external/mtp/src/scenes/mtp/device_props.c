#include "device_props.h"
#include "utils.h"
#include "usb_desc.h"
#include "mtp_support.h"
#include <power/power_service/power.h>

int GetDevicePropValueInternal(uint32_t prop_code, uint8_t* buffer) {
    uint8_t* ptr = buffer;
    uint16_t length;

    switch(prop_code) {
    case 0xd402: {
        const char* deviceName = furi_hal_version_get_name_ptr();
        if(deviceName == NULL) {
            deviceName = "Flipper Zero";
        }

        WriteMTPString(ptr, deviceName, &length);
        ptr += length;
        break;
    }
    case 0x5001: {
        // Battery level
        Power* power = furi_record_open(RECORD_POWER);
        FURI_LOG_I("MTP", "Getting battery level");
        if(power == NULL) {
            *(uint8_t*)ptr = 0x00;
        } else {
            PowerInfo info;
            power_get_info(power, &info);

            FURI_LOG_I("MTP", "Battery level: %d", info.charge);

            *(uint8_t*)ptr = info.charge;
            furi_record_close(RECORD_POWER);
        }
        ptr += sizeof(uint8_t);
        break;
    }
    default:
        // Unsupported property
        break;
    }

    return ptr - buffer;
}

int GetDevicePropDescInternal(uint32_t prop_code, uint8_t* buffer) {
    uint8_t* ptr = buffer;
    uint16_t length;

    switch(prop_code) {
    case 0xd402:
        // Device friendly name
        *(uint16_t*)ptr = prop_code;
        ptr += 2;

        // type is string
        *(uint16_t*)ptr = 0xffff;
        ptr += 2;

        // read-only
        *(uint8_t*)ptr = 0x00;
        ptr += 1;

        length = GetDevicePropValueInternal(prop_code, ptr);
        ptr += length;

        length = GetDevicePropValueInternal(prop_code, ptr);
        ptr += length;

        // no-form
        *(uint8_t*)ptr = 0x00;
        ptr += 1;
        break;
    case 0x5001:
        // Device friendly name
        *(uint16_t*)ptr = prop_code;
        ptr += 2;

        // type is uint8
        *(uint16_t*)ptr = 0x0002;
        ptr += 2;

        // read-only
        *(uint8_t*)ptr = 0x00;
        ptr += 1;

        length = GetDevicePropValueInternal(prop_code, ptr);
        ptr += length;

        length = GetDevicePropValueInternal(prop_code, ptr);
        ptr += length;

        // no-form
        *(uint8_t*)ptr = 0x00;
        ptr += 1;
        break;

    default:
        // Unsupported property
        break;
    }

    return ptr - buffer;
}

void GetDevicePropValue(AppMTP* mtp, uint32_t transaction_id, uint32_t prop_code) {
    uint8_t* response = malloc(sizeof(uint8_t) * MTP_BUFFER_SIZE);
    int length = GetDevicePropValueInternal(prop_code, response);
    send_mtp_response_buffer(
        mtp, MTP_TYPE_DATA, MTP_OP_GET_DEVICE_PROP_VALUE, transaction_id, response, length);
    send_mtp_response_buffer(mtp, MTP_TYPE_RESPONSE, MTP_RESP_OK, transaction_id, NULL, 0);
    free(response);
}

void GetDevicePropDesc(AppMTP* mtp, uint32_t transaction_id, uint32_t prop_code) {
    uint8_t* response = malloc(sizeof(uint8_t) * MTP_BUFFER_SIZE);
    int length = GetDevicePropDescInternal(prop_code, response);
    send_mtp_response_buffer(
        mtp, MTP_TYPE_DATA, MTP_OP_GET_DEVICE_PROP_DESC, transaction_id, response, length);
    send_mtp_response_buffer(mtp, MTP_TYPE_RESPONSE, MTP_RESP_OK, transaction_id, NULL, 0);
    free(response);
}

int BuildDeviceInfo(uint8_t* buffer) {
    uint8_t* ptr = buffer;
    uint16_t length;

    // Standard version
    *(uint16_t*)ptr = 100;
    ptr += sizeof(uint16_t);

    // Vendor extension ID
    *(uint32_t*)ptr = MTP_VENDOR_EXTENSION_ID;
    ptr += sizeof(uint32_t);

    // Vendor extension version
    *(uint16_t*)ptr = MTP_VENDOR_EXTENSION_VERSION;
    ptr += sizeof(uint16_t);

    // Vendor extension description
    WriteMTPString(ptr, "microsoft.com: 1.0;", &length);
    ptr += length;

    // Functional mode
    FURI_LOG_I("MTP", "MTP Functional Mode");
    *(uint16_t*)ptr = MTP_FUNCTIONAL_MODE;
    ptr += sizeof(uint16_t);

    // Operations supported
    FURI_LOG_I("MTP", "Writing Operation Supported");
    length = sizeof(supported_operations) / sizeof(uint16_t);
    *(uint32_t*)ptr = length; // Number of supported operations
    FURI_LOG_I("MTP", "Supported Operations: %d", length);
    ptr += sizeof(uint32_t);

    for(int i = 0; i < length; i++) {
        FURI_LOG_I("MTP", "Operation: %04x", supported_operations[i]);
        *(uint16_t*)ptr = supported_operations[i];
        ptr += sizeof(uint16_t);
    }

    // Supported events (example, add as needed)
    FURI_LOG_I("MTP", "Supported Events");
    *(uint32_t*)ptr = 0; // Number of supported events
    ptr += sizeof(uint32_t);

    FURI_LOG_I("MTP", "Supported Device Properties");
    length = sizeof(supported_device_properties) / sizeof(uint16_t);
    *(uint32_t*)ptr = length; // Number of supported device properties
    ptr += sizeof(uint32_t);

    for(int i = 0; i < length; i++) {
        FURI_LOG_I("MTP", "Supported Device Props");
        *(uint16_t*)ptr = supported_device_properties[i];
        ptr += sizeof(uint16_t);
    }

    // Supported capture formats (example, add as needed)
    *(uint32_t*)ptr = 0; // Number of supported capture formats
    ptr += sizeof(uint32_t);

    // Supported playback formats (example, add as needed)
    FURI_LOG_I("MTP", "Supported Playback Formats");
    length = sizeof(supported_playback_formats) / sizeof(uint16_t);
    *(uint32_t*)ptr = length; // Number of supported playback formats
    ptr += sizeof(uint32_t);

    for(int i = 0; i < length; i++) {
        *(uint16_t*)ptr = supported_playback_formats[i];
        ptr += sizeof(uint16_t);
    }

    // Manufacturer
    WriteMTPString(ptr, USB_MANUFACTURER_STRING, &length);
    ptr += length;

    // Model
    WriteMTPString(ptr, USB_DEVICE_MODEL, &length);
    ptr += length;

    // Device version
    const Version* ver = furi_hal_version_get_firmware_version();
    WriteMTPString(ptr, version_get_version(ver), &length);
    ptr += length;

    // Serial number
    char* serial = malloc(sizeof(char) * furi_hal_version_uid_size() * 2 + 1);
    const uint8_t* uid = furi_hal_version_uid();
    for(size_t i = 0; i < furi_hal_version_uid_size(); i++) {
        serial[i * 2] = byte_to_hex(uid[i] >> 4);
        serial[i * 2 + 1] = byte_to_hex(uid[i] & 0x0F);
    }
    serial[furi_hal_version_uid_size() * 2] = '\0';

    WriteMTPString(ptr, serial, &length);
    ptr += length;

    free(serial);

    return ptr - buffer;
}
