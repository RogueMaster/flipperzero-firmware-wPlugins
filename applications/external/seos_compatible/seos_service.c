#include "seos_service.h"
#include "headers/app_common.h"
#include <furi_ble/event_dispatcher.h>
#include <furi_ble/gatt.h>

#include "headers/ble_vs_codes.h"
#include "headers/ble_gatt_aci.h"

#include <furi.h>

#include "seos_service_uuid.inc"
#include <stdint.h>

#define TAG "BtSeosSvc"

static uint8_t select[] =
    {0xc0, 0x00, 0xa4, 0x04, 0x00, 0x0a, 0xa0, 0x00, 0x00, 0x04, 0x40, 0x00, 0x01, 0x01, 0x00, 0x01};

typedef enum {
    SeosSvcGattCharacteristicRxTx = 0,
    SeosSvcGattCharacteristicCount,
} SeosSvcGattCharacteristicId;

typedef struct {
    const void* data_ptr;
    uint16_t data_len;
} SeosSvcDataWrapper;

static bool
    ble_svc_seos_data_callback(const void* context, const uint8_t** data, uint16_t* data_len) {
    FURI_LOG_D(TAG, "ble_svc_seos_data_callback");
    const SeosSvcDataWrapper* report_data = context;
    if(data) {
        *data = report_data->data_ptr;
        *data_len = report_data->data_len;
    } else {
        *data_len = BLE_SVC_SEOS_DATA_LEN_MAX;
    }
    return false;
}

static BleGattCharacteristicParams ble_svc_seos_chars[SeosSvcGattCharacteristicCount] = {
    [SeosSvcGattCharacteristicRxTx] =
        {.name = "SEOS",
         .data_prop_type = FlipperGattCharacteristicDataCallback,
         .data.callback.fn = ble_svc_seos_data_callback,
         .data.callback.context = NULL,
         .uuid.Char_UUID_128 = BLE_SVC_SEOS_READER_CHAR_UUID, // changed before init
         .uuid_type = UUID_TYPE_128,
         .char_properties = CHAR_PROP_WRITE_WITHOUT_RESP | CHAR_PROP_NOTIFY,
         .security_permissions = ATTR_PERMISSION_NONE,
         .gatt_evt_mask = GATT_NOTIFY_ATTRIBUTE_WRITE,
         .is_variable = CHAR_VALUE_LEN_VARIABLE},
};

struct BleServiceSeos {
    uint16_t svc_handle;
    BleGattCharacteristicInstance chars[SeosSvcGattCharacteristicCount];
    FuriMutex* buff_size_mtx;
    uint32_t buff_size;
    uint16_t bytes_ready_to_receive;
    SeosServiceEventCallback callback;
    void* context;
    GapSvcEventHandler* event_handler;
    FlowMode mode;
};

static BleEventAckStatus ble_svc_seos_event_handler(void* event, void* context) {
    BleServiceSeos* seos_svc = (BleServiceSeos*)context;
    BleEventAckStatus ret = BleEventNotAck;
    hci_event_pckt* event_pckt = (hci_event_pckt*)(((hci_uart_pckt*)event)->data);
    evt_blecore_aci* blecore_evt = (evt_blecore_aci*)event_pckt->data;
    aci_gatt_attribute_modified_event_rp0* attribute_modified;

    if(event_pckt->evt == HCI_LE_META_EVT_CODE) {
    } else if(event_pckt->evt == HCI_DISCONNECTION_COMPLETE_EVT_CODE) {
    } else if(event_pckt->evt == HCI_VENDOR_SPECIFIC_DEBUG_EVT_CODE) {
        if(blecore_evt->ecode == ACI_GATT_ATTRIBUTE_MODIFIED_VSEVT_CODE) {
            attribute_modified = (aci_gatt_attribute_modified_event_rp0*)blecore_evt->data;
            if(attribute_modified->Attr_Handle ==
               seos_svc->chars[SeosSvcGattCharacteristicRxTx].handle + 2) {
                // Descriptor handle
                ret = BleEventAckFlowEnable;
                FURI_LOG_D(TAG, "Descriptor event %d bytes", attribute_modified->Attr_Data_Length);

                if(attribute_modified->Attr_Data_Length == 2) {
                    uint16_t* value = (uint16_t*)attribute_modified->Attr_Data;
                    if(*value == 1) { // ENABLE_NOTIFICATION_VALUE)
                        if(seos_svc->mode == FLOW_READER) {
                            SeosSvcDataWrapper report_data = {
                                .data_ptr = select, .data_len = sizeof(select)};

                            ble_gatt_characteristic_update(
                                seos_svc->svc_handle,
                                &seos_svc->chars[SeosSvcGattCharacteristicRxTx],
                                &report_data);
                        } else if(seos_svc->mode == FLOW_CRED) {
                            FURI_LOG_D(TAG, "No action for FLOW_CRED after subscribe");
                        }
                    }
                }
            } else if(
                attribute_modified->Attr_Handle ==
                seos_svc->chars[SeosSvcGattCharacteristicRxTx].handle + 1) {
                FURI_LOG_D(TAG, "Received %d bytes", attribute_modified->Attr_Data_Length);
                if(seos_svc->callback) {
                    furi_check(
                        furi_mutex_acquire(seos_svc->buff_size_mtx, FuriWaitForever) ==
                        FuriStatusOk);
                    SeosServiceEvent event = {
                        .event = SeosServiceEventTypeDataReceived,
                        .data = {
                            .buffer = attribute_modified->Attr_Data,
                            .size = attribute_modified->Attr_Data_Length,
                        }};
                    uint32_t buff_free_size = seos_svc->callback(event, seos_svc->context);
                    FURI_LOG_D(TAG, "Available buff size: %ld", buff_free_size);
                    furi_check(furi_mutex_release(seos_svc->buff_size_mtx) == FuriStatusOk);
                } else {
                    FURI_LOG_W(TAG, "No seos_cvs->callback defined");
                }
                ret = BleEventAckFlowEnable;
            }
        } else if(blecore_evt->ecode == ACI_GATT_SERVER_CONFIRMATION_VSEVT_CODE) {
            FURI_LOG_T(TAG, "Ack received");
            if(seos_svc->callback) {
                SeosServiceEvent event = {
                    .event = SeosServiceEventTypeDataSent,
                };
                seos_svc->callback(event, seos_svc->context);
            }
            ret = BleEventAckFlowEnable;
        } else {
            FURI_LOG_D(
                TAG,
                "ble_svc_seos_event_handler unhandled blecore_evt->ecode %d",
                blecore_evt->ecode);
        }
    } else {
        FURI_LOG_D(
            TAG, "ble_svc_seos_event_handler unhandled event_pckt->evt %d", event_pckt->evt);
    }
    return ret;
}

