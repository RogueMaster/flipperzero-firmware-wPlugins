#pragma once

#include <stdint.h>
#include <stdbool.h>

#define DISPLAY_BATTERY_BAR              0
#define DISPLAY_BATTERY_PERCENT          1
#define DISPLAY_BATTERY_INVERTED_PERCENT 2
#define DISPLAY_BATTERY_RETRO_3          3
#define DISPLAY_BATTERY_RETRO_5          4
#define DISPLAY_BATTERY_BAR_PERCENT      5
#define DISPLAY_BATTERY_NONE             6

#define ICON_STYLE_STOCK 0
#define ICON_STYLE_SLIM  1

typedef enum {
    FavoriteAppDownLong = 0,
    FavoriteAppLeftShort,
    FavoriteAppLeftLong,
    FavoriteAppRightShort,
    FavoriteAppRightLong,
    FavoriteAppUpLong,

    FavoriteAppNumber,
} FavoriteAppShortcut;

typedef enum {
    DummyAppLeftShort = 0,
    DummyAppLeftLong,
    DummyAppRightShort,
    DummyAppRightLong,
    DummyAppUpLong,
    DummyAppDownShort,
    DummyAppDownLong,
    DummyAppOkShort,
    DummyAppOkLong,

    DummyAppNumber,
} DummyAppShortcut;

typedef struct {
    char name_or_path[128];
} FavoriteApp;

typedef struct {
    uint32_t auto_lock_delay_ms;
    uint8_t displayBatteryPercentage;
    bool is_dumbmode;
    uint8_t icon_style;
    bool lock_icon;
    bool bt_icon;
    bool rpc_icon;
    bool sdcard;
    bool stealth_icon;
    bool top_bar;
    uint8_t dummy_mode;
    bool dumbmode_icon;
    bool auto_lock_with_pin;
    uint8_t display_clock;
    FavoriteApp favorite_apps[FavoriteAppNumber];
    FavoriteApp dummy_apps[DummyAppNumber];
} DesktopSettings;

void desktop_settings_load(DesktopSettings* settings);
void desktop_settings_save(const DesktopSettings* settings);
