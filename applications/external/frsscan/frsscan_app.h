#pragma once

#include <gui/gui.h>
#include <gui/view_port.h>
#include <subghz/devices/devices.h>

typedef enum {
    ScanDirDown,
    ScanDirUp,
} ScanDir;

typedef struct {
    Gui* gui;
    ViewPort* view_port;
    FuriMessageQueue* event_queue;
    bool running;
    uint32_t freq_channel;
    uint32_t frequency;
    float rssi;
    float sensitivity;
    bool scanning;
    ScanDir scan_dir;
    const SubGhzDevice* radio_device;
    bool speaker_acquired;
} FRSScanApp;

FRSScanApp* frsscan_app_alloc(void);
void frsscan_app_free(FRSScanApp* app);
int32_t frsscan_app(void* p);
