/**
 * Nordic UART Service profile — implementation.
 *
 * Structure mirrors Flipper's upstream ble_profile_serial (MIT-licensed):
 * we register a custom primary service with RX (write) + TX (notify)
 * characteristics, hook the event dispatcher to receive writes, and use
 * ble_gatt_characteristic_update() to push notifications back.
 *
 * One SDK wart: the FAP API exports ble_gatt_*, ble_event_dispatcher_*,
 * and profile template hooks, but NOT the ST BlueNRG packet types needed
 * to parse events (hci_uart_pckt / hci_event_pckt / evt_blecore_aci) or
 * the ACI vendor sub-event code for attribute-modified.  Those are
 * wire-format ABI structs from the ST WPAN stack; we redeclare them
 * locally below.  If ST changes them this file breaks (highly unlikely
 * — these have been stable for years).
 */

#include "nus_profile.h"
#include "app_settings.h"

#include <furi.h>
#include <furi_hal_version.h>
#include <furi_ble/event_dispatcher.h>
#include <furi_ble/gatt.h>
#include <gap.h>

#include <ble/core/ble_defs.h>
#include <ble/core/ble_std.h>
#include <ble/core/auto/ble_types.h>
#include <compiler.h>

#define TAG "NusProfile"

/* ── ST BlueNRG event wrappers (not in FAP SDK; redeclared locally) ── */

typedef __PACKED_STRUCT {
    uint8_t type;
    uint8_t data[1];
}
hci_uart_pckt_local;

typedef __PACKED_STRUCT {
    uint8_t evt;
    uint8_t plen;
    uint8_t data[1];
}
hci_event_pckt_local;

typedef __PACKED_STRUCT {
    uint16_t ecode;
    uint8_t data[1];
}
evt_blecore_aci_local;

#ifndef ACI_GATT_ATTRIBUTE_MODIFIED_VSEVT_CODE
#define ACI_GATT_ATTRIBUTE_MODIFIED_VSEVT_CODE 0x0C01
#endif

/* ── NUS UUIDs (little-endian 128-bit for ST stack) ─────────────── */
/* Base: 6e400001-b5a3-f393-e0a9-e50e24dcca9e */

static const uint8_t NUS_SVC_UUID[16] = {
    0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0,
    0x93, 0xf3, 0xa3, 0xb5, 0x01, 0x00, 0x40, 0x6e};

static const uint8_t NUS_RX_CHAR_UUID[16] = {
    0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0,
    0x93, 0xf3, 0xa3, 0xb5, 0x02, 0x00, 0x40, 0x6e};

static const uint8_t NUS_TX_CHAR_UUID[16] = {
    0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0,
    0x93, 0xf3, 0xa3, 0xb5, 0x03, 0x00, 0x40, 0x6e};

/* ── characteristic descriptors ─────────────────────────────────── */

typedef enum {
    NusCharRx = 0,
    NusCharTx,
    NusCharCount,
} NusCharId;

/* Forward decl — defined below the profile state struct. */
static bool nus_tx_read_cb(const void* context, const uint8_t** data, uint16_t* data_len);

/* Security: we *advertise* pairing support (bonding_mode=true below) so
 * a client can opt in, but the characteristics themselves don't demand
 * authentication — forcing AUTHEN here turned the first non-ack write
 * into a pairing request that some centrals (incl. macOS CoreBluetooth
 * in non-interactive mode) silently abandoned, dropping the link.
 * Anthropic's spec explicitly allows unencrypted operation; upgrading to
 * AUTHEN for bonded transport-encrypt is a separate task. */
#define NUS_CHAR_SEC ATTR_PERMISSION_NONE

