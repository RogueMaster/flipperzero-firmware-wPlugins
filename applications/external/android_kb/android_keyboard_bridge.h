#pragma once

#include <furi.h>
#include <gui/gui.h>
#include <bt/bt_service/bt.h>
#include <furi_ble/profile_interface.h>
#include <input/input.h>
#include <notification/notification_messages.h>

#include "protocol.h"

#define AKB_TAG "AndroidKBBridge"

/** Flipper LCD framebuffer size used by optional screenshot capture. */
#define AKB_FB_SIZE 1024

typedef struct {
    Bt* bt;
    FuriHalBleProfileBase* profile;
    Gui* gui;
    ViewPort* view_port;
    NotificationApp* notifications;

    FuriPubSub* input_events;
    FuriPubSubSubscription* input_subscription;

    FuriMutex* mutex;
    FuriMessageQueue* hid_queue;

    AkbProtocolParser parser;

    bool ble_active;
    bool ble_phone_connected;
    /** Set from BLE RX callback; cleared on main loop — must be volatile. */
    volatile bool ble_need_buffer_notify;
    volatile bool exit_requested;
    volatile bool backlight_forced;
    /** Set from BT status callback; decremented on main loop. */
    volatile uint8_t reclaim_ticks;
    /** Polled on main loop; read under mutex in draw callback. */
    volatile bool usb_connected;
    uint32_t frames_received;
    uint32_t hid_applied;
    char status_line[48];

#ifdef AKB_SCREENSHOT
    uint8_t fb_copy[AKB_FB_SIZE];
    volatile bool fb_valid;
    volatile bool screenshot_request;
    uint8_t down_press_count;
    uint32_t down_first_tick;
#endif
} AndroidKeyboardBridge;
