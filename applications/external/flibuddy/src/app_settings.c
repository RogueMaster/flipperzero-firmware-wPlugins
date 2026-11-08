#include "app_settings.h"

/* Versions 1 and 2 stored a TransportKind. */
static bool known_transport(unsigned kind) {
    return kind == TransportKindUsb || kind == TransportKindBluetooth;
}

static bool known_connection(unsigned connection) {
    return connection == AppConnectionUsb || connection == AppConnectionBluetooth ||
           connection == AppConnectionAuto;
}

static bool known_feedback(unsigned feedback) {
    return feedback == AppFeedbackVibroLed || feedback == AppFeedbackLed ||
           feedback == AppFeedbackOff;
}

static bool valid_v2(const AppSettingsV2* s) {
    return s->version == 2 && known_transport(s->transport) && s->autosave <= 1 &&
           known_feedback(s->feedback) && s->sound <= 1;
}

static bool valid_v3(const AppSettingsV3* s) {
    return s->version == 3 && known_connection(s->connection) && s->autosave <= 1 &&
           known_feedback(s->feedback) && s->sound <= 1 && s->after_scan <= AppAfterScanWeigh &&
           s->weigh_as <= AppWeighAsNet;
}

static bool valid(const AppSettings* s) {
    return s->version == APP_SETTINGS_VERSION && known_connection(s->connection) &&
           s->autosave <= 1 && known_feedback(s->feedback) && s->sound <= 1 &&
           s->after_scan <= AppAfterScanWeigh && s->weigh_as <= AppWeighAsNet && s->reserved <= 1;
}

void app_settings_defaults(AppSettings* settings) {
    settings->version = APP_SETTINGS_VERSION;
    settings->connection = AppConnectionAuto;
    settings->autosave = 0;
    settings->feedback = AppFeedbackVibroLed;
    settings->sound = 0;
    settings->after_scan = AppAfterScanShow;
    settings->weigh_as = AppWeighAsGross;
    settings->reserved = 0;
}

void app_settings_load(AppSettings* settings, const AppSettingsStorage* storage) {
    AppSettings stored;
    AppSettingsV3 v3;
    AppSettingsV2 v2;
    AppSettingsV1 v1;
    if(storage->load(storage->ctx, APP_SETTINGS_VERSION, &stored, sizeof(stored)) &&
       valid(&stored)) {
        *settings = stored;
        settings->reserved = 0;
        return;
    }
    app_settings_defaults(settings);
    if(storage->load(storage->ctx, 3, &v3, sizeof(v3)) && valid_v3(&v3)) {
        settings->connection = v3.connection;
        settings->autosave = v3.autosave;
        settings->feedback = v3.feedback;
        settings->sound = v3.sound;
        settings->after_scan = v3.after_scan;
        settings->weigh_as = v3.weigh_as;
    } else if(storage->load(storage->ctx, 2, &v2, sizeof(v2)) && valid_v2(&v2)) {
        settings->connection = v2.transport; /* the same numbers */
        settings->autosave = v2.autosave;
        settings->feedback = v2.feedback;
        settings->sound = v2.sound;
    } else if(
        storage->load(storage->ctx, 1, &v1, sizeof(v1)) && v1.version == 1 &&
        known_transport(v1.transport)) {
        settings->connection = v1.transport;
    }
}

bool app_settings_save(const AppSettings* settings, const AppSettingsStorage* storage) {
    return storage->save(storage->ctx, settings, sizeof(*settings));
}

AppConnection app_settings_connection(const AppSettings* settings) {
    return (AppConnection)settings->connection;
}

void app_settings_set_connection(AppSettings* settings, AppConnection connection) {
    if(known_connection(connection)) settings->connection = (uint8_t)connection;
}

bool app_connection_uses(AppConnection connection, TransportKind kind) {
    switch(connection) {
    case AppConnectionUsb:
        return kind == TransportKindUsb;
    case AppConnectionBluetooth:
        return kind == TransportKindBluetooth;
    case AppConnectionAuto:
        return kind == TransportKindUsb || kind == TransportKindBluetooth;
    }
    return false;
}

bool app_settings_autosave(const AppSettings* settings) {
    return settings->autosave != 0;
}

void app_settings_set_autosave(AppSettings* settings, bool on) {
    settings->autosave = on ? 1 : 0;
}

AppFeedback app_settings_feedback(const AppSettings* settings) {
    return (AppFeedback)settings->feedback;
}

void app_settings_set_feedback(AppSettings* settings, AppFeedback feedback) {
    if(known_feedback(feedback)) settings->feedback = (uint8_t)feedback;
}

bool app_settings_sound(const AppSettings* settings) {
    return settings->sound != 0;
}

void app_settings_set_sound(AppSettings* settings, bool on) {
    settings->sound = on ? 1 : 0;
}

AppAfterScan app_settings_after_scan(const AppSettings* settings) {
    return (AppAfterScan)settings->after_scan;
}

void app_settings_set_after_scan(AppSettings* settings, AppAfterScan after) {
    if(after == AppAfterScanShow || after == AppAfterScanWeigh)
        settings->after_scan = (uint8_t)after;
}

AppWeighAs app_settings_weigh_as(const AppSettings* settings) {
    return (AppWeighAs)settings->weigh_as;
}

void app_settings_set_weigh_as(AppSettings* settings, AppWeighAs weigh_as) {
    if(weigh_as == AppWeighAsGross || weigh_as == AppWeighAsNet) {
        settings->weigh_as = (uint8_t)weigh_as;
    }
}