static const BleGattCharacteristicParams nus_chars[NusCharCount] = {
    [NusCharRx] =
        {.name = "NUS RX",
         .data_prop_type = FlipperGattCharacteristicDataFixed,
         .data.fixed.length = NUS_PROFILE_RX_MAX_LEN,
         .uuid_type = UUID_TYPE_128,
         .char_properties = CHAR_PROP_WRITE_WITHOUT_RESP | CHAR_PROP_WRITE,
         .security_permissions = NUS_CHAR_SEC,
         .gatt_evt_mask = GATT_NOTIFY_ATTRIBUTE_WRITE,
         .is_variable = CHAR_VALUE_LEN_VARIABLE},
    [NusCharTx] = {
        .name = "NUS TX",
        /* Callback-based data so every notify carries the exact payload
         * size (Fixed would pad to data.fixed.length bytes on every
         * update — unacceptable for our variable-length JSON lines). */
        .data_prop_type = FlipperGattCharacteristicDataCallback,
        .data.callback.fn = nus_tx_read_cb,
        .data.callback.context = NULL,
        .uuid_type = UUID_TYPE_128,
        .char_properties = CHAR_PROP_READ | CHAR_PROP_NOTIFY,
        .security_permissions = NUS_CHAR_SEC,
        .gatt_evt_mask = GATT_DONT_NOTIFY_EVENTS,
        .is_variable = CHAR_VALUE_LEN_VARIABLE}};

/* ── profile state ──────────────────────────────────────────────── */

typedef struct {
    FuriHalBleProfileBase base;

    uint16_t svc_handle;
    BleGattCharacteristicInstance chars[NusCharCount];
    GapSvcEventHandler* event_handler;

    NusProfileRxCallback rx_cb;
    void* rx_ctx;
} BleProfileNus;

_Static_assert(offsetof(BleProfileNus, base) == 0, "Wrong layout");

/* Callback-backed TX: on each update the caller passes a NusTxPayload via
 * the source arg of ble_gatt_characteristic_update; the stack forwards it
 * to nus_tx_read_cb as `context`, which returns the exact bytes+length so
 * the emitted notification is not padded. */
typedef struct {
    const uint8_t* data;
    uint16_t len;
} NusTxPayload;

static bool nus_tx_read_cb(const void* context, const uint8_t** data, uint16_t* data_len) {
    /* During ble_gatt_characteristic_init the stack queries for the max
     * attribute size by passing data=NULL (the pointer itself, not *data).
     * In that case we must ONLY touch data_len; writing to *data would
     * dereference a NULL pointer. */
    if(!context || !data) {
        if(data_len) *data_len = NUS_PROFILE_RX_MAX_LEN;
        return false;
    }
    const NusTxPayload* p = context;
    *data = p->data;
    *data_len = p->len;
    return false; /* stack does NOT take ownership */
}

/* Fill UUID fields that must be set at runtime (BleGattCharacteristicParams
 * stores UUID by value, so copy before init). */
static void nus_fill_char_uuids(BleGattCharacteristicParams out[NusCharCount]) {
    memcpy(out, nus_chars, sizeof(nus_chars));
    memcpy(out[NusCharRx].uuid.Char_UUID_128, NUS_RX_CHAR_UUID, 16);
    memcpy(out[NusCharTx].uuid.Char_UUID_128, NUS_TX_CHAR_UUID, 16);
}

/* ── event dispatcher callback (BLE thread) ─────────────────────── */

static BleEventAckStatus nus_event_handler(void* event, void* context) {
    BleProfileNus* nus = context;
    BleEventAckStatus ret = BleEventNotAck;

    hci_event_pckt_local* pckt = (hci_event_pckt_local*)(((hci_uart_pckt_local*)event)->data);
    if(pckt->evt != HCI_VENDOR_SPECIFIC_DEBUG_EVT_CODE) return ret;

    evt_blecore_aci_local* core_evt = (evt_blecore_aci_local*)pckt->data;
    if(core_evt->ecode != ACI_GATT_ATTRIBUTE_MODIFIED_VSEVT_CODE) return ret;

    aci_gatt_attribute_modified_event_rp0* mod =
        (aci_gatt_attribute_modified_event_rp0*)core_evt->data;

    /* RX char value handle = char declaration handle + 1.
     * (descriptor/CCCD is + 2, only present on TX.) */
    if(mod->Attr_Handle == nus->chars[NusCharRx].handle + 1) {
        if(nus->rx_cb) {
            nus->rx_cb(mod->Attr_Data, mod->Attr_Data_Length, nus->rx_ctx);
        }
        ret = BleEventAckFlowEnable;
    }
    return ret;
}

