// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt
//
// Shared Apple Find My advertisement and anti-stalking eligibility rules.
#include "test.h"
#include "tracker_rules.h"

void suite_tracker_rules(void);

void suite_tracker_rules(void) {
    printf("[tracker_rules]\n");

    // Apple manufacturer data starts with the little-endian company id, then
    // 0x12, payload length, and the status byte.
    const uint8_t nearby_airtag[] = {0x4c, 0x00, 0x12, 0x02, 0x10, 0x00};
    AppleFindMyAdvert info;
    CHECK(apple_find_my_decode(nearby_airtag, sizeof(nearby_airtag), &info));
    CHECK(info.valid);
    CHECK(!info.separated);
    CHECK(!info.owner_connected_recently);
    CHECK_INT_EQ(info.device_type, APPLE_FIND_MY_DEVICE_AIRTAG);
    CHECK_INT_EQ(info.battery_level, 0);
    CHECK(apple_find_my_is_tracker(&info));

    // A 0x19 payload is Apple's separated-state form. The status bit is
    // independent: it reports whether an owner connected during the current
    // key-rotation period, not proof of who owns the device.
    const uint8_t separated_airtag[29] = {0x4c, 0x00, 0x12, 0x19, 0x14};
    CHECK(apple_find_my_decode(separated_airtag, sizeof(separated_airtag), &info));
    CHECK(info.valid);
    CHECK(info.separated);
    CHECK(info.owner_connected_recently);
    CHECK_INT_EQ(info.device_type, APPLE_FIND_MY_DEVICE_AIRTAG);
    CHECK_INT_EQ(info.payload_len, APPLE_FIND_MY_SEPARATED_PAYLOAD_LEN);
    CHECK(apple_find_my_is_tracker(&info));

    // The Apple status nibble is a category filter. Phones/Macs (0) and
    // AirPods (3) must not become anti-stalking tracker rows just because they
    // use the same company id and Find My payload type.
    const uint8_t nearby_apple_device[] = {0x4c, 0x00, 0x12, 0x02, 0x00, 0x00};
    CHECK(apple_find_my_decode(nearby_apple_device, sizeof(nearby_apple_device), &info));
    CHECK(!apple_find_my_is_tracker(&info));
    const uint8_t nearby_airpods[] = {0x4c, 0x00, 0x12, 0x02, 0x30, 0x00};
    CHECK(apple_find_my_decode(nearby_airpods, sizeof(nearby_airpods), &info));
    CHECK(!apple_find_my_is_tracker(&info));

    // Malformed or unrelated Apple adverts are rejected instead of being
    // treated as AirTags on company id alone.
    const uint8_t wrong_company[] = {0x4d, 0x00, 0x12, 0x02, 0x10, 0x00};
    CHECK(!apple_find_my_decode(wrong_company, sizeof(wrong_company), &info));
    const uint8_t wrong_type[] = {0x4c, 0x00, 0x07, 0x02, 0x10, 0x00};
    CHECK(!apple_find_my_decode(wrong_type, sizeof(wrong_type), &info));
    const uint8_t wrong_length[] = {0x4c, 0x00, 0x12, 0x03, 0x10, 0x00, 0x00};
    CHECK(!apple_find_my_decode(wrong_length, sizeof(wrong_length), &info));
    const uint8_t truncated_separated[] = {0x4c, 0x00, 0x12, 0x19, 0x10};
    CHECK(!apple_find_my_decode(truncated_separated, sizeof(truncated_separated), &info));
    CHECK(!info.valid);

    // Weak distant adverts are intentionally outside the anti-stalking signal.
    CHECK(!ble_tracker_rssi_is_usable(-86));
    CHECK(ble_tracker_rssi_is_usable(BLE_TRACKER_MIN_RSSI));
    CHECK(ble_tracker_rssi_is_usable(-40));

    CHECK(ble_tracker_category_is_known(BLE_TRACKER_CAT_AIRTAG));
    CHECK(ble_tracker_category_is_known(BLE_TRACKER_CAT_TILE));
    CHECK(ble_tracker_category_is_known(BLE_TRACKER_CAT_SMARTTAG));
    CHECK(ble_tracker_category_is_known(BLE_TRACKER_CAT_FIND_MY_DEV));
    CHECK(!ble_tracker_category_is_known(1)); // Flock is not a personal tracker
    CHECK(!ble_tracker_category_is_known(6)); // Flipper is a recon tool
}
