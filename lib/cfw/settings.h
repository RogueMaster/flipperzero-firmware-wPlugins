#pragma once

#include <furi_hal_serial_types.h>
#include <furi_hal_version.h>
#include <stddef.h>
#include <stdint.h>
#include <toolbox/colors.h>

#define CFW_SETTINGS_PATH INT_PATH(".cfw_settings.txt")

#define ASSET_PACKS_NAME_LEN 32

typedef enum {
    BatteryIconOff,
    BatteryIconBar,
    BatteryIconPercent,
    BatteryIconInvertedPercent,
    BatteryIconRetro3,
    BatteryIconRetro5,
    BatteryIconBarPercent,
    BatteryIconCount,
} BatteryIcon;

typedef enum {
    MenuStyleList,
    MenuStyleWii,
    MenuStyleDsi,
    MenuStylePs4,
    MenuStyleVertical,
    MenuStyleC64,
    MenuStyleCompact,
    MenuStyleMNTM,
    MenuStyleCoverFlow,
    // Append styles to keep existing saved menu_style values compatible.
    MenuStyleGrid,
    MenuStyleMacintosh,
    MenuStyle3D,
    MenuStyleTerminal,
    MenuStyleEurocorp,
    MenuStyleCount,
} MenuStyle;

/** Return the loader plugin filename for a stored menu style, or NULL for List. */
static inline const char* cfw_menu_style_get_plugin_name(MenuStyle style) {
    switch(style) {
    case MenuStyleWii:
        return "menu_style_wii.fal";
    case MenuStyleDsi:
        return "menu_style_dsi.fal";
    case MenuStylePs4:
        return "menu_style_ps4.fal";
    case MenuStyleVertical:
        return "menu_style_vertical.fal";
    case MenuStyleC64:
        return "menu_style_c64.fal";
    case MenuStyleCompact:
        return "menu_style_compact.fal";
    case MenuStyleMNTM:
        return "menu_style_mntm.fal";
    case MenuStyleCoverFlow:
        return "menu_style_coverflow.fal";
    case MenuStyleGrid:
        return "menu_style_grid.fal";
    case MenuStyleMacintosh:
        return "menu_style_macintosh.fal";
    case MenuStyle3D:
        return "menu_style_3d.fal";
    case MenuStyleTerminal:
        return "menu_style_terminal.fal";
    case MenuStyleEurocorp:
        return "menu_style_eurocorp.fal";
    default:
        return NULL;
    }
}

typedef enum {
    SpiDefault, // CS on pa4
    SpiExtra, // CS on pc3
    SpiCount,
} SpiHandle;

typedef enum {
    ScreenColorModeDefault,
    ScreenColorModeCustom,
    ScreenColorModeRainbow,
    ScreenColorModeRgbBacklight,
    ScreenColorModeCount,
} ScreenColorMode;

typedef union __attribute__((packed)) {
    struct {
        ScreenColorMode mode;
        RgbColor rgb;
    };
    uint32_t value;
} ScreenFrameColor;

typedef enum {
    BrowserPathOff,
    BrowserPathCurrent,
    BrowserPathBrief,
    BrowserPathFull,
    BrowserPathModeCount,
} BrowserPathMode;

typedef struct {
    char asset_pack[ASSET_PACKS_NAME_LEN];
    uint32_t anim_speed;
    int32_t cycle_anims;
    bool unlock_anims;
    bool game_mode;
    MenuStyle menu_style;
    bool lock_on_boot;
    bool bad_pins_format;
    bool allow_locked_rpc_usb;
    bool allow_locked_rpc_ble;
    bool lockscreen_poweroff;
    bool lockscreen_time;
    bool lockscreen_seconds;
    bool lockscreen_date;
    bool lockscreen_statusbar;
    bool lockscreen_prompt;
    bool lockscreen_transparent;
    bool lockscreen_skip_animation;
    BatteryIcon battery_icon;
    bool status_icons;
    bool bar_borders;
    bool bar_background;
    bool sort_dirs_first;
    bool show_hidden_files;
    bool show_internal_tab;
    BrowserPathMode browser_path_mode;
    uint32_t favorite_timeout;
    bool scroll_marquee;
    bool dark_mode;
    bool rgb_backlight;
    uint32_t butthurt_timer;
    bool midnight_format_00;
    bool popup_overlay;
    SpiHandle spi_cc1101_handle;
    SpiHandle spi_nrf24_handle;
    FuriHalSerialId uart_esp_channel;
    FuriHalSerialId uart_nmea_channel;
    bool file_naming_prefix_after;
    FuriHalVersionColor spoof_color;
    ScreenFrameColor rpc_color_fg;
    ScreenFrameColor rpc_color_bg;
    /* Append fields to preserve offsets used by existing external apps. */
    MenuStyle game_menu_style;
    uint32_t game_start_point;
} CFWSettings;

void cfw_settings_save(void);
extern CFWSettings cfw_settings;