BleServiceSeos* ble_svc_seos_start(FlowMode mode) {
    BleServiceSeos* seos_svc = malloc(sizeof(BleServiceSeos));
    seos_svc->mode = mode;

    seos_svc->event_handler =
        ble_event_dispatcher_register_svc_handler(ble_svc_seos_event_handler, seos_svc);

    if(mode == FLOW_READER) {
        if(!ble_gatt_service_add(
               UUID_TYPE_128, &reader_service_uuid, PRIMARY_SERVICE, 12, &seos_svc->svc_handle)) {
            free(seos_svc);
            return NULL;
        }
    } else if(mode == FLOW_CRED) {
        if(!ble_gatt_service_add(
               UUID_TYPE_128, &cred_service_uuid, PRIMARY_SERVICE, 12, &seos_svc->svc_handle)) {
            free(seos_svc);
            return NULL;
        }
    }

    for(uint8_t i = 0; i < SeosSvcGattCharacteristicCount; i++) {
        if(i == SeosSvcGattCharacteristicRxTx) {
            if(mode == FLOW_READER) {
                uint8_t uuid[] = BLE_SVC_SEOS_READER_CHAR_UUID;
                memcpy(
                    ble_svc_seos_chars[SeosSvcGattCharacteristicRxTx].uuid.Char_UUID_128,
                    uuid,
                    sizeof(uuid));
            } else if(mode == FLOW_CRED) {
                uint8_t uuid[] = BLE_SVC_SEOS_CRED_CHAR_UUID;
                memcpy(
                    ble_svc_seos_chars[SeosSvcGattCharacteristicRxTx].uuid.Char_UUID_128,
                    uuid,
                    sizeof(uuid));
            }
            ble_gatt_characteristic_init(
                seos_svc->svc_handle, &ble_svc_seos_chars[i], &seos_svc->chars[i]);
        }
    }

    seos_svc->buff_size_mtx = furi_mutex_alloc(FuriMutexTypeNormal);

    return seos_svc;
}

void ble_svc_seos_set_callbacks(
    BleServiceSeos* seos_svc,
    uint16_t buff_size,
    SeosServiceEventCallback callback,
    void* context) {
    furi_check(seos_svc);
    seos_svc->callback = callback;
    seos_svc->context = context;
    seos_svc->buff_size = buff_size;
    seos_svc->bytes_ready_to_receive = buff_size;
}

void ble_svc_seos_stop(BleServiceSeos* seos_svc) {
    furi_check(seos_svc);

    ble_event_dispatcher_unregister_svc_handler(seos_svc->event_handler);

    for(uint8_t i = 0; i < SeosSvcGattCharacteristicCount; i++) {
        ble_gatt_characteristic_delete(seos_svc->svc_handle, &seos_svc->chars[i]);
    }
    ble_gatt_service_delete(seos_svc->svc_handle);
    furi_mutex_free(seos_svc->buff_size_mtx);
    free(seos_svc);
}

bool ble_svc_seos_update_tx(BleServiceSeos* seos_svc, uint8_t* data, uint16_t data_len) {
    if(data_len > BLE_SVC_SEOS_DATA_LEN_MAX) {
        return false;
    }

    SeosSvcDataWrapper report_data = {.data_ptr = data, .data_len = data_len};

    ble_gatt_characteristic_update(
        seos_svc->svc_handle, &seos_svc->chars[SeosSvcGattCharacteristicRxTx], &report_data);

    return true;
}
