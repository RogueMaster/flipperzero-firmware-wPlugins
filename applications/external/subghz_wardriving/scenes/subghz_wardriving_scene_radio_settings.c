#include "../subghz_wardriving_i.h"
#include "../helpers/subghz_wardriving_custom_event.h"
#include <lib/toolbox/value_index.h>
#include <applications/drivers/subghz/cc1101_ext/cc1101_ext_interconnect.h>

#define RADIO_DEVICE_COUNT 2
const char* const radio_device_text[RADIO_DEVICE_COUNT] = {
    "Internal",
    "External",
};

const uint32_t radio_device_value[RADIO_DEVICE_COUNT] = {
    SubGhzRadioDeviceTypeInternal,
    SubGhzRadioDeviceTypeExternalCC1101,
};

#define ON_OFF_COUNT 2
const char* const on_off_text[ON_OFF_COUNT] = {
    "OFF",
    "ON",
};

// Keep saved protocol values stable when the firmware has no phone GPS service.
const char* const gps_protocol_text[] = {
    "OFF",
#if SUBGHZ_GPS_HAS_RPC
    "RPC",
#endif
    "NMEA",
    "Ubox",
};
const uint32_t gps_protocol_value[] = {
    SubGhzGpsProtocolOff,
#if SUBGHZ_GPS_HAS_RPC
    SubGhzGpsProtocolRpc,
#endif
    SubGhzGpsProtocolNmea,
    SubGhzGpsProtocolUbox,
};
#define GPS_PROTOCOL_COUNT COUNT_OF(gps_protocol_value)

#define GPS_BAUDRATE_COUNT 6
const char* const gps_baudrate_text[GPS_BAUDRATE_COUNT] = {
    "4800",
    "9600",
    "19200",
    "38400",
    "57600",
    "115200",
};
const uint32_t gps_baudrate_value[GPS_BAUDRATE_COUNT] = {
    4800,
    9600,
    19200,
    38400,
    57600,
    115200,
};

// index == SubGhzGpsPins
const char* const gps_pins_text[SubGhzGpsPinsCount] = {
    "13,14",
    "15,16",
};

#define DEBUG_COUNTER_COUNT 17
const char* const debug_counter_text[DEBUG_COUNTER_COUNT] = {
    "+1",
    "+2",
    "+3",
    "+4",
    "+5",
    "+10",
    "+50",
    "OVFL",
    "OFEX",
    "No",
    "-1",
    "-2",
    "-3",
    "-4",
    "-5",
    "-10",
    "-50",
};
const int32_t debug_counter_val[DEBUG_COUNTER_COUNT] = {
    1,
    2,
    3,
    4,
    5,
    10,
    50,
    65535,
    -2147483647,
    0,
    -1,
    -2,
    -3,
    -4,
    -5,
    -10,
    -50,
};

#define GPS_BAUDRATE_LOCKED_MSG "Not used in\nthis source"

static VariableItem* gps_baudrate_item = NULL;
static VariableItem* gps_pins_item = NULL;

//TX Power
#define TX_POWER_COUNT 9
const char* const tx_power_text[TX_POWER_COUNT] = {
    "Preset",
    "10dBm +",
    "7dBm",
    "5dBm",
    "0dBm",
    "-10dBm",
    "-15dBm",
    "-20dBm",
    "-30dBm",
};

static void subghz_scene_radio_settings_set_device(VariableItem* item) {
    SubGhz* subghz = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);

    if(!subghz_wardriving_txrx_radio_device_is_external_connected(
           subghz->txrx, SUBGHZ_DEVICE_CC1101_EXT_NAME) &&
       radio_device_value[index] == SubGhzRadioDeviceTypeExternalCC1101) {
        //ToDo correct if there is more than 1 module
        index = 0;
    }
    variable_item_set_current_value_text(item, radio_device_text[index]);
    subghz_wardriving_txrx_radio_device_set(subghz->txrx, radio_device_value[index]);
}

