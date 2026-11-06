#include "minitest.h"

#include <stdint.h>

#include "ble_wire.h"

static const uint8_t service_uuid[] = BLE_WIRE_SERVICE_UUID;
static const uint8_t rx_uuid[] = BLE_WIRE_RX_UUID;
static const uint8_t tx_uuid[] = BLE_WIRE_TX_UUID;

/* The canonical text form of a UUID the stack holds LSB first. */
static const char* uuid_text(const uint8_t* lsb_first) {
    static char text[37];
    size_t at = 0;
    for(int i = 15; i >= 0; i--) {
        if(i == 11 || i == 9 || i == 7 || i == 5) text[at++] = '-';
        snprintf(text + at, sizeof(text) - at, "%02X", lsb_first[i]);
        at += 2;
    }
    return text;
}

static void test_uuids_match_the_gatt_table(void) {
    /* plan, Technical Details; the host has the same in FliBuddyGATT.swift */
    CHECK_STR_EQ(uuid_text(service_uuid), "51C07E84-E37E-4D52-AC6D-B2003116F506");
    CHECK_STR_EQ(uuid_text(rx_uuid), "D250A7B1-D301-43F6-928E-BF2CA5D115BD");
    CHECK_STR_EQ(uuid_text(tx_uuid), "C3AB2366-401D-482C-9002-B0A046C2A4E4");
}

static void test_advertisement_without_a_name_fits_31_bytes(void) {
    /* flags (3) + TX power (3) + the 128-bit UUID AD (18) */
    CHECK_EQ_INT(ble_wire_adv_size(0), 24);
    CHECK(ble_wire_adv_size(0) <= BLE_WIRE_ADV_MAX);
}

static void test_advertisement_with_a_name_does_not_fit(void) {
    /* why the name left: 8 chars made it 34 bytes and the stack refused it
     * (set_discoverable failed 146) */
    CHECK_EQ_INT(ble_wire_adv_size(8), 34);
    CHECK(ble_wire_adv_size(8) > BLE_WIRE_ADV_MAX);
}

static void test_gap_name_advertises_no_name(void) {
    char name[BLE_WIRE_GAP_NAME_SIZE];
    ble_wire_gap_name(name, "Mars");
    /* gap.c advertises strlen(name) bytes as the local name AD */
    CHECK_EQ_INT(strlen(name), 0);
    CHECK(ble_wire_adv_size(strlen(name)) <= BLE_WIRE_ADV_MAX);
}

static void test_gap_name_keeps_the_device_name(void) {
    char name[BLE_WIRE_GAP_NAME_SIZE];
    /* gap.c takes the GAP device name from name + 1 */
    ble_wire_gap_name(name, "Mars");
    CHECK_STR_EQ(name + 1, "Mars");
    ble_wire_gap_name(name, "Abcdefgh");
    CHECK_STR_EQ(name + 1, "Abcdefgh");
    ble_wire_gap_name(name, "Abcdefghi");
    CHECK_STR_EQ(name + 1, "Abcdefgh");
    ble_wire_gap_name(name, "Abcdefghij");
    CHECK_STR_EQ(name + 1, "Abcdefgh");
}

static void test_gap_name_falls_back_when_the_flipper_has_none(void) {
    char name[BLE_WIRE_GAP_NAME_SIZE];
    ble_wire_gap_name(name, NULL);
    CHECK_STR_EQ(name + 1, "Flipper");
    ble_wire_gap_name(name, "");
    CHECK_STR_EQ(name + 1, "Flipper");
}

static void test_mfg_data_is_the_test_company_id_and_the_name(void) {
    uint8_t data[BLE_WIRE_MFG_DATA_MAX];
    size_t len = ble_wire_mfg_data(data, "Mars");
    const uint8_t expected[] = {0xFF, 0xFF, 'M', 'a', 'r', 's'};
    CHECK_EQ_INT(len, sizeof(expected));
    CHECK_MEM_EQ(data, expected, sizeof(expected));
}

static void test_mfg_data_falls_back_when_the_flipper_has_none(void) {
    uint8_t data[BLE_WIRE_MFG_DATA_MAX];
    const uint8_t expected[] = {0xFF, 0xFF, 'F', 'l', 'i', 'p', 'p', 'e', 'r'};
    size_t len = ble_wire_mfg_data(data, NULL);
    CHECK_EQ_INT(len, sizeof(expected));
    CHECK_MEM_EQ(data, expected, sizeof(expected));
    len = ble_wire_mfg_data(data, "");
    CHECK_EQ_INT(len, sizeof(expected));
    CHECK_MEM_EQ(data, expected, sizeof(expected));
}