/* ── template hooks ─────────────────────────────────────────────── */

static FuriHalBleProfileBase* nus_profile_start(FuriHalBleProfileParams params) {
    UNUSED(params);

    BleProfileNus* nus = malloc(sizeof(BleProfileNus));
    memset(nus, 0, sizeof(*nus));
    nus->base.config = ble_profile_nus;

    nus->event_handler = ble_event_dispatcher_register_svc_handler(nus_event_handler, nus);

    Service_UUID_t svc_uuid = {0};
    memcpy(svc_uuid.Service_UUID_128, NUS_SVC_UUID, 16);
    if(!ble_gatt_service_add(UUID_TYPE_128, &svc_uuid, PRIMARY_SERVICE, 8, &nus->svc_handle)) {
        FURI_LOG_E(TAG, "ble_gatt_service_add failed");
        ble_event_dispatcher_unregister_svc_handler(nus->event_handler);
        free(nus);
        return NULL;
    }

    BleGattCharacteristicParams chars[NusCharCount];
    nus_fill_char_uuids(chars);
    for(uint8_t i = 0; i < NusCharCount; i++) {
        ble_gatt_characteristic_init(nus->svc_handle, &chars[i], &nus->chars[i]);
    }

    FURI_LOG_I(TAG, "NUS profile started (svc=%u)", nus->svc_handle);
    return &nus->base;
}

static void nus_profile_stop(FuriHalBleProfileBase* profile) {
    furi_check(profile);
    furi_check(profile->config == ble_profile_nus);

    BleProfileNus* nus = (BleProfileNus*)profile;
    ble_event_dispatcher_unregister_svc_handler(nus->event_handler);
    for(uint8_t i = 0; i < NusCharCount; i++) {
        ble_gatt_characteristic_delete(nus->svc_handle, &nus->chars[i]);
    }
    ble_gatt_service_delete(nus->svc_handle);
    free(nus);
}

#define CONN_INTERVAL_MIN 0x06 /* 7.5 ms */
#define CONN_INTERVAL_MAX 0x24 /* 45 ms */

