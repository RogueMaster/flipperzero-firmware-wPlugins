#include "period_tracker.h"

// Load PIN from file
bool period_tracker_load_pin(PeriodTrackerApp* app) {
    File* file = storage_file_alloc(app->storage);
    bool pin_exists = false;

    if(storage_file_open(file, APP_DATA_PATH("pin.txt"), FSAM_READ, FSOM_OPEN_EXISTING)) {
        char pin_buffer[16];
        uint16_t bytes_read = storage_file_read(file, pin_buffer, sizeof(pin_buffer) - 1);
        if(bytes_read > 0) {
            pin_buffer[bytes_read] = '\0';
            app->pin_code = (uint32_t)atoi(pin_buffer);
            pin_exists = true;
            FURI_LOG_I(TAG, "PIN loaded from file");
        }
        storage_file_close(file);
    }

    storage_file_free(file);
    return pin_exists;
}

// Check if PIN is locked
bool period_tracker_check_pin_locked(PeriodTrackerApp* app) {
    File* file = storage_file_alloc(app->storage);
    bool locked = false;

    if(storage_file_open(file, APP_DATA_PATH("pin.locked"), FSAM_READ, FSOM_OPEN_EXISTING)) {
        locked = true;
        storage_file_close(file);
        FURI_LOG_W(TAG, "PIN is locked");
    }

    storage_file_free(file);
    return locked;
}

// Save PIN to file
bool period_tracker_save_pin(PeriodTrackerApp* app, uint32_t pin) {
    File* file = storage_file_alloc(app->storage);
    bool success = false;

    if(storage_file_open(file, APP_DATA_PATH("pin.txt"), FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        char pin_buffer[16];
        int len = snprintf(pin_buffer, sizeof(pin_buffer), "%lu", pin);
        if(storage_file_write(file, pin_buffer, len)) {
            success = true;
            app->pin_code = pin;
            app->pin_set = true;
            FURI_LOG_I(TAG, "PIN saved");
        }
        storage_file_close(file);
    }

    storage_file_free(file);
    return success;
}

// Remove PIN
bool period_tracker_remove_pin(PeriodTrackerApp* app) {
    bool success = storage_simply_remove(app->storage, APP_DATA_PATH("pin.txt"));
    if(success) {
        app->pin_set = false;
        app->pin_code = 0;
        FURI_LOG_I(TAG, "PIN removed");
    }
    return success;
}

// Load PIN failure count
uint8_t period_tracker_load_pin_fails(PeriodTrackerApp* app) {
    File* file = storage_file_alloc(app->storage);
    uint8_t fails = 0;

    if(storage_file_open(file, APP_DATA_PATH("pin.fails"), FSAM_READ, FSOM_OPEN_EXISTING)) {
        char fails_buffer[8];
        uint16_t bytes_read = storage_file_read(file, fails_buffer, sizeof(fails_buffer) - 1);
        if(bytes_read > 0) {
            fails_buffer[bytes_read] = '\0';
            fails = (uint8_t)atoi(fails_buffer);
            FURI_LOG_I(TAG, "PIN fails loaded: %u", fails);
        }
        storage_file_close(file);
    }

    storage_file_free(file);
    return fails;
}

// Save PIN failure count
bool period_tracker_save_pin_fails(PeriodTrackerApp* app, uint8_t fails) {
    File* file = storage_file_alloc(app->storage);
    bool success = false;

    if(storage_file_open(file, APP_DATA_PATH("pin.fails"), FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        char fails_buffer[8];
        int len = snprintf(fails_buffer, sizeof(fails_buffer), "%u", fails);
        if(storage_file_write(file, fails_buffer, len)) {
            success = true;
            FURI_LOG_I(TAG, "PIN fails saved: %u", fails);
        }
        storage_file_close(file);
    }

    storage_file_free(file);
    return success;
}

// Reset PIN failure count
void period_tracker_reset_pin_fails(PeriodTrackerApp* app) {
    storage_simply_remove(app->storage, APP_DATA_PATH("pin.fails"));
    app->pin_fails = 0;
    FURI_LOG_I(TAG, "PIN fails reset");
}
