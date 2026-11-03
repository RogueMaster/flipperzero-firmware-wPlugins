#include "clipboard_service.h"

#include <furi.h>
#include <ble/core/ble_std.h>
#include <furi_ble/event_dispatcher.h>
#include <furi_ble/gatt.h>

#define CLIPBOARD_VALUE_MAX                    128
#define ACI_GATT_ATTRIBUTE_MODIFIED_EVENT_CODE 0x0C01

typedef struct {
    uint8_t type;
    uint8_t data[1];
} FURI_PACKED ClipboardHciUartPacket;

typedef struct {
    uint8_t event;
    uint8_t parameter_length;
    uint8_t data[1];
} FURI_PACKED ClipboardHciEventPacket;

typedef struct {
    uint16_t event_code;
    uint8_t data[1];
} FURI_PACKED ClipboardBlecoreEvent;

static const Service_UUID_t clipboard_service_uuid = {
    .Service_UUID_128 =
        {0x01,
         0x03,
         0xAA,
         0x10,
         0x77,
         0x3D,
         0x52,
         0x8E,
         0x9A,
         0x4C,
         0x21,
         0x7B,
         0xC0,
         0xD9,
         0xF1,
         0xA6},
};

#define CLIPBOARD_TX_UUID \
    {0x02, 0x03, 0xAA, 0x10, 0x77, 0x3D, 0x52, 0x8E, 0x9A, 0x4C, 0x21, 0x7B, 0xC0, 0xD9, 0xF1, 0xA6}
#define CLIPBOARD_RX_UUID \
    {0x03, 0x03, 0xAA, 0x10, 0x77, 0x3D, 0x52, 0x8E, 0x9A, 0x4C, 0x21, 0x7B, 0xC0, 0xD9, 0xF1, 0xA6}

typedef struct {
    const uint8_t* data;
    uint16_t size;
} ClipboardValue;

static bool
    clipboard_value_callback(const void* context, const uint8_t** data, uint16_t* data_size) {
    if(!data) {
        *data_size = CLIPBOARD_VALUE_MAX;
    } else {
        const ClipboardValue* value = context;
        *data = value->data;
        *data_size = value->size;
    }
    return false;
}

enum {
    ClipboardCharacteristicRx,
    ClipboardCharacteristicTx,
    ClipboardCharacteristicCount,
};

static const BleGattCharacteristicParams clipboard_characteristics[ClipboardCharacteristicCount] = {
    [ClipboardCharacteristicRx] =
        {.name = "Clipboard RX",
         .data_prop_type = FlipperGattCharacteristicDataFixed,
         .data.fixed.length = CLIPBOARD_VALUE_MAX,
         .uuid.Char_UUID_128 = CLIPBOARD_RX_UUID,
         .uuid_type = UUID_TYPE_128,
         .char_properties = CHAR_PROP_WRITE | CHAR_PROP_WRITE_WITHOUT_RESP,
         .security_permissions = ATTR_PERMISSION_AUTHEN_WRITE,
         .gatt_evt_mask = GATT_NOTIFY_ATTRIBUTE_WRITE,
         .is_variable = CHAR_VALUE_LEN_VARIABLE},
    [ClipboardCharacteristicTx] =
        {.name = "Clipboard TX",
         .data_prop_type = FlipperGattCharacteristicDataCallback,
         .data.callback.fn = clipboard_value_callback,
         .data.callback.context = NULL,
         .uuid.Char_UUID_128 = CLIPBOARD_TX_UUID,
         .uuid_type = UUID_TYPE_128,
         .char_properties = CHAR_PROP_READ | CHAR_PROP_NOTIFY,
         .security_permissions = ATTR_PERMISSION_AUTHEN_READ,
         .gatt_evt_mask = GATT_DONT_NOTIFY_EVENTS,
         .is_variable = CHAR_VALUE_LEN_VARIABLE},
};

struct BleServiceClipboard {
    uint16_t service_handle;
    BleGattCharacteristicInstance characteristics[ClipboardCharacteristicCount];
    GapSvcEventHandler* event_handler;
    SerialServiceEventCallback callback;
    void* context;
};

static BleEventAckStatus clipboard_event_handler(void* event, void* context) {
    BleServiceClipboard* service = context;
    ClipboardHciEventPacket* event_packet =
        (ClipboardHciEventPacket*)(((ClipboardHciUartPacket*)event)->data);
    ClipboardBlecoreEvent* blecore_event = (ClipboardBlecoreEvent*)event_packet->data;

    if(event_packet->event != HCI_VENDOR_SPECIFIC_DEBUG_EVT_CODE ||
       blecore_event->event_code != ACI_GATT_ATTRIBUTE_MODIFIED_EVENT_CODE) {
        return BleEventNotAck;
    }

    aci_gatt_attribute_modified_event_rp0* modified =
        (aci_gatt_attribute_modified_event_rp0*)blecore_event->data;
    if(modified->Attr_Handle == service->characteristics[ClipboardCharacteristicRx].handle + 1) {
        if(service->callback) {
            SerialServiceEvent clipboard_event = {
                .event = SerialServiceEventTypeDataReceived,
                .data =
                    {
                        .buffer = modified->Attr_Data,
                        .size = modified->Attr_Data_Length,
                    },
            };
            service->callback(clipboard_event, service->context);
        }
        return BleEventAckFlowEnable;
    }

    if(modified->Attr_Handle == service->characteristics[ClipboardCharacteristicTx].handle + 2) {
        return BleEventAckFlowEnable;
    }
    return BleEventNotAck;
}

BleServiceClipboard* ble_svc_clipboard_start(void) {
    BleServiceClipboard* service = malloc(sizeof(BleServiceClipboard));
    memset(service, 0, sizeof(BleServiceClipboard));
    service->event_handler =
        ble_event_dispatcher_register_svc_handler(clipboard_event_handler, service);

    if(!ble_gatt_service_add(
           UUID_TYPE_128, &clipboard_service_uuid, PRIMARY_SERVICE, 6, &service->service_handle)) {
        ble_event_dispatcher_unregister_svc_handler(service->event_handler);
        free(service);
        return NULL;
    }

    for(size_t i = 0; i < ClipboardCharacteristicCount; i++) {
        ble_gatt_characteristic_init(
            service->service_handle, &clipboard_characteristics[i], &service->characteristics[i]);
    }
    return service;
}

void ble_svc_clipboard_stop(BleServiceClipboard* service) {
    furi_check(service);
    ble_event_dispatcher_unregister_svc_handler(service->event_handler);
    for(size_t i = 0; i < ClipboardCharacteristicCount; i++) {
        ble_gatt_characteristic_delete(service->service_handle, &service->characteristics[i]);
    }
    ble_gatt_service_delete(service->service_handle);
    free(service);
}

void ble_svc_clipboard_set_callback(
    BleServiceClipboard* service,
    SerialServiceEventCallback callback,
    void* context) {
    furi_check(service);
    service->callback = callback;
    service->context = context;
}

bool ble_svc_clipboard_tx(BleServiceClipboard* service, const uint8_t* data, uint16_t size) {
    furi_check(service);
    if(size > CLIPBOARD_VALUE_MAX) return false;
    ClipboardValue value = {.data = data, .size = size};
    // Firmware 1.5.1's wrapper returns true on error, unlike the service API.
    return !ble_gatt_characteristic_update(
        service->service_handle, &service->characteristics[ClipboardCharacteristicTx], &value);
}
