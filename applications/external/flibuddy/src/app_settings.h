/* The app's persisted settings: the connection (USB, Bluetooth or both),
 * Auto-save new, After scan, Weigh as, Feedback and Sound, and whether the
 * first run is over (Get app opened by itself, or an app connected).
 *
 * The file is a saved_struct (magic APP_SETTINGS_MAGIC, version
 * APP_SETTINGS_VERSION) at APP_SETTINGS_PATH. The file access sits behind
 * AppSettingsStorage so that the choice logic here is tested on the host;
 * app_settings_file.c is the SDK backend. An older file keeps what it has
 * and takes the defaults for the rest: version 3 lacks the first-run flag,
 * version 2 also After scan and Weigh as, version 1 holds only the
 * transport. An older file comes from a run before, so its first run is
 * over. A missing or unreadable file, another version or any out-of-range
 * field loads the defaults (Auto, Auto-save off, After scan Show, Weigh as
 * Gross, Vibro+LED, Sound off, first run not over).
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "transport.h"

#define APP_SETTINGS_PARENT  "/ext/apps_data"
#define APP_SETTINGS_DIR     APP_SETTINGS_PARENT "/flibuddy"
#define APP_SETTINGS_PATH    APP_SETTINGS_DIR "/settings.bin"
#define APP_SETTINGS_MAGIC   0x5B
#define APP_SETTINGS_VERSION 4

/* Stored in the settings file, so the numbers never change. USB and
 * Bluetooth are the TransportKind numbers older files stored. */
typedef enum {
    AppConnectionUsb = 0,
    AppConnectionBluetooth = 1,
    AppConnectionAuto = 2, /* both at once; the first host to say hello wins */
} AppConnection;

typedef enum {
    AppFeedbackVibroLed = 0,
    AppFeedbackLed = 1,
    AppFeedbackOff = 2,
} AppFeedback;

/* What a known spool's scan opens. */
typedef enum {
    AppAfterScanShow = 0, /* the result */
    AppAfterScanWeigh = 1, /* Weight */
} AppAfterScan;

/* The mode Weight opens in. */
typedef enum {
    AppWeighAsGross = 0,
    AppWeighAsNet = 1,
} AppWeighAs;

typedef struct {
    uint8_t version; /* APP_SETTINGS_VERSION */
    uint8_t connection; /* AppConnection */
    uint8_t autosave; /* 0 or 1 */
    uint8_t feedback; /* AppFeedback */
    uint8_t sound; /* 0 or 1 */
    uint8_t after_scan; /* AppAfterScan */
    uint8_t weigh_as; /* AppWeighAs */
    uint8_t first_run_done; /* 0 or 1 */
} AppSettings;

/* The older files, read only to migrate them. */
typedef struct {
    uint8_t version; /* 3 */
    uint8_t connection;
    uint8_t autosave;
    uint8_t feedback;
    uint8_t sound;
    uint8_t after_scan;
    uint8_t weigh_as;
} AppSettingsV3;

typedef struct {
    uint8_t version; /* 2 */
    uint8_t transport; /* TransportKind */
    uint8_t autosave;
    uint8_t feedback;
    uint8_t sound;
} AppSettingsV2;

typedef struct {
    uint8_t version; /* 1 */
    uint8_t transport; /* TransportKind */
} AppSettingsV1;

typedef struct {
    /* Read exactly `size` bytes of a struct stored as `version`; false when
     * there is none, it does not match or cannot be read. */
    bool (*load)(void* ctx, uint8_t version, void* data, size_t size);
    /* Store `size` bytes as APP_SETTINGS_VERSION; false when the file could
     * not be written. */
    bool (*save)(void* ctx, const void* data, size_t size);
    void* ctx;
} AppSettingsStorage;

void app_settings_defaults(AppSettings* settings);

/* Always leaves valid settings: the stored ones, migrated ones or the
 * defaults. */
void app_settings_load(AppSettings* settings, const AppSettingsStorage* storage);

bool app_settings_save(const AppSettings* settings, const AppSettingsStorage* storage);

AppConnection app_settings_connection(const AppSettings* settings);

/* Unknown values are ignored. */
void app_settings_set_connection(AppSettings* settings, AppConnection connection);

/* Whether the connection runs this transport. */
bool app_connection_uses(AppConnection connection, TransportKind kind);

bool app_settings_autosave(const AppSettings* settings);

void app_settings_set_autosave(AppSettings* settings, bool on);

AppFeedback app_settings_feedback(const AppSettings* settings);

/* Unknown values are ignored. */
void app_settings_set_feedback(AppSettings* settings, AppFeedback feedback);

bool app_settings_sound(const AppSettings* settings);

void app_settings_set_sound(AppSettings* settings, bool on);

AppAfterScan app_settings_after_scan(const AppSettings* settings);

/* Unknown values are ignored. */
void app_settings_set_after_scan(AppSettings* settings, AppAfterScan after);

AppWeighAs app_settings_weigh_as(const AppSettings* settings);

/* Unknown values are ignored. */
void app_settings_set_weigh_as(AppSettings* settings, AppWeighAs weigh_as);

/* Whether Get app has opened by itself or an app has connected: Get app
 * then no longer opens by itself. */
bool app_settings_first_run_done(const AppSettings* settings);

void app_settings_set_first_run_done(AppSettings* settings);
