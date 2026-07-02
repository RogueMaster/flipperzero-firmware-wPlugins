#include "settings_i.h"

#include <furi_hal.h>
#include <rgb_backlight.h>
#include <flipper_format/flipper_format.h>

#define TAG "CFWSettings"

CFWSettings cfw_settings = {
    .asset_pack = "RM", // Default
    .anim_speed = 100, // 100%
    .cycle_anims = 0, // Meta.txt
    .unlock_anims = true, // ON
    .game_mode = false, // OFF
    .menu_style = MenuStyleWii, // DSi
    .lock_on_boot = true, // ON
    .bad_pins_format = false, // OFF
    .allow_locked_rpc_usb = false, // OFF
    .allow_locked_rpc_ble = false, // OFF
    .lockscreen_poweroff = true, // ON
    .lockscreen_time = true, // ON
    .lockscreen_seconds = false, // OFF
    .lockscreen_date = true, // ON
    .lockscreen_statusbar = true, // ON
    .lockscreen_prompt = true, // ON
    .lockscreen_transparent = false, // OFF
    .lockscreen_skip_animation = false, // OFF
    .battery_icon = BatteryIconBarPercent, // Bar %
    .status_icons = true, // ON
    .bar_borders = false, // OFF
    .bar_background = false, // OFF
    .sort_dirs_first = true, // ON
    .show_hidden_files = true, // ON
    .show_internal_tab = true, // ON
    .browser_path_mode = BrowserPathOff, // OFF
    .favorite_timeout = 0, // OFF
    .scroll_marquee = false, // OFF
    .dark_mode = false, // OFF
    .rgb_backlight = false, // OFF
    .butthurt_timer = 21600, // 6 H
    .midnight_format_00 = true, // 00:XX
    .popup_overlay = true, // ON
    .spi_cc1101_handle = SpiDefault, // &furi_hal_spi_bus_handle_external
    .spi_nrf24_handle = SpiDefault, // &furi_hal_spi_bus_handle_external
    .uart_esp_channel = FuriHalSerialIdUsart, // pin 13,14
    .uart_nmea_channel = FuriHalSerialIdUsart, // pin 13,14
    .file_naming_prefix_after = false, // Before
    .spoof_color = FuriHalVersionColorUnknown, // Real
    .rpc_color_fg = {{ScreenColorModeDefault, {.value = 0x000000}}}, // Default Black
    .rpc_color_bg = {{ScreenColorModeDefault, {.value = 0xFF8200}}}, // Default Orange
};

typedef enum {
    cfw_settings_type_str,
    cfw_settings_type_int,
    cfw_settings_type_uint,
    cfw_settings_type_bool,
} cfw_settings_type;

static const struct {
    cfw_settings_type type;
    const char* key;
    void* val;
    union {
        size_t str_len;
        struct {
            int32_t i_min;
            int32_t i_max;
            uint8_t i_sz;
        };
        struct {
            uint32_t u_min;
            uint32_t u_max;
            uint8_t u_sz;
        };
    };
#define setting(t, n)             .type = cfw_settings_type##t, .key = #n, .val = &cfw_settings.n
#define setting_str(n)            setting(_str, n), .str_len = sizeof(cfw_settings.n)
#define num(t, n, min, max)       .t##_min = min, .t##_max = max, .t##_sz = sizeof(cfw_settings.n)
#define setting_int(n, min, max)  setting(_int, n), num(i, n, min, max)
#define setting_uint(n, min, max) setting(_uint, n), num(u, n, min, max)
#define setting_enum(n, cnt)      setting_uint(n, 0, cnt - 1)
#define setting_bool(n)           setting(_bool, n)
} cfw_settings_entries[] = {
    {setting_str(asset_pack)},
    {setting_uint(anim_speed, 25, 300)},
    {setting_int(cycle_anims, -1, 86400)},
    {setting_bool(unlock_anims)},
    {setting_bool(game_mode)},
    {setting_enum(menu_style, MenuStyleCount)},
    {setting_bool(bad_pins_format)},
    {setting_bool(allow_locked_rpc_usb)},
    {setting_bool(allow_locked_rpc_ble)},
    {setting_bool(lock_on_boot)},
    {setting_bool(lockscreen_poweroff)},
    {setting_bool(lockscreen_time)},
    {setting_bool(lockscreen_seconds)},
    {setting_bool(lockscreen_date)},
    {setting_bool(lockscreen_statusbar)},
    {setting_bool(lockscreen_prompt)},
    {setting_bool(lockscreen_transparent)},
    {setting_bool(lockscreen_skip_animation)},
    {setting_enum(battery_icon, BatteryIconCount)},
    {setting_bool(status_icons)},
    {setting_bool(bar_borders)},
    {setting_bool(bar_background)},
    {setting_bool(sort_dirs_first)},
    {setting_bool(show_hidden_files)},
    {setting_bool(show_internal_tab)},
    {setting_enum(browser_path_mode, BrowserPathModeCount)},
    {setting_uint(favorite_timeout, 0, 60)},
    {setting_bool(scroll_marquee)},
    {setting_bool(dark_mode)},
    {setting_bool(rgb_backlight)},
    {setting_uint(butthurt_timer, 0, 172800)},
    {setting_bool(midnight_format_00)},
    {setting_bool(popup_overlay)},
    {setting_enum(spi_cc1101_handle, SpiCount)},
    {setting_enum(spi_nrf24_handle, SpiCount)},
    {setting_enum(uart_esp_channel, FuriHalSerialIdMax)},
    {setting_enum(uart_nmea_channel, FuriHalSerialIdMax)},
    {setting_bool(file_naming_prefix_after)},
    {setting_enum(spoof_color, FuriHalVersionColorCount)},
    {setting_uint(rpc_color_fg, 0x000000, 0xFFFFFF)},
    {setting_uint(rpc_color_bg, 0x000000, 0xFFFFFF)},
};