static void test_mfg_data_is_cut_to_what_gap_takes(void) {
    uint8_t data[BLE_WIRE_MFG_DATA_MAX];
    size_t len = ble_wire_mfg_data(data, "Abcdefghijklmnopqrstuvwxyz");
    CHECK_EQ_INT(len, BLE_WIRE_MFG_DATA_MAX);
    CHECK_MEM_EQ(data + 2, "Abcdefghijklmnopqrs", BLE_WIRE_MFG_DATA_MAX - 2);
    len = ble_wire_mfg_data(data, "Abcdefghijklmnopqrst");
    CHECK_EQ_INT(len, BLE_WIRE_MFG_DATA_MAX);
    CHECK_MEM_EQ(data + 2, "Abcdefghijklmnopqrs", BLE_WIRE_MFG_DATA_MAX - 2);
}

static void test_mfg_data_keeps_a_name_that_just_fits(void) {
    uint8_t data[BLE_WIRE_MFG_DATA_MAX];
    size_t len = ble_wire_mfg_data(data, "Abcdefghijklmnopqrs");
    CHECK_EQ_INT(len, BLE_WIRE_MFG_DATA_MAX);
    CHECK_MEM_EQ(data + 2, "Abcdefghijklmnopqrs", BLE_WIRE_MFG_DATA_MAX - 2);
}

static void test_scan_response_fits_31_bytes(void) {
    /* the stack adds the AD length and type (2) to the manufacturer data */
    CHECK_EQ_INT(ble_wire_scan_response_size(BLE_WIRE_MFG_DATA_MAX), 23);
    CHECK(ble_wire_scan_response_size(BLE_WIRE_MFG_DATA_MAX) <= BLE_WIRE_ADV_MAX);
}

static void test_link_is_off_until_the_transport_opens(void) {
    CHECK_EQ_INT(ble_wire_link(false, false, false), BleWireLinkOff);
    CHECK_EQ_INT(ble_wire_link(false, true, false), BleWireLinkOff);
    /* a stale subscription of a closed transport is no host */
    CHECK_EQ_INT(ble_wire_link(false, true, true), BleWireLinkOff);
}

static void test_link_advertises_while_no_host_subscribed(void) {
    CHECK_EQ_INT(ble_wire_link(true, true, false), BleWireLinkAdvertising);
    CHECK_EQ_INT(ble_wire_link(true, false, false), BleWireLinkOff);
}

static void test_subscribed_host_is_connected(void) {
    /* firmware 1.5.1 reports Connected only after pairing, which a host
     * never does, so the Bt status may still say advertising */
    CHECK_EQ_INT(ble_wire_link(true, true, true), BleWireLinkConnected);
    CHECK_EQ_INT(ble_wire_link(true, false, true), BleWireLinkConnected);
}

static void test_mac_differs_from_the_default_profile(void) {
    const uint8_t base[6] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66};
    uint8_t mac[6];
    ble_wire_mac(mac, base);
    CHECK(memcmp(mac, base, 6) != 0);
    /* nor the HID profile's, which increments byte 2 */
    CHECK(mac[2] != (uint8_t)(base[2] + 1));
    CHECK_MEM_EQ(mac, base, 2);
    CHECK_MEM_EQ(mac + 3, base + 3, 3);
}

/* Packet builders: [type, event code, parameter length, parameters]. */
static uint8_t packet[300];

static const uint8_t* vendor_event(uint16_t ecode, const uint8_t* params, size_t len) {
    packet[0] = 0x04;
    packet[1] = 0xFF;
    packet[2] = (uint8_t)(2 + len);
    packet[3] = ecode & 0xFF;
    packet[4] = ecode >> 8;
    memcpy(packet + 5, params, len);
    return packet;
}

static const uint8_t*
    attribute_modified(uint16_t handle, const uint8_t* data, uint16_t len, uint16_t claimed) {
    uint8_t params[8 + 250];
    params[0] = 0x01; /* connection handle */
    params[1] = 0x08;
    params[2] = handle & 0xFF;
    params[3] = handle >> 8;
    params[4] = 0; /* offset */
    params[5] = 0;
    params[6] = claimed & 0xFF;
    params[7] = claimed >> 8;
    memcpy(params + 8, data, len);
    return vendor_event(0x0C01, params, 8 + len);
}

