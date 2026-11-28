#include "tpms_app_i.h"

#include <power/power_service/power.h>

#include <stdio.h>

#define TAG "TPMSRadio"
#define TPMS_EXT_POWER_STABILIZE_MS 120U
#define TPMS_DEVICE_CC1101_INT_NAME "cc1101_int"
#define TPMS_DEVICE_CC1101_EXT_NAME "cc1101_ext"

static void tpms_radio_power_enable(void) {
    Power* power = furi_record_open(RECORD_POWER);
    power_enable_otg(power, true);
    furi_record_close(RECORD_POWER);
}

static void tpms_radio_power_disable(void) {
    Power* power = furi_record_open(RECORD_POWER);
    power_enable_otg(power, false);
    furi_record_close(RECORD_POWER);
}

static bool tpms_radio_power_external(TPMSApp* app) {
    furi_assert(app);
    if(furi_hal_power_is_otg_enabled()) return true;

    tpms_radio_power_enable();
    app->txrx->radio_otg_owned = true;
    furi_delay_ms(TPMS_EXT_POWER_STABILIZE_MS);
    return furi_hal_power_is_otg_enabled();
}

static void tpms_radio_release_owned_power(TPMSApp* app) {
    furi_assert(app);
    if(app->txrx->radio_otg_owned) {
        tpms_radio_power_disable();
        app->txrx->radio_otg_owned = false;
    }
}

static void tpms_radio_end_external(TPMSApp* app) {
    furi_assert(app);
    if(app->txrx->radio_external_active && app->txrx->radio_device) {
        subghz_devices_sleep(app->txrx->radio_device);
        if(app->txrx->radio_external_begun) {
            subghz_devices_end(app->txrx->radio_device);
        }
    }
    app->txrx->radio_external_active = false;
    app->txrx->radio_external_begun = false;
    app->txrx->radio_device = NULL;
    tpms_radio_release_owned_power(app);
}

static bool tpms_radio_use_internal(TPMSApp* app) {
    furi_assert(app);
    if(app->txrx->radio_external_active) tpms_radio_end_external(app);

    const SubGhzDevice* internal =
        subghz_devices_get_by_name(TPMS_DEVICE_CC1101_INT_NAME);
    if(!internal) {
        FURI_LOG_E(TAG, "Internal CC1101 device not found");
        app->txrx->radio_device = NULL;
        return false;
    }

    app->txrx->radio_device = internal;
    app->txrx->radio_external_active = false;
    app->txrx->radio_external_begun = false;
    tpms_radio_release_owned_power(app);
    return true;
}

static bool tpms_radio_try_external(TPMSApp* app) {
    furi_assert(app);

    if(app->txrx->radio_external_active && app->txrx->radio_device) {
        if(subghz_devices_is_connect(app->txrx->radio_device)) return true;
        FURI_LOG_W(TAG, "External CC1101 disconnected");
        tpms_radio_end_external(app);
    }

    if(!tpms_radio_power_external(app)) {
        FURI_LOG_W(TAG, "OTG power unavailable");
        return false;
    }

    const SubGhzDevice* external =
        subghz_devices_get_by_name(TPMS_DEVICE_CC1101_EXT_NAME);
    if(!external || !subghz_devices_is_connect(external)) {
        FURI_LOG_I(TAG, "External CC1101 not detected");
        tpms_radio_release_owned_power(app);
        return false;
    }

    if(!subghz_devices_begin(external)) {
        FURI_LOG_E(TAG, "External CC1101 begin failed");
        tpms_radio_release_owned_power(app);
        return false;
    }

    app->txrx->radio_device = external;
    app->txrx->radio_external_active = true;
    app->txrx->radio_external_begun = true;
    FURI_LOG_I(TAG, "Using external CC1101");
    return true;
}

static bool tpms_radio_prepare_selected(TPMSApp* app) {
    furi_assert(app);
    if(!app->txrx->radio_device) return false;

    /* AUTO/WEW must initialise the internal CC1101 through the same reset
       path as forced WEW. This prevents the old label-only fallback bug. */
    subghz_devices_reset(app->txrx->radio_device);
    subghz_devices_idle(app->txrx->radio_device);
    app->txrx->txrx_state = TPMSTxRxStateIDLE;
    return true;
}

