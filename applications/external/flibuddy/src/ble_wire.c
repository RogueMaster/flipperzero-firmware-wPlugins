#include <string.h>

#include "ble_wire.h"

/* AD structures: a length byte, a type byte, the data */
#define AD_HEADER        2
#define AD_FLAGS_SIZE    (AD_HEADER + 1)
#define AD_TX_POWER_SIZE (AD_HEADER + 1)
#define AD_UUID_128_SIZE (AD_HEADER + 16)
#define UNNAMED_FLIPPER  "Flipper"

#define HCI_DISCONNECTION_COMPLETE_EVT_CODE 0x05
#define HCI_VENDOR_SPECIFIC_DEBUG_EVT_CODE  0xFF

#define ACI_GATT_ATTRIBUTE_MODIFIED_VSEVT_CODE 0x0C01
#define ACI_ATT_EXCHANGE_MTU_RESP_VSEVT_CODE   0x0C03

#define CCCD_NOTIFY 0x01

#define MAC_BYTE_CHANGED 2
#define MAC_XOR          0x5B

size_t ble_wire_adv_size(size_t name_len) {
    size_t size = AD_FLAGS_SIZE + AD_TX_POWER_SIZE + AD_UUID_128_SIZE;
    return name_len > 0 ? size + AD_HEADER + name_len : size;
}

BleWireLink ble_wire_link(bool opened, bool advertising, bool subscribed) {
    if(!opened) return BleWireLinkOff;
    if(subscribed) return BleWireLinkConnected;
    return advertising ? BleWireLinkAdvertising : BleWireLinkOff;
}

static const char* name_or_fallback(const char* flipper_name) {
    return flipper_name && flipper_name[0] ? flipper_name : UNNAMED_FLIPPER;
}

void ble_wire_gap_name(char* out, const char* flipper_name) {
    const char* name = name_or_fallback(flipper_name);
    size_t len = strlen(name);
    if(len > BLE_WIRE_GAP_NAME_CHARS) len = BLE_WIRE_GAP_NAME_CHARS;
    out[0] = '\0';
    memcpy(out + 1, name, len);
    out[1 + len] = '\0';
}

size_t ble_wire_mfg_data(uint8_t* out, const char* flipper_name) {
    const char* name = name_or_fallback(flipper_name);
    size_t len = strlen(name);
    if(len > BLE_WIRE_MFG_DATA_MAX - 2) len = BLE_WIRE_MFG_DATA_MAX - 2;
    out[0] = BLE_WIRE_COMPANY_ID & 0xFF;
    out[1] = BLE_WIRE_COMPANY_ID >> 8;
    memcpy(out + 2, name, len);
    return 2 + len;
}

size_t ble_wire_scan_response_size(size_t mfg_len) {
    return AD_HEADER + mfg_len;
}

void ble_wire_mac(uint8_t out[6], const uint8_t flipper_mac[6]) {
    memcpy(out, flipper_mac, 6);
    out[MAC_BYTE_CHANGED] ^= MAC_XOR;
}

static uint16_t le16(const uint8_t* p) {
    return (uint16_t)(p[0] | (p[1] << 8));
}

/* Attribute modified: connection handle, attribute handle, offset, data
 * length (16 bits each), data. */
static BleWireEvent
    attribute_modified(const uint8_t* params, size_t len, const BleWireHandles* h) {
    BleWireEvent e = {.type = BleWireEventOther};
    if(len < 8) return e;
    uint16_t handle = le16(params + 2);
    size_t data_len = le16(params + 6);
    if(data_len > len - 8) return e;
    const uint8_t* data = params + 8;
    if(handle == h->rx_value) {
        e.type = BleWireEventRx;
        e.data = data;
        e.len = data_len;
    } else if(handle == h->tx_cccd && data_len > 0) {
        e.type = BleWireEventSubscribe;
        e.subscribed = (data[0] & CCCD_NOTIFY) != 0;
    }
    return e;
}

static BleWireEvent vendor_event(const uint8_t* params, size_t len, const BleWireHandles* h) {
    BleWireEvent e = {.type = BleWireEventOther};
    if(len < 2) return e;
    uint16_t code = le16(params);
    params += 2;
    len -= 2;
    switch(code) {
    case ACI_GATT_ATTRIBUTE_MODIFIED_VSEVT_CODE:
        return attribute_modified(params, len, h);
    case ACI_ATT_EXCHANGE_MTU_RESP_VSEVT_CODE:
        /* connection handle, server RX MTU */
        if(len >= 4) {
            e.type = BleWireEventMtu;
            e.mtu = le16(params + 2);
        }
        return e;
    default:
        return e;
    }
}

BleWireEvent ble_wire_decode(const uint8_t* packet, const BleWireHandles* handles) {
    BleWireEvent e = {.type = BleWireEventOther};
    uint8_t code = packet[1];
    size_t len = packet[2];
    const uint8_t* params = packet + 3;
    if(code == HCI_VENDOR_SPECIFIC_DEBUG_EVT_CODE) return vendor_event(params, len, handles);
    /* status, connection handle, reason; a non-zero status did not disconnect */
    if(code == HCI_DISCONNECTION_COMPLETE_EVT_CODE && len >= 4 && params[0] == 0) {
        e.type = BleWireEventDisconnected;
    }
    return e;
}
