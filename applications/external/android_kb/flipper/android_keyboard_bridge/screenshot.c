#include "screenshot.h"

#ifdef AKB_SCREENSHOT

#include <furi_hal_rtc.h>
#include <storage/storage.h>
#include <notification/notification_messages.h>

#include <string.h>
#include <stdio.h>

/** Three short Downs must land within this window (ms). */
#define AKB_SHOT_TRIPLE_MS 900

/** Flipper LCD is 128×64; u8g2 tile buffer is 1024 bytes. */
#define AKB_FB_W    128
#define AKB_FB_H    64
#define AKB_FB_SIZE 1024

static void akb_fb_commit_callback(
    uint8_t* data,
    size_t size,
    CanvasOrientation orientation,
    void* context) {
    UNUSED(orientation);
    AndroidKeyboardBridge* app = context;
    if(!data || size == 0) {
        return;
    }
    size_t n = size;
    if(n > AKB_FB_SIZE) {
        n = AKB_FB_SIZE;
    }
    // GUI thread — keep this short; main loop owns the file write.
    memcpy(app->fb_copy, data, n);
    app->fb_valid = true;
}

/**
 * Flipper framebuffer is vertical-tile (u8g2). PBM wants horizontal rows,
 * 1 bit/pixel, MSB = leftmost pixel, 1 = black.
 */
static void akb_fb_to_pbm_bits(const uint8_t* fb, uint8_t* out) {
    memset(out, 0, AKB_FB_SIZE);
    for(uint8_t y = 0; y < AKB_FB_H; y++) {
        for(uint8_t x = 0; x < AKB_FB_W; x++) {
            const uint8_t tile = fb[(y / 8) * AKB_FB_W + x];
            if(tile & (1u << (y % 8))) {
                out[y * (AKB_FB_W / 8) + (x / 8)] |= (uint8_t)(0x80u >> (x % 8));
            }
        }
    }
}

static bool akb_screenshot_write_pbm(AndroidKeyboardBridge* app) {
    if(!app->fb_valid) {
        return false;
    }

    uint8_t pbm_bits[AKB_FB_SIZE];
    akb_fb_to_pbm_bits(app->fb_copy, pbm_bits);

    DateTime dt;
    furi_hal_rtc_get_datetime(&dt);

    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(storage, EXT_PATH("apps_data"));
    storage_simply_mkdir(storage, EXT_PATH("apps_data/android_keyboard_bridge"));

    char path[96];
    snprintf(
        path,
        sizeof(path),
        EXT_PATH("apps_data/android_keyboard_bridge/shot_%04u%02u%02u_%02u%02u%02u.pbm"),
        dt.year,
        dt.month,
        dt.day,
        dt.hour,
        dt.minute,
        dt.second);

    File* file = storage_file_alloc(storage);
    bool ok = false;
    if(storage_file_open(file, path, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        const char* header = "P4\n128 64\n";
        ok = storage_file_write(file, header, strlen(header)) == strlen(header);
        if(ok) {
            ok = storage_file_write(file, pbm_bits, AKB_FB_SIZE) == AKB_FB_SIZE;
        }
        storage_file_close(file);
    }
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);

    if(ok) {
        notification_message(app->notifications, &sequence_success);
        furi_check(furi_mutex_acquire(app->mutex, FuriWaitForever) == FuriStatusOk);
        snprintf(app->status_line, sizeof(app->status_line), "Shot saved");
        furi_mutex_release(app->mutex);
        FURI_LOG_I(AKB_TAG, "Screenshot: %s", path);
    } else {
        notification_message(app->notifications, &sequence_error);
        furi_check(furi_mutex_acquire(app->mutex, FuriWaitForever) == FuriStatusOk);
        snprintf(app->status_line, sizeof(app->status_line), "Shot failed");
        furi_mutex_release(app->mutex);
        FURI_LOG_E(AKB_TAG, "Screenshot write failed: %s", path);
    }
    return ok;
}

void akb_screenshot_init(AndroidKeyboardBridge* app) {
    app->fb_valid = false;
    app->screenshot_request = false;
    app->down_press_count = 0;
    app->down_first_tick = 0;
    memset(app->fb_copy, 0, sizeof(app->fb_copy));
    gui_add_framebuffer_callback(app->gui, akb_fb_commit_callback, app);
}

void akb_screenshot_free(AndroidKeyboardBridge* app) {
    gui_remove_framebuffer_callback(app->gui, akb_fb_commit_callback, app);
}

void akb_screenshot_on_down_short(AndroidKeyboardBridge* app) {
    const uint32_t now = furi_get_tick();
    if(app->down_press_count == 0 ||
       (now - app->down_first_tick) > AKB_SHOT_TRIPLE_MS) {
        app->down_press_count = 1;
        app->down_first_tick = now;
        return;
    }

    app->down_press_count++;
    if(app->down_press_count >= 3) {
        app->down_press_count = 0;
        app->screenshot_request = true;
    }
}

void akb_screenshot_poll(AndroidKeyboardBridge* app) {
    if(!app->screenshot_request) {
        return;
    }
    app->screenshot_request = false;
    // Give the GUI one more commit so fb_copy matches what's on screen.
    view_port_update(app->view_port);
    furi_delay_ms(30);
    akb_screenshot_write_pbm(app);
}

#endif // AKB_SCREENSHOT