static void subghz_scene_radio_settings_set_tx_power(VariableItem* item) {
    SubGhz* subghz = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);

    //Update the Menu Item on screen
    variable_item_set_current_value_text(item, tx_power_text[index]);

    //Set TX power and remember setting
    subghz->last_settings->tx_power = subghz->tx_power = index;

    //Save the settings now, this is the convention here!
    subghz_wardriving_last_settings_save(subghz->last_settings);
}

static void subghz_scene_receiver_config_set_debug_counter(VariableItem* item) {
    uint8_t index = variable_item_get_current_value_index(item);

    variable_item_set_current_value_text(item, debug_counter_text[index]);
    furi_hal_subghz_set_rolling_counter_mult(debug_counter_val[index]);
}

static void subghz_scene_receiver_config_set_gps_protocol(VariableItem* item) {
    SubGhz* subghz = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);

    variable_item_set_current_value_text(item, gps_protocol_text[index]);
    subghz->last_settings->gps_protocol = gps_protocol_value[index];
    subghz_wardriving_last_settings_save(subghz->last_settings);

    if(gps_baudrate_item) {
        variable_item_set_locked(
            gps_baudrate_item,
            gps_protocol_value[index] < SubGhzGpsProtocolNmea,
            GPS_BAUDRATE_LOCKED_MSG);
    }
    if(gps_pins_item) {
        variable_item_set_locked(
            gps_pins_item,
            gps_protocol_value[index] < SubGhzGpsProtocolNmea,
            GPS_BAUDRATE_LOCKED_MSG);
    }
}

static void subghz_scene_receiver_config_set_gps_baudrate(VariableItem* item) {
    SubGhz* subghz = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);

    variable_item_set_current_value_text(item, gps_baudrate_text[index]);
    subghz->last_settings->gps_baudrate = gps_baudrate_value[index];
    subghz_wardriving_last_settings_save(subghz->last_settings);
}

static void subghz_scene_receiver_config_set_gps_pins(VariableItem* item) {
    SubGhz* subghz = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);

    variable_item_set_current_value_text(item, gps_pins_text[index]);
    subghz->last_settings->gps_pins = index;
    subghz_wardriving_last_settings_save(subghz->last_settings);
}

static void subghz_scene_receiver_config_set_protocol_file_names(VariableItem* item) {
    SubGhz* subghz = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);

    variable_item_set_current_value_text(item, on_off_text[index]);

    subghz->last_settings->protocol_file_names = (index == 1);
    subghz_wardriving_last_settings_save(subghz->last_settings);
}

