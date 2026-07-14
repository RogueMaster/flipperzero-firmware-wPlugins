/**
*  ▌  ▞▚ ▛▚ ▌  ▞▚ ▟  Copyright© 2025 LAB401 GPLv3
*  ▌  ▛▜ ▛▚ ▙▙ ▌▐ ▐  This program is free software
*  ▀▀ ▘▝ ▀▘  ▘ ▝▘ ▀▘ See LICENSE.txt - lab401.com
*    + Tixlegeek
*/
#include "401DigiLab_config.h"
#include "401_config.h"
#include <dialogs/dialogs.h>
#include <notification/notification_messages.h>

static const char* TAG = "401_DigiLabConfig";

#define CALIBRATION_STEP_COUNT 3

/**
  Dialog messages for the calibration procedure.
*/
static const CalibrationStep calibration_steps[CALIBRATION_STEP_COUNT] = {
    {"Hey there!\nTime to calibrate the probe\nso that tool is accurate!", "Next"},
    {"Connect the probes, and\nwhen prompted, hold the\nred probe on the \n\"5V\" pad. ", "Start"},
    {"Everything's ready?\n(hold the probe on +5!)", "Calibrate"},
};
// On error.
static const CalibrationStep calibration_steps_error = {
    "There was an error\ncheck your connection and\ntry again :/",
    "Retry"};
// On success.
static const CalibrationStep calibration_steps_success = {
    "Seems legit!\nyour device's now\ncalibrated.",
    "Next"};

/**
* @brief Allocate memory and initialize the app configuration.
*
* @return Returns a pointer to the allocated AppConfig.
*/

AppConfig* app_config_alloc(void* ctx) {
    furi_assert(ctx);
    AppContext* app = (AppContext*)ctx;
    AppConfig* appConfig = malloc(sizeof(AppConfig));
    appConfig->view = view_alloc();
    view_set_context(appConfig->view, app);
    return appConfig;
}

static void config_adc_start(FuriHalAdcHandle** adc) {
    *adc = furi_hal_adc_acquire();
    furi_hal_adc_configure(*adc);
}

static void config_adc_stop(FuriHalAdcHandle** adc) {
    if(*adc) {
        furi_hal_adc_release(*adc);
    }
}

/**
* @brief Free the memory occupied by the app configuration.
*
* @param appConfig The AppConfig to be freed.
*/
void app_config_free(AppConfig* appConfig) {
    furi_assert(appConfig);
    view_free_model(appConfig->view);
    view_free(appConfig->view);

    free(appConfig);
}

/**
* @brief Get the view associated with the app configuration.
*
* @param appConfig The AppConfig for which the view is to be fetched.
* @return Returns a pointer to the View.
*/
View* app_config_get_view(AppConfig* appConfig) {
    furi_assert(appConfig);
    return appConfig->view;
}

/**
* @brief Callback when the app configuration scene is entered.
*
* @param ctx The context passed to the callback.
*/
void app_scene_config_on_enter(void* ctx) {
    AppContext* app = (AppContext*)ctx;
    DialogsApp* dialogs = furi_record_open(RECORD_DIALOGS);

    DialogMessageButton res;
    DialogMessage* msg = dialog_message_alloc();
    bool calibrated = false;
    uint8_t calibration_step = 0;
    bool must_recalibrate = (config_check_calibration() == L401_OK) ? false : true;
    while(calibrated == false) {
        // Display dialogs
        for(uint8_t i = calibration_step; i < CALIBRATION_STEP_COUNT; i++) {
            dialog_message_set_header(msg, "Calibration", 64, 0, AlignCenter, AlignTop);
            dialog_message_set_text(
                msg, calibration_steps[i].text, 63, 32, AlignCenter, AlignCenter);
            dialog_message_set_buttons(msg, "", calibration_steps[i].button, "");
            res = dialog_message_show(dialogs, msg);
            if((res == DialogMessageButtonBack) && (must_recalibrate == false)) {
                dialog_message_free(msg);
                scene_manager_previous_scene(app->scene_manager);
                return;
            }
        }
        // Read ADC Value
        FuriHalAdcHandle* adc;
        config_adc_start(&adc);
        uint16_t raw = furi_hal_adc_read(adc, FuriHalAdcChannel4);
        float voltage = furi_hal_adc_convert_to_voltage(adc, raw);
        // Convert to bridgefactor
        app->data->config->BridgeFactor = (double)(5 / voltage) * 1000;
        config_adc_stop(&adc);

        // Calibration obviously failed
        if((app->data->config->BridgeFactor < (double)DIGILAB_DEFAULT_SCOPE_BRIDGEFACTOR_MIN) ||
           (app->data->config->BridgeFactor > (double)DIGILAB_DEFAULT_SCOPE_BRIDGEFACTOR_MAX)) {
            notification_message(app->notifications, &sequence_error);
            app->data->config->BridgeFactor = DIGILAB_DEFAULT_SCOPE_BRIDGEFACTOR;
            config_reset_calibration();
            dialog_message_set_header(msg, "Calibration", 64, 0, AlignCenter, AlignTop);
            dialog_message_set_text(
                msg, calibration_steps_error.text, 63, 32, AlignCenter, AlignCenter);
            dialog_message_set_buttons(msg, "", calibration_steps_error.button, "");
            res = dialog_message_show(dialogs, msg);
            // Next step is 1
            calibration_step = 1;
            // Still not calibrated
            calibrated = false;
        } else
        // Calibration succeeded
        {
            notification_message(app->notifications, &sequence_success);
            config_set_calibration();
            dialog_message_set_header(msg, "Calibration", 64, 0, AlignCenter, AlignTop);
            dialog_message_set_text(
                msg, calibration_steps_success.text, 63, 32, AlignCenter, AlignCenter);
            dialog_message_set_buttons(msg, "", calibration_steps_success.button, "");
            res = dialog_message_show(dialogs, msg);
            // Device is calibrated
            calibrated = true;
        }
    }

    dialog_message_free(msg);
    scene_manager_next_scene(app->scene_manager, AppSceneSplash);
}

/**
    * @brief Handle scene manager events for the app configuration scene.
    *
    * @param context The context passed to the callback.
    * @param event The scene manager event.
    * @return Returns true if the event was consumed, otherwise false.
    */
bool app_scene_config_on_event(void* ctx, SceneManagerEvent event) {
    UNUSED(ctx);
    UNUSED(event);
    return false;
}

/**
    * @brief Callback when the app configuration scene is exited.
    *
    * @param ctx The context passed to the callback.
    */
void app_scene_config_on_exit(void* ctx) {
    AppContext* app = (AppContext*)ctx;

    furi_record_close(RECORD_DIALOGS);
    l401_err res = config_save_json(DIGILABCONF_CONFIG_FILE, app->data->config);

    if(res == L401_OK) {
        FURI_LOG_I(TAG, "Successfully saved configuration to %s.", DIGILABCONF_CONFIG_FILE);
    } else {
        FURI_LOG_E(TAG, "Error while saving to %s: %d", DIGILABCONF_CONFIG_FILE, res);
    }
}
