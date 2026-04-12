#pragma once

#include <gui/gui.h>
#include <gui/view_port.h>
#include <subghz/devices/devices.h>

typedef enum {
    ScanDirectionUp,
    ScanDirectionDown,
} ScanDirection;

typedef enum {
    ScanModeChannel,
    ScanModeRange
} ScanMode;

typedef enum {
    ResumeLock,
    ResumeDelay,
    ResumeHold
} ScanResumeMode;

typedef enum {
    ScanSpeedFast = 0,
    ScanSpeedBalanced,
    ScanSpeedAccurate
} ScanSpeedMode;

typedef enum {
    ProfilePMR,
    ProfileFRS_GMRS
} ScanProfile;

typedef enum {
    UiScreenScanner,
    UiScreenMenu,
    UiScreenProfiles,
    UiScreenSettings,
    UiScreenResumeMode,
    UiScreenSquelch,
} UiScreen;

typedef struct {
    Gui* gui;
    ViewPort* view_port;
    FuriMessageQueue* event_queue;
    ScanProfile profile;
    ScanSpeedMode scan_speed;

    bool running;
    bool signal_locked;
    bool scanning_before_menu;
    bool ignore_input;
    bool menu_just_opened;

    uint32_t frequency;
    float rssi;
    float sensitivity;
    float rssi_smoothed;
    float bars_smooth;
    
    UiScreen ui_screen;
    uint8_t menu_index;
    uint8_t settings_index;

    bool scanning;
    ScanDirection scan_direction;

    const SubGhzDevice* radio_device;
    bool speaker_acquired;
    
    ScanResumeMode resume_mode;
    uint32_t delay_ms;
    uint32_t hold_timer;

    ScanMode scan_mode;

    uint32_t* channels;
    size_t channel_count;
    size_t channel_index;

} RadioScannerApp;


void radio_scanner_app_free(RadioScannerApp* app);
int32_t ham_scanner_app(void* p);