void subghz_scene_radio_settings_on_enter(void* context) {
    SubGhz* subghz = context;

    VariableItemList* variable_item_list = subghz->variable_item_list;
    int32_t value_index;
    VariableItem* item;

    uint8_t value_count_device = RADIO_DEVICE_COUNT;
    if(subghz_wardriving_txrx_radio_device_get(subghz->txrx) == SubGhzRadioDeviceTypeInternal &&
       !subghz_wardriving_txrx_radio_device_is_external_connected(
           subghz->txrx, SUBGHZ_DEVICE_CC1101_EXT_NAME))
        value_count_device = 1; // Only 1 item if external disconnected
    item = variable_item_list_add(
        subghz->variable_item_list,
        "Module",
        value_count_device,
        subghz_scene_radio_settings_set_device,
        subghz);
    value_index = value_index_uint32(
        subghz_wardriving_txrx_radio_device_get(subghz->txrx),
        radio_device_value,
        value_count_device);
    variable_item_set_current_value_index(item, value_index);
    variable_item_set_current_value_text(item, radio_device_text[value_index]);

    //Add TX Power
    item = variable_item_list_add(
        subghz->variable_item_list,
        "TX Power",
        TX_POWER_COUNT,
        subghz_scene_radio_settings_set_tx_power,
        subghz);

    value_index = subghz->tx_power;
    variable_item_set_current_value_index(item, value_index);
    variable_item_set_current_value_text(item, tx_power_text[value_index]);

    item = variable_item_list_add(
        variable_item_list,
        "GPS source",
        GPS_PROTOCOL_COUNT,
        subghz_scene_receiver_config_set_gps_protocol,
        subghz);
    value_index = value_index_uint32(
        subghz->last_settings->gps_protocol, gps_protocol_value, GPS_PROTOCOL_COUNT);
    variable_item_set_current_value_index(item, value_index);
    variable_item_set_current_value_text(item, gps_protocol_text[value_index]);

    item = variable_item_list_add(
        variable_item_list,
        "GPS Baudrate",
        GPS_BAUDRATE_COUNT,
        subghz_scene_receiver_config_set_gps_baudrate,
        subghz);
    value_index = value_index_uint32(
        subghz->last_settings->gps_baudrate, gps_baudrate_value, GPS_BAUDRATE_COUNT);
    variable_item_set_current_value_index(item, value_index);
    variable_item_set_current_value_text(item, gps_baudrate_text[value_index]);
    gps_baudrate_item = item;
    variable_item_set_locked(
        item,
        subghz->last_settings->gps_protocol < SubGhzGpsProtocolNmea,
        GPS_BAUDRATE_LOCKED_MSG);

    item = variable_item_list_add(
        variable_item_list,
        "GPS Pins",
        SubGhzGpsPinsCount,
        subghz_scene_receiver_config_set_gps_pins,
        subghz);
    value_index = subghz->last_settings->gps_pins;
    if(value_index >= SubGhzGpsPinsCount) value_index = SubGhzGpsPinsUsart;
    variable_item_set_current_value_index(item, value_index);
    variable_item_set_current_value_text(item, gps_pins_text[value_index]);
    gps_pins_item = item;
    variable_item_set_locked(
        item,
        subghz->last_settings->gps_protocol < SubGhzGpsProtocolNmea,
        GPS_BAUDRATE_LOCKED_MSG);

    item = variable_item_list_add(
        variable_item_list,
        "Protocol Names",
        ON_OFF_COUNT,
        subghz_scene_receiver_config_set_protocol_file_names,
        subghz);
    value_index = subghz->last_settings->protocol_file_names;
    variable_item_set_current_value_index(item, value_index);
    variable_item_set_current_value_text(item, on_off_text[value_index]);

    item = variable_item_list_add(
        variable_item_list,
        "Counter Incr.",
        furi_hal_rtc_is_flag_set(FuriHalRtcFlagDebug) ? DEBUG_COUNTER_COUNT : 3,
        subghz_scene_receiver_config_set_debug_counter,
        subghz);
    value_index = value_index_int32(
        furi_hal_subghz_get_rolling_counter_mult(),
        debug_counter_val,
        furi_hal_rtc_is_flag_set(FuriHalRtcFlagDebug) ? DEBUG_COUNTER_COUNT : 3);
    furi_hal_subghz_set_rolling_counter_mult(debug_counter_val[value_index]);

    variable_item_set_current_value_index(item, value_index);
    variable_item_set_current_value_text(item, debug_counter_text[value_index]);

    view_dispatcher_switch_to_view(subghz->view_dispatcher, SubGhzViewIdVariableItemList);
}

bool subghz_scene_radio_settings_on_event(void* context, SceneManagerEvent event) {
    SubGhz* subghz = context;
    UNUSED(subghz);
    UNUSED(event);

    return false;
}

void subghz_scene_radio_settings_on_exit(void* context) {
    SubGhz* subghz = context;
    gps_baudrate_item = NULL;
    gps_pins_item = NULL;
    variable_item_list_set_selected_item(subghz->variable_item_list, 0);
    variable_item_list_reset(subghz->variable_item_list);
}