static const BleWireHandles handles = {.rx_value = 0x0011, .tx_cccd = 0x0015};

static void test_write_to_rx_is_received_bytes(void) {
    const uint8_t data[] = "{\"type\":\"hello\"}";
    BleWireEvent e = ble_wire_decode(
        attribute_modified(0x0011, data, sizeof(data) - 1, sizeof(data) - 1), &handles);
    CHECK_EQ_INT(e.type, BleWireEventRx);
    CHECK_EQ_INT(e.len, sizeof(data) - 1);
    CHECK_MEM_EQ(e.data, data, sizeof(data) - 1);
}

static void test_write_of_243_bytes_is_received_whole(void) {
    uint8_t data[243];
    for(size_t i = 0; i < sizeof(data); i++)
        data[i] = (uint8_t)('a' + i % 26);
    BleWireEvent e = ble_wire_decode(attribute_modified(0x0011, data, 243, 243), &handles);
    CHECK_EQ_INT(e.type, BleWireEventRx);
    CHECK_EQ_INT(e.len, 243);
    CHECK_MEM_EQ(e.data, data, 243);
}

static void test_write_claiming_more_than_the_event_holds_is_ignored(void) {
    const uint8_t data[] = "abc";
    BleWireEvent e = ble_wire_decode(attribute_modified(0x0011, data, 3, 4), &handles);
    CHECK_EQ_INT(e.type, BleWireEventOther);
}

static void test_write_to_another_attribute_is_not_ours(void) {
    const uint8_t data[] = "abc";
    BleWireEvent e = ble_wire_decode(attribute_modified(0x0012, data, 3, 3), &handles);
    CHECK_EQ_INT(e.type, BleWireEventOther);
}

static void test_cccd_write_enabling_notifications_subscribes(void) {
    const uint8_t on[] = {0x01, 0x00};
    BleWireEvent e = ble_wire_decode(attribute_modified(0x0015, on, 2, 2), &handles);
    CHECK_EQ_INT(e.type, BleWireEventSubscribe);
    CHECK(e.subscribed);
}

static void test_cccd_write_disabling_notifications_unsubscribes(void) {
    const uint8_t off[] = {0x00, 0x00};
    BleWireEvent e = ble_wire_decode(attribute_modified(0x0015, off, 2, 2), &handles);
    CHECK_EQ_INT(e.type, BleWireEventSubscribe);
    CHECK(!e.subscribed);
}

static void test_cccd_write_of_indications_only_does_not_subscribe(void) {
    const uint8_t indicate[] = {0x02, 0x00};
    BleWireEvent e = ble_wire_decode(attribute_modified(0x0015, indicate, 2, 2), &handles);
    CHECK_EQ_INT(e.type, BleWireEventSubscribe);
    CHECK(!e.subscribed);
}

static void test_empty_cccd_write_is_ignored(void) {
    BleWireEvent e = ble_wire_decode(attribute_modified(0x0015, NULL, 0, 0), &handles);
    CHECK_EQ_INT(e.type, BleWireEventOther);
}

static void test_mtu_exchange_gives_the_mtu(void) {
    const uint8_t params[] = {0x01, 0x08, 185, 0};
    BleWireEvent e = ble_wire_decode(vendor_event(0x0C03, params, sizeof(params)), &handles);
    CHECK_EQ_INT(e.type, BleWireEventMtu);
    CHECK_EQ_INT(e.mtu, 185);
}

static void test_mtu_above_255_is_read_little_endian(void) {
    const uint8_t params[] = {0x01, 0x08, 0x05, 0x02};
    BleWireEvent e = ble_wire_decode(vendor_event(0x0C03, params, sizeof(params)), &handles);
    CHECK_EQ_INT(e.type, BleWireEventMtu);
    CHECK_EQ_INT(e.mtu, 517);
}

static void test_short_mtu_event_is_ignored(void) {
    const uint8_t params[] = {0x01, 0x08, 185};
    BleWireEvent e = ble_wire_decode(vendor_event(0x0C03, params, sizeof(params)), &handles);
    CHECK_EQ_INT(e.type, BleWireEventOther);
}

static void test_tx_pool_available_is_passed_on(void) {
    /* the sender does not wait for room: the SDK's update already does */
    const uint8_t params[] = {0x01, 0x08, 0x04, 0x00};
    BleWireEvent e = ble_wire_decode(vendor_event(0x0C16, params, sizeof(params)), &handles);
    CHECK_EQ_INT(e.type, BleWireEventOther);
}

