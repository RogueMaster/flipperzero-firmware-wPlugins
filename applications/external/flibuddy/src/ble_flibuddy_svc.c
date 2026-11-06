#include <furi.h>
#include <furi_hal_version.h>
#include <ble/core/ble_defs.h>
#include <furi_ble/event_dispatcher.h>
#include <furi_ble/gatt.h>

#include "ble_lines.h"
#include "ble_flibuddy_svc.h"
#include "ble_sdk_pin.h"
#include "ble_wire.h"

#define TAG "FliBuddyBle"

/* the service, RX (declaration, value), TX (declaration, value, CCCD) */
#define SVC_ATTRIBUTE_RECORDS 6

#if FLIBUDDY_BLE_REQUIRE_ENCRYPTION
#define SVC_PERMISSIONS (ATTR_PERMISSION_ENCRY_READ | ATTR_PERMISSION_ENCRY_WRITE)
#else
#define SVC_PERMISSIONS ATTR_PERMISSION_NONE
#endif

/* 7.5 ms to 45 ms, as the serial profile; the watch asks for 30 ms or more */
#define CONNECTION_INTERVAL_MIN 0x06
#define CONNECTION_INTERVAL_MAX 0x24

typedef enum {
    CharRx,
    CharTx,
    CharCount,
} FliBuddyChar;

typedef struct {
    const uint8_t* data;
    uint16_t len;
} TxValue;

/* TX has a variable length: the SDK takes it from here, the maximum when
 * `data` is NULL (at creation), else the value being notified. */
static bool tx_value(const void* context, const uint8_t** data, uint16_t* len) {
    if(data == NULL) {
        *len = BLE_LINES_CHUNK_MAX;
        return false;
    }
    const TxValue* value = context;
    *data = value->data;
    *len = value->len;
    return false; /* not ours to free */
}

static const BleGattCharacteristicParams chars[CharCount] = {
    [CharRx] =
        {.name = "RX",
         .data_prop_type = FlipperGattCharacteristicDataFixed,
         .data.fixed.length = BLE_LINES_CHUNK_MAX,
         .uuid.Char_UUID_128 = BLE_WIRE_RX_UUID,
         .uuid_type = UUID_TYPE_128,
         .char_properties = CHAR_PROP_WRITE | CHAR_PROP_WRITE_WITHOUT_RESP,
         .security_permissions = SVC_PERMISSIONS,
         .gatt_evt_mask = GATT_NOTIFY_ATTRIBUTE_WRITE,
         .is_variable = CHAR_VALUE_LEN_VARIABLE},
    [CharTx] =
        {.name = "TX",
         .data_prop_type = FlipperGattCharacteristicDataCallback,
         .data.callback.fn = tx_value,
         .uuid.Char_UUID_128 = BLE_WIRE_TX_UUID,
         .uuid_type = UUID_TYPE_128,
         .char_properties = CHAR_PROP_NOTIFY,
         .security_permissions = SVC_PERMISSIONS,
         /* the host's CCCD write must reach on_event as an attribute
          * modified event: the subscription is how a host connects */
         .gatt_evt_mask = GATT_NOTIFY_ATTRIBUTE_WRITE,
         .is_variable = CHAR_VALUE_LEN_VARIABLE},
};

static const Service_UUID_t service_uuid = {.Service_UUID_128 = BLE_WIRE_SERVICE_UUID};

typedef struct {
    FuriHalBleProfileBase base;
    uint16_t svc_handle;
    BleGattCharacteristicInstance chars[CharCount];
    GapSvcEventHandler* event_handler;
    const FliBuddyBleCallbacks* callbacks;
} FliBuddyBleProfile;
_Static_assert(offsetof(FliBuddyBleProfile, base) == 0, "the base comes first");

/* One instance, never freed: the Bt service still reads the stopped
 * profile's config (furi_hal_bt_check_profile_type) while it switches to
 * the next one. */
static FliBuddyBleProfile instance;

/* BLE event thread. Writes to the service's attributes are handled here;
 * everything else also goes on to the other handlers and GAP. */
