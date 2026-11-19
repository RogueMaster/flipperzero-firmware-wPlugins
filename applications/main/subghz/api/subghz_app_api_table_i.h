#include "../subghz_i.h"

/*
 * A list of app's private functions and objects to expose for plugins.
 * It is used to generate a table of symbols for import resolver to use.
 *
 * Keep this list small: anything exported here is pinned into the firmware
 * image for good, since the resolver counts as a reference.
 */
static constexpr auto subghz_app_api_table = sort(create_array_t<sym_entry>(
    // Frequency Analyzer: its worker walks the frequency list, its view refuses to hand back a
    // frequency the current radio cannot tune, and its scene persists the trigger and feedback
    // levels on the way out.
    API_METHOD(subghz_txrx_radio_device_get, SubGhzRadioDeviceType, (SubGhzTxRx*)),
    API_METHOD(subghz_txrx_get_setting, SubGhzSetting*, (SubGhzTxRx*)),
    API_METHOD(subghz_txrx_radio_device_is_frequency_valid, bool, (SubGhzTxRx*, uint32_t)),
    API_METHOD(subghz_last_settings_save, bool, (SubGhzLastSettings*)),
    // Saved signal settings reuse the app's existing file and radio paths.
    API_METHOD(subghz_key_load, bool, (SubGhz*, const char*, bool)),
    API_METHOD(subghz_tx_start, bool, (SubGhz*, FlipperFormat*)),
    API_METHOD(subghz_txrx_stop, void, (SubGhzTxRx*)),
    // Radio Settings retains RM's module polling, amplifier control and GPS selection.
    API_METHOD(
        subghz_txrx_radio_device_set,
        SubGhzRadioDeviceType,
        (SubGhzTxRx*, SubGhzRadioDeviceType)),
    API_METHOD(subghz_txrx_radio_device_is_external_connected, bool, (SubGhzTxRx*, const char*)),
    API_METHOD(subghz_txrx_radio_device_poll, bool, (SubGhzTxRx*)),
    API_METHOD(subghz_txrx_set_ext_leds_and_amp, void, (SubGhzTxRx*, bool)),
    API_METHOD(subghz_gps_plugin_init, SubGhzGPS*, (uint32_t)),
    API_METHOD(subghz_gps_plugin_deinit, void, (SubGhzGPS*))));