bool tpms_radio_init(TPMSApp* app) {
    furi_assert(app);
    furi_assert(app->txrx);

    subghz_devices_init();
    app->txrx->radio_devices_initialized = true;
    app->txrx->radio_mode = TPMSRadioModeAuto;
    app->txrx->radio_device = NULL;
    app->txrx->radio_external_active = false;
    app->txrx->radio_external_begun = false;
    app->txrx->radio_otg_owned = false;
    app->txrx->radio_auto_probe_done = false;
    return tpms_radio_select(app, TPMSRadioModeAuto);
}

bool tpms_radio_select(TPMSApp* app, TPMSRadioMode mode) {
    furi_assert(app);
    furi_assert(app->txrx);

    if(app->txrx->txrx_state == TPMSTxRxStateRx) tpms_rx_end(app);
    app->txrx->radio_mode = mode;
    app->txrx->radio_auto_probe_done = false;

    bool selected = false;
    if(mode == TPMSRadioModeInternal) {
        selected = tpms_radio_use_internal(app);
    } else {
        selected = tpms_radio_try_external(app);
        app->txrx->radio_auto_probe_done = true;
        if(!selected) {
            /* AUTO/WEW and unavailable forced ZEW use the complete internal
               initialisation path, exactly like manually selected WEW. */
            selected = tpms_radio_use_internal(app);
        }
    }

    return selected && tpms_radio_prepare_selected(app);
}

bool tpms_radio_ensure_selected(TPMSApp* app) {
    furi_assert(app);
    furi_assert(app->txrx);
    if(!app->txrx->radio_devices_initialized) return tpms_radio_init(app);

    if(app->txrx->radio_external_active) {
        if(app->txrx->radio_device && subghz_devices_is_connect(app->txrx->radio_device)) {
            return true;
        }
        FURI_LOG_W(TAG, "External radio lost; selecting safe fallback");
        return tpms_radio_select(app, app->txrx->radio_mode);
    }

    if(app->txrx->radio_device) {
        /* Do not probe the external module again before every RX/TX. When
           AUTO falls back to WEW, the internal radio stays selected until
           the user changes Radio or restarts the application. */
        return true;
    }

    return tpms_radio_select(app, app->txrx->radio_mode);
}

void tpms_radio_deinit(TPMSApp* app) {
    furi_assert(app);
    if(!app->txrx || !app->txrx->radio_devices_initialized) return;

    if(app->txrx->radio_external_active) {
        tpms_radio_end_external(app);
    } else if(app->txrx->radio_device) {
        subghz_devices_sleep(app->txrx->radio_device);
        app->txrx->radio_device = NULL;
    }

    tpms_radio_release_owned_power(app);
    subghz_devices_deinit();
    app->txrx->radio_devices_initialized = false;
}

bool tpms_radio_is_external(const TPMSApp* app) {
    furi_assert(app);
    return app->txrx && app->txrx->radio_external_active;
}

float tpms_radio_get_rssi(const TPMSApp* app) {
    furi_assert(app);
    if(!app->txrx || !app->txrx->radio_device) return -127.0f;
    return subghz_devices_get_rssi(app->txrx->radio_device);
}

void tpms_radio_get_setting_text(const TPMSApp* app, char* text, size_t text_size) {
    furi_assert(app);
    furi_assert(text);
    if(text_size == 0U) return;

    switch(app->txrx->radio_mode) {
    case TPMSRadioModeInternal:
        snprintf(text, text_size, "INT");
        break;
    case TPMSRadioModeExternal:
        snprintf(text, text_size, "%s", tpms_radio_is_external(app) ? "EXT" : "EXT->INT");
        break;
    case TPMSRadioModeAuto:
    default:
        snprintf(text, text_size, "%s", tpms_radio_is_external(app) ? "AUTO/EXT" : "AUTO/INT");
        break;
    }
}