static BleEventAckStatus on_event(void* event, void* context) {
    FliBuddyBleProfile* profile = context;
    const FliBuddyBleCallbacks* cb = profile->callbacks;
    BleWireHandles handles = {
        .rx_value = profile->chars[CharRx].handle + 1,
        .tx_cccd = profile->chars[CharTx].handle + 2,
    };
    BleWireEvent e = ble_wire_decode(event, &handles);
    switch(e.type) {
    case BleWireEventRx:
        cb->on_rx(e.data, e.len, cb->ctx);
        return BleEventAckFlowEnable;
    case BleWireEventSubscribe:
        cb->on_subscribe(e.subscribed, cb->ctx);
        return BleEventAckFlowEnable;
    case BleWireEventMtu:
        cb->on_mtu(e.mtu, cb->ctx);
        break;
    case BleWireEventDisconnected:
        cb->on_disconnect(cb->ctx);
        break;
    case BleWireEventOther:
        break;
    }
    return BleEventNotAck;
}

static FuriHalBleProfileBase* profile_start(FuriHalBleProfileParams params) {
    furi_check(params);
    FliBuddyBleProfile* profile = &instance;
    memset(profile, 0, sizeof(FliBuddyBleProfile));
    profile->base.config = flibuddy_ble_profile;
    profile->callbacks = params;

    if(!ble_gatt_service_add(
           UUID_TYPE_128,
           &service_uuid,
           PRIMARY_SERVICE,
           SVC_ATTRIBUTE_RECORDS,
           &profile->svc_handle)) {
        FURI_LOG_E(TAG, "service not added");
        return NULL;
    }
    for(size_t i = 0; i < CharCount; i++) {
        ble_gatt_characteristic_init(profile->svc_handle, &chars[i], &profile->chars[i]);
    }
    profile->event_handler = ble_event_dispatcher_register_svc_handler(on_event, profile);
    return &profile->base;
}

static void profile_stop(FuriHalBleProfileBase* base) {
    furi_check(base && base->config == flibuddy_ble_profile);
    FliBuddyBleProfile* profile = (FliBuddyBleProfile*)base;
    ble_event_dispatcher_unregister_svc_handler(profile->event_handler);
    for(size_t i = 0; i < CharCount; i++) {
        ble_gatt_characteristic_delete(profile->svc_handle, &profile->chars[i]);
    }
    ble_gatt_service_delete(profile->svc_handle);
}

static const GapConfig gap_template = {
    .adv_service =
        {
            .UUID_Type = UUID_TYPE_128,
            .Service_UUID_128 = BLE_WIRE_SERVICE_UUID,
        },
    .appearance_char = 0x8600, /* as the serial profile */
    .bonding_mode = false,
    .pairing_method = GapPairingNone,
    .conn_param =
        {
            .conn_int_min = CONNECTION_INTERVAL_MIN,
            .conn_int_max = CONNECTION_INTERVAL_MAX,
            .slave_latency = 0,
            .supervisor_timeout = 0,
        },
};

static void profile_gap_config(GapConfig* config, FuriHalBleProfileParams params) {
    UNUSED(params);
    furi_check(config);
    memcpy(config, &gap_template, sizeof(GapConfig));
    ble_wire_mac(config->mac_address, furi_hal_version_get_ble_mac());
    /* no name in the advertisement, which has no room for it (ble_wire.h);
     * the name goes in the GAP device name and the scan response */
    const char* name = furi_hal_version_get_name_ptr();
    _Static_assert(sizeof(config->adv_name) >= BLE_WIRE_GAP_NAME_SIZE, "the name fits");
    ble_wire_gap_name(config->adv_name, name);
    _Static_assert(sizeof(config->mfg_data) >= BLE_WIRE_MFG_DATA_MAX, "the data fits");
    config->mfg_data_len = (uint8_t)ble_wire_mfg_data(config->mfg_data, name);
}

static const FuriHalBleProfileTemplate profile_template = {
    .start = profile_start,
    .stop = profile_stop,
    .get_gap_config = profile_gap_config,
};

const FuriHalBleProfileTemplate* const flibuddy_ble_profile = &profile_template;

bool flibuddy_ble_notify(FuriHalBleProfileBase* base, const uint8_t* data, size_t len) {
    furi_check(base && base->config == flibuddy_ble_profile);
    furi_check(len > 0 && len <= BLE_LINES_CHUNK_MAX);
    FliBuddyBleProfile* profile = (FliBuddyBleProfile*)base;
    TxValue value = {.data = data, .len = (uint16_t)len};
    /* in SDK 1.5.1 this returns true when the update FAILED (gatt.c returns
     * result != BLE_STATUS_SUCCESS), after retrying a busy stack for up to
     * a second. Verified on SDK API FLIBUDDY_VERIFIED_SDK_API only: see
     * ble_sdk_pin.h for what to re-check on a new firmware. */
    return !ble_gatt_characteristic_update(profile->svc_handle, &profile->chars[CharTx], &value);
}