void cfw_settings_load(void) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* file = flipper_format_file_alloc(storage);
    if(flipper_format_file_open_existing(file, CFW_SETTINGS_PATH)) {
        FuriString* val_str = furi_string_alloc();
        int32_t val_int;
        uint32_t val_uint;
        bool val_bool;

        bool ok;
        for(size_t entry_i = 0; entry_i < COUNT_OF(cfw_settings_entries); entry_i++) {
#define entry cfw_settings_entries[entry_i]
            switch(entry.type) {
            case cfw_settings_type_str:
                ok = flipper_format_read_string(file, entry.key, val_str);
                if(ok) strlcpy((char*)entry.val, furi_string_get_cstr(val_str), entry.str_len);
                break;
            case cfw_settings_type_int:
                ok = flipper_format_read_int32(file, entry.key, &val_int, 1);
                val_int = CLAMP(val_int, entry.i_max, entry.i_min);
                if(ok) memcpy(entry.val, &val_int, entry.i_sz);
                break;
            case cfw_settings_type_uint:
                ok = flipper_format_read_uint32(file, entry.key, &val_uint, 1);
                val_uint = CLAMP(val_uint, entry.u_max, entry.u_min);
                if(ok) memcpy(entry.val, &val_uint, entry.u_sz);
                break;
            case cfw_settings_type_bool:
                ok = flipper_format_read_bool(file, entry.key, &val_bool, 1);
                if(ok) *(bool*)entry.val = val_bool;
                break;
            default:
                continue;
            }
            if(!ok) flipper_format_rewind(file);
        }

        furi_string_free(val_str);
    }
    flipper_format_free(file);
    furi_record_close(RECORD_STORAGE);

    rgb_backlight_load_settings(cfw_settings.rgb_backlight);
}

void cfw_settings_save(void) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* file = flipper_format_file_alloc(storage);

    if(flipper_format_file_open_always(file, CFW_SETTINGS_PATH)) {
        int32_t tmp_int;
        uint32_t tmp_uint;
        for(size_t entry_i = 0; entry_i < COUNT_OF(cfw_settings_entries); entry_i++) {
#define entry cfw_settings_entries[entry_i]
            switch(entry.type) {
            case cfw_settings_type_str:
                flipper_format_write_string_cstr(file, entry.key, (char*)entry.val);
                break;
            case cfw_settings_type_int:
                tmp_int = 0;
                memcpy(&tmp_int, entry.val, entry.i_sz);
                flipper_format_write_int32(file, entry.key, &tmp_int, 1);
                break;
            case cfw_settings_type_uint:
                tmp_uint = 0;
                memcpy(&tmp_uint, entry.val, entry.u_sz);
                flipper_format_write_uint32(file, entry.key, &tmp_uint, 1);
                break;
            case cfw_settings_type_bool:
                flipper_format_write_bool(file, entry.key, (bool*)entry.val, 1);
                break;
            default:
                continue;
            }
        }
    }

    flipper_format_free(file);
    furi_record_close(RECORD_STORAGE);
}