static void nus_profile_get_gap_config(GapConfig* config, FuriHalBleProfileParams params) {
    UNUSED(params);
    furi_check(config);
    memset(config, 0, sizeof(*config));

    /* Adv layout.  The primary adv PDU is 31 B; Flipper's gap.c calls
     * aci_gap_set_discoverable with ADV_IND, which auto-inserts a 3 B
     * Flags AD *and* a 3 B Tx Power AD (empirically verified — the
     * scanner reports kCBAdvDataTxPowerLevel).  That leaves 25 B for
     * our content.  A full 128-bit NUS UUID AD is 18 B on its own, so
     * a "Claude" Local Name AD (8 B) would push the PDU to 32 B and
     * aci_gap_set_discoverable fails with INVALID_LEN_PDU (err 146).
     *
     * Workaround: carry only a 16-bit placeholder UUID in the adv
     * (4 B AD) and leave the real NUS UUID in the GATT service table,
     * where any NUS-speaking central discovers it after connecting.
     * Claude Desktop's picker filters by name prefix ("Claude"), not by
     * service UUID in the adv, so dropping the 128-bit UUID from adv
     * costs nothing at the picker layer. */
    config->adv_service.UUID_Type = UUID_TYPE_16;
    config->adv_service.Service_UUID_16 = 0xFEA0;
    config->appearance_char = 0x8600;
    /* Pairing is currently disabled — the NUS chars are ATTR_PERMISSION_NONE
     * so the link stays unencrypted (Anthropic's spec allows this), and
     * enabling bonding without a 6-digit passkey display UI causes half-
     * finished pairing handshakes that some centrals refuse to retry. */
    config->bonding_mode = false;
    config->pairing_method = GapPairingNone;
    config->conn_param.conn_int_min = CONN_INTERVAL_MIN;
    config->conn_param.conn_int_max = CONN_INTERVAL_MAX;
    config->conn_param.slave_latency = 0;
    config->conn_param.supervisor_timeout = 0;

    /* Present Hardware Buddy mode under a BLE MAC distinct from stock
     * Flipper firmware's (and from our own Bridge mode).  macOS
     * CoreBluetooth keeps a sticky cache mapping MAC → CBPeripheral.name
     * based on whatever name it saw first for that MAC — typically the
     * stock firmware's "flip_XXXXX" GATT Device Name.  Claude Desktop's
     * picker filters on CBPeripheral.name, so without a MAC difference
     * the cached stock-firmware name shadows our advertised "Claude …"
     * and the picker silently drops us.  XOR-ing one bit of the factory
     * MAC keeps each Flipper's NUS-mode MAC unique-per-device, stable
     * across app restarts, and reversible. */
    memcpy(config->mac_address, furi_hal_version_get_ble_mac(), sizeof(config->mac_address));
    config->mac_address[0] ^= 0x01;

    /* Name — must start with "Claude" or Claude Desktop's picker drops
     * us.  Default "Claude Flipper"; the `cmd: name` protocol message
     * can override it, but only if the new name also starts with
     * "Claude" (otherwise we silently fall back to default rather than
     * making the device un-pickable). */
    char user_name[APP_SETTINGS_DEVNAME_MAX] = {0};
    const char* visible = "Claude Flipper";
    if(app_settings_get_device_name(user_name, sizeof(user_name)) && user_name[0]
       && strncmp(user_name, "Claude", 6) == 0) {
        visible = user_name;
    }
    config->adv_name[0] = 0x09; /* AD_TYPE_COMPLETE_LOCAL_NAME */
    strlcpy(config->adv_name + 1, visible, FURI_HAL_VERSION_DEVICE_NAME_LENGTH - 1);
}

static const FuriHalBleProfileTemplate profile_callbacks = {
    .start = nus_profile_start,
    .stop = nus_profile_stop,
    .get_gap_config = nus_profile_get_gap_config,
};

const FuriHalBleProfileTemplate* const ble_profile_nus = &profile_callbacks;

/* ── public API ─────────────────────────────────────────────────── */

bool nus_profile_tx(FuriHalBleProfileBase* profile, const uint8_t* data, uint16_t size) {
    furi_check(profile && profile->config == ble_profile_nus);
    furi_check(data);

    BleProfileNus* nus = (BleProfileNus*)profile;

    uint16_t sent = 0;
    while(sent < size) {
        uint16_t chunk = size - sent;
        if(chunk > NUS_PROFILE_RX_MAX_LEN) chunk = NUS_PROFILE_RX_MAX_LEN;
        NusTxPayload payload = {.data = data + sent, .len = chunk};
        if(!ble_gatt_characteristic_update(nus->svc_handle, &nus->chars[NusCharTx], &payload)) {
            FURI_LOG_E(TAG, "TX update failed at offset %u", sent);
            return false;
        }
        sent += chunk;
    }
    return true;
}

void nus_profile_set_rx_callback(
    FuriHalBleProfileBase* profile,
    NusProfileRxCallback callback,
    void* context) {
    furi_check(profile && profile->config == ble_profile_nus);
    BleProfileNus* nus = (BleProfileNus*)profile;
    nus->rx_cb = callback;
    nus->rx_ctx = context;
}