static void test_disconnection_complete_ends_the_link(void) {
    const uint8_t disconnected[] = {0x04, 0x05, 0x04, 0x00, 0x01, 0x08, 0x13};
    BleWireEvent e = ble_wire_decode(disconnected, &handles);
    CHECK_EQ_INT(e.type, BleWireEventDisconnected);
}

static void test_failed_disconnection_keeps_the_link(void) {
    const uint8_t failed[] = {0x04, 0x05, 0x04, 0x0C, 0x01, 0x08, 0x13};
    BleWireEvent e = ble_wire_decode(failed, &handles);
    CHECK_EQ_INT(e.type, BleWireEventOther);
}

static void test_short_attribute_modified_is_ignored(void) {
    /* seven of the eight header bytes: no data length */
    const uint8_t params[] = {0x01, 0x08, 0x11, 0x00, 0x00, 0x00, 0x00};
    CHECK_EQ_INT(
        ble_wire_decode(vendor_event(0x0C01, params, sizeof(params)), &handles).type,
        BleWireEventOther);
}

static void test_short_disconnection_complete_is_ignored(void) {
    /* status and connection handle, no reason */
    const uint8_t short_event[] = {0x04, 0x05, 0x03, 0x00, 0x01, 0x08};
    CHECK_EQ_INT(ble_wire_decode(short_event, &handles).type, BleWireEventOther);
}

static void test_other_events_are_not_ours(void) {
    const uint8_t meta[] = {0x04, 0x3E, 0x01, 0x01};
    CHECK_EQ_INT(ble_wire_decode(meta, &handles).type, BleWireEventOther);
    const uint8_t params[] = {0x01, 0x08};
    CHECK_EQ_INT(
        ble_wire_decode(vendor_event(0x0C02, params, sizeof(params)), &handles).type,
        BleWireEventOther);
    const uint8_t no_code[] = {0x04, 0xFF, 0x01, 0x01};
    CHECK_EQ_INT(ble_wire_decode(no_code, &handles).type, BleWireEventOther);
}

int main(void) {
    RUN_TEST(test_uuids_match_the_gatt_table);
    RUN_TEST(test_advertisement_without_a_name_fits_31_bytes);
    RUN_TEST(test_advertisement_with_a_name_does_not_fit);
    RUN_TEST(test_gap_name_advertises_no_name);
    RUN_TEST(test_gap_name_keeps_the_device_name);
    RUN_TEST(test_gap_name_falls_back_when_the_flipper_has_none);
    RUN_TEST(test_mfg_data_is_the_test_company_id_and_the_name);
    RUN_TEST(test_mfg_data_falls_back_when_the_flipper_has_none);
    RUN_TEST(test_mfg_data_is_cut_to_what_gap_takes);
    RUN_TEST(test_mfg_data_keeps_a_name_that_just_fits);
    RUN_TEST(test_scan_response_fits_31_bytes);
    RUN_TEST(test_link_is_off_until_the_transport_opens);
    RUN_TEST(test_link_advertises_while_no_host_subscribed);
    RUN_TEST(test_subscribed_host_is_connected);
    RUN_TEST(test_mac_differs_from_the_default_profile);
    RUN_TEST(test_write_to_rx_is_received_bytes);
    RUN_TEST(test_write_of_243_bytes_is_received_whole);
    RUN_TEST(test_write_claiming_more_than_the_event_holds_is_ignored);
    RUN_TEST(test_write_to_another_attribute_is_not_ours);
    RUN_TEST(test_cccd_write_enabling_notifications_subscribes);
    RUN_TEST(test_cccd_write_disabling_notifications_unsubscribes);
    RUN_TEST(test_cccd_write_of_indications_only_does_not_subscribe);
    RUN_TEST(test_empty_cccd_write_is_ignored);
    RUN_TEST(test_mtu_exchange_gives_the_mtu);
    RUN_TEST(test_mtu_above_255_is_read_little_endian);
    RUN_TEST(test_short_mtu_event_is_ignored);
    RUN_TEST(test_tx_pool_available_is_passed_on);
    RUN_TEST(test_disconnection_complete_ends_the_link);
    RUN_TEST(test_failed_disconnection_keeps_the_link);
    RUN_TEST(test_short_attribute_modified_is_ignored);
    RUN_TEST(test_short_disconnection_complete_is_ignored);
    RUN_TEST(test_other_events_are_not_ours);
    return MINITEST_RESULT();
}
